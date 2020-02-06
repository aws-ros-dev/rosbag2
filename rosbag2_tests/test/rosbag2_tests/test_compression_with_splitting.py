import ctypes
import logging
import os
import signal
import socket
import sqlite3
import subprocess
import sys
import time
import yaml

from ament_index_python.packages import get_package_share_directory
import rclpy
from test_msgs.msg import Strings

logging.basicConfig(level=logging.INFO)


def signal_handler(signum, frame):
    logging.info('Ignoring signal {}'.format(signum))


def wait_for_db(database_path, timeout_in_sec=10):
    uri = database_path.as_uri() + '?mode=ro'
    end_time = time.time() + timeout_in_sec
    while time.time() < end_time:
        try:
            if database_path.exists():
                db = sqlite3.connect(uri, uri=True)
                return db
        except sqlite3.Error as e:
            logging.debug('Caught error {}\nRetrying...'.format(e))
        time.sleep(0.05)
    logging.info('Wait for database timed out')


def create_string_message(message_str, message_size):
    # Python pre-allocates a certain amount of memory for a string (30-50 bytes) which is
    # system dependent. We subtract that amount to get the size of the actual string
    string_size = sys.getsizeof(message_str) - sys.getsizeof('')
    iterations = int(message_size / string_size)
    new_str = message_str
    for _ in range(iterations):
        new_str += message_str

    string_message = Strings()
    string_message.string_value = new_str
    return string_message


def run_publisher(topic_name, message, publish_rate_in_sec, message_count):
    node = rclpy.create_node('scoped_publisher', start_parameter_services=False)
    publisher = node.create_publisher(Strings, topic_name, 10)
    current_count = 0
    while rclpy.ok() and current_count < message_count:
        publisher.publish(message)
        current_count += 1
        time.sleep(publish_rate_in_sec)
    node.destroy_node()


def wait_for_metadata(metadata_path, timeout_in_sec=10):
    end = time.time() + timeout_in_sec
    while time.time() < end:
        if metadata_path.exists():
            return
        time.sleep(0.05)
    logging.info('Wait for metadata timed out')


def test_compression_with_splitting(tmp_path):
    signal.signal(signal.SIGINT, signal_handler)

    topic_name = "/test_topic"
    bagfile_split_size = 4 * 1024 * 1024  # 4MB

    rclpy.init(args=None)

    bagfile_name = 'compression_split'
    bagfile_path = tmp_path / bagfile_name
    database_name = '{}_0.db3'.format(bagfile_name)
    database_path = bagfile_path / database_name

    command = "ros2 bag record " \
              "--max-bag-size {} " \
              "--compression-mode file " \
              "--compression-format zstd " \
              "--output {} {}".format(bagfile_split_size, bagfile_path, topic_name)
    logging.info('Executing: {}'.format(command))
    command = command.split()
    process_handle = subprocess.Popen(command)

    logging.info('Waiting for database...')
    wait_for_db(database_path)

    expected_splits = 4
    message_str = "Test"
    message_size = 1024 * 1024  # 1 MB
    message_count = bagfile_split_size * expected_splits / message_size
    string_message = create_string_message(message_str, message_size)
    logging.info('Creating publisher...')
    run_publisher(topic_name, string_message, 0.05, message_count)

    logging.info('Stopping execution')

    try:
        if sys.platform == 'win32':
            process_handle.send_signal(signal.CTRL_C_EVENT)
        else:
            process_handle.send_signal(signal.SIGINT)
        process_handle.wait()
    except KeyboardInterrupt:
        logging.info('Test ignored ctrl-c')

    metadata_path = bagfile_path / 'metadata.yaml'
    wait_for_metadata(metadata_path)
    logging.info('Checking {}'.format(metadata_path))
    with open(str(metadata_path), 'r') as metadata_file:
        data = yaml.load(metadata_file, Loader=yaml.SafeLoader)
        relative_file_paths = data['rosbag2_bagfile_information']['relative_file_paths']
        for relative_path in relative_file_paths:
            complete_path = tmp_path / relative_path
            assert complete_path.exists() is True
            assert complete_path.suffix == '.zstd'
