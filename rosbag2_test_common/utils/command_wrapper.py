import argparse
import logging
import signal
import subprocess
import sys
from threading import Thread
import time

try:
	import win32api
	import win32file
	import win32pipe
except ImportError:
	logging.info('This module can only be used on windows')
	sys.exit(1)

PIPE_NAME = r'\\.\pipe\rosbag2_tests'
CTRL_C_PIPE_TRIGGER = 'CTRL_C_EVENT'
CLIENT_CLOSED_TRIGGER = 'CLOSE_PIPE_EVENT'

logging.basicConfig(level=logging.INFO)


class CommandWrapper:
	def __init__(self, command):
		self._proc = None
		self._command = command
		# Used to avoid sending ctrl-c twice if a ctrl-break is received
		self._process_is_running = False
		self._process_thread = Thread(target=self.execute)
		signal.signal(signal.SIGBREAK, self._break_signal_handler)
		# Setup pipe for IPC
		self._pipe_handle = None
		self._setup_pipe_thread = Thread(target=self._setup_pipe, daemon=True)
		self._setup_pipe_thread.start()
		self._pipe_listener_thread = Thread(target=self._pipe_listener)

	def _cleanup(self):
		if self._pipe_handle:
			win32file.CloseHandle(self._pipe_handle)

	def _pipe_listener(self):
		logging.info('Listening on pipe...')
		while True:
			resp = win32file.ReadFile(self._pipe_handle, 64*1024)
			if resp == CTRL_C_PIPE_TRIGGER:
				self.send_ctrl_c_event()
			elif resp == CLIENT_CLOSED_TRIGGER:
				break
			logging.info(f"message: {resp}")
			time.sleep(0.05)
		logging.info("Closing pipe")
		win32file.CloseHandle(self._pipe_handle)

	def _setup_pipe(self):
		logging.info("Creating named pipe")
		self._pipe_handle = win32pipe.CreateNamedPipe(
			PIPE_NAME,
			win32pipe.PIPE_ACCESS_INBOUND,
			win32pipe.PIPE_TYPE_MESSAGE | win32pipe.PIPE_READMODE_MESSAGE | win32pipe.PIPE_NOWAIT,
			1, 65536, 65536,
			300,
			None)
		logging.info("Waiting for client...")
		win32pipe.WaitNamedPipe(PIPE_NAME, 10000)
		win32pipe.ConnectNamedPipe(self._pipe_handle, None)
		logging.info("Client connected")
		self._pipe_listener_thread.start()

	def _break_signal_handler(self, signum, frame):
		logging.info("Ctrl-break event received. Sending ctrl-c instead.")
		try:
			self.send_ctrl_c_event()
		except KeyboardInterrupt:
			logging.info("Ignoring ctrl-break")

	def send_ctrl_c_event(self):
		if self._process_is_running:
			try:
				logging.info("Sending ctrl-c")
				self._proc.send_signal(signal.CTRL_C_EVENT)
				logging.info("Waiting for process to terminate")
				self._proc.wait()
			except KeyboardInterrupt:
				logging.info("Ignoring ctrl-c")
				self._cleanup()
		self._process_is_running = False

	def execute(self, timeout_in_sec=3):
		logging.info("Executing: \"{}\"".format(self._command))
		self._proc = subprocess.Popen(self._command, stdin=subprocess.PIPE)
		self._process_is_running = True
		# Remove
		time.sleep(timeout_in_sec)
		self.send_ctrl_c_event()


if __name__ == '__main__':
	parser = argparse.ArgumentParser()
	parser.add_argument(
		'-c', '--command', type=str	
	)
	args = parser.parse_args()

	cw = CommandWrapper(args.command)
	try:
		cw.execute()
	except KeyboardInterrupt:
		logging.info("Ignoring ctrl-c main")

	logging.info("Execution complete")
	while True:
		time.sleep(1)
