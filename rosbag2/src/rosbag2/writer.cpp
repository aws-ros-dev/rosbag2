// Copyright 2018, Bosch Software Innovations GmbH.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "rosbag2/compressor_poc.hpp"
#include "rosbag2/writer.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "rosbag2/info.hpp"
#include "rosbag2/logging.hpp"
#include "rosbag2/storage_options.hpp"
#include "rosbag2_storage/filesystem_helper.hpp"


namespace rosbag2
{

namespace
{
std::string format_storage_uri(const std::string & base_folder, uint64_t storage_count)
{
  // Right now `base_folder_` is always just the folder name for where to install the bagfile.
  // The name of the folder needs to be queried in case Writer is opened with a relative path.
  std::stringstream storage_file_name;
  storage_file_name << rosbag2_storage::FilesystemHelper::get_folder_name(base_folder);

  // Only append the counter after we have split.
  // This is so bagfiles have the old naming convention if splitting is disabled.
  if (storage_count > 0) {
    storage_file_name << "_" << storage_count;
  }

  return rosbag2_storage::FilesystemHelper::concat({base_folder, storage_file_name.str()});
}
}  // namespace

Writer::Writer(
  std::unique_ptr<rosbag2_storage::StorageFactoryInterface> storage_factory,
  std::shared_ptr<SerializationFormatConverterFactoryInterface> converter_factory,
  std::unique_ptr<rosbag2_storage::MetadataIo> metadata_io)
: storage_factory_(std::move(storage_factory)),
  converter_factory_(std::move(converter_factory)),
  storage_(nullptr),
  metadata_io_(std::move(metadata_io)),
  converter_(nullptr),
  max_bagfile_size_(rosbag2_storage::storage_interfaces::MAX_BAGFILE_SIZE_BYTES_NO_SPLIT),
  topics_names_to_info_(),
  metadata_()
{
  compressor_ = std::make_unique<CompressorPoC>();
}

Writer::~Writer()
{
  // TODO(dabonnie) Is the dtor really a good place to finalize all this?
  auto const current_uri = storage_->get_relative_path();
  if (compression_options_.mode == CompressionMode::FILE) {
    compress_file(current_uri);
  } else {
    metadata_.relative_file_paths.push_back(current_uri);
  }

  if (!base_folder_.empty()) {
    finalize_metadata();
    metadata_io_->write_metadata(base_folder_, metadata_);
  }

  storage_.reset();  // Necessary to ensure that the storage is destroyed before the factory
  storage_factory_.reset();
}

void Writer::init_metadata()
{
  metadata_ = rosbag2_storage::BagMetadata{};
  metadata_.storage_identifier = storage_->get_storage_identifier();
  metadata_.starting_time = std::chrono::time_point<std::chrono::high_resolution_clock>(
    std::chrono::nanoseconds::max());
  // TODO(dabonnie) fixme when compressing this isn't valid
  // This is already taken care of in dtor tho?
  // metadata_.relative_file_paths = {storage_->get_relative_path()};
}

void Writer::open(
  const StorageOptions & storage_options,
  const ConverterOptions & converter_options,
  const CompressionOptions & compression_options)
{
  compression_options_ = compression_options;
  std::cout << "Compression Mode is: " << compression_options.mode << std::endl;
  open(storage_options, converter_options);
}

void Writer::open(
  const StorageOptions & storage_options,
  const ConverterOptions & converter_options)
{
  max_bagfile_size_ = storage_options.max_bagfile_size;
  base_folder_ = storage_options.uri;

  if (converter_options.output_serialization_format !=
    converter_options.input_serialization_format)
  {
    converter_ = std::make_unique<Converter>(converter_options, converter_factory_);
  }

  const auto storage_uri = format_storage_uri(base_folder_, 0);

  storage_ = storage_factory_->open_read_write(storage_uri, storage_options.storage_id);
  if (!storage_) {
    throw std::runtime_error("No storage could be initialized. Abort");
  }

  init_metadata();
}

void Writer::create_topic(const TopicMetadata & topic_with_type)
{
  if (!storage_) {
    throw std::runtime_error("Bag is not open. Call open() before writing.");
  }

  if (converter_) {
    converter_->add_topic(topic_with_type.name, topic_with_type.type);
  }

  if (topics_names_to_info_.find(topic_with_type.name) ==
    topics_names_to_info_.end())
  {
    rosbag2_storage::TopicInformation info{};
    info.topic_metadata = topic_with_type;

    const auto insert_res = topics_names_to_info_.insert(
      std::make_pair(topic_with_type.name, info));

    if (!insert_res.second) {
      std::stringstream errmsg;
      errmsg << "Failed to insert topic \"" << topic_with_type.name << "\"!";

      throw std::runtime_error(errmsg.str());
    }

    storage_->create_topic(topic_with_type);
  }
}

void Writer::remove_topic(const TopicMetadata & topic_with_type)
{
  if (!storage_) {
    throw std::runtime_error("Bag is not open. Call open() before removing.");
  }

  if (topics_names_to_info_.erase(topic_with_type.name) > 0) {
    storage_->remove_topic(topic_with_type);
  } else {
    std::stringstream errmsg;
    errmsg << "Failed to remove the non-existing topic \"" <<
      topic_with_type.name << "\"!";

    throw std::runtime_error(errmsg.str());
  }
}

void Writer::split_bagfile()
{
  ROSBAG2_LOG_INFO("Splitting bag file");
  const auto current_uri = storage_->get_relative_path();
  const auto storage_uri_rollover = format_storage_uri(
    base_folder_,
    metadata_.relative_file_paths.size());
  storage_ = storage_factory_->open_read_write(storage_uri_rollover, metadata_.storage_identifier);

  if (!storage_) {
    std::stringstream errmsg;
    errmsg << "Failed to rollover bagfile to new file: \"" << storage_uri_rollover << "\"!";
    throw std::runtime_error(errmsg.str());
  }

  // Re-register all Topics since we rolled-over to a new bagfile.
  for (const auto & topic : topics_names_to_info_) {
    storage_->create_topic(topic.second.topic_metadata);
  }

  if (compression_options_.mode == CompressionMode::FILE) {
    compress_file(current_uri);
  } else {
    metadata_.relative_file_paths.push_back(current_uri);
  }
}

void Writer::compress_file(std::string uri_to_compress)
{

  // TODO(dabonnie) start this in a new thread? most likely don't block....
  std::cout << "COMPRESSING FILE" << std::endl;
  auto start = std::chrono::high_resolution_clock::now();
  auto compressed_uri = compressor_->compress_uri(uri_to_compress);
  auto end = std::chrono::high_resolution_clock::now();

  // TODO(dabonnie) wish there was an https://en.wikipedia.org/wiki/ISO_8601#Durations format
  // https://github.com/HowardHinnant/date
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  std::cout << "Compression took " << duration.count() << " milliseconds" << std::endl;

  // TODO(dabonnie) what happens if compression fails for a single file?
  metadata_.relative_file_paths.push_back(compressed_uri);

  // delete file
  std::cout << "Deleting original bagfile " << uri_to_compress << std::endl;
  std::remove(uri_to_compress.c_str());
}

void Writer::write(std::shared_ptr<SerializedBagMessage> message)
{
  if (!storage_) {
    throw std::runtime_error("Bag is not open. Call open() before writing.");
  }

  // Update the message count for the Topic.
  ++topics_names_to_info_.at(message->topic_name).message_count;

  if (should_split_bagfile()) {
    split_bagfile();
  }

  const auto message_timestamp = std::chrono::time_point<std::chrono::high_resolution_clock>(
    std::chrono::nanoseconds(message->time_stamp));
  metadata_.starting_time = std::min(metadata_.starting_time, message_timestamp);

  const auto duration = message_timestamp - metadata_.starting_time;
  metadata_.duration = std::max(metadata_.duration, duration);

  // TODO(dabonnie) compress a single message
  // if we need to compress a single message, then compress here
  // message has a shared pointer to serailzied data, compress that then pass)
  auto converted_message = converter_ ? converter_->convert(message) : message;
  if (compression_options_.mode == CompressionMode::MESSAGE) {
    auto compressed_message = compressor_->compress_bag_message_data(converted_message);
    storage_->write(compressed_message);
  } else {
    storage_->write(converted_message);
  }
}
// TODO(dabonnie) fixme
bool Writer::should_split_bagfile() const
{
//  if (max_bagfile_size_ == rosbag2_storage::storage_interfaces::MAX_BAGFILE_SIZE_NO_SPLIT) {
//    return false;
//  } else {
//    return storage_->get_bagfile_size() > max_bagfile_size_;
//  }

  // TODO(dabonnie) hardcoded for PoC, command line split size (-b) was not in this branch
  return storage_->get_bagfile_size() > 1024 * 100;
}

void Writer::finalize_metadata()
{
  metadata_.bag_size = 0;

  for (const auto & path : metadata_.relative_file_paths) {
    metadata_.bag_size += rosbag2_storage::FilesystemHelper::get_file_size(path);
  }

  metadata_.topics_with_message_count.clear();
  metadata_.topics_with_message_count.reserve(topics_names_to_info_.size());
  metadata_.message_count = 0;

  for (const auto & topic : topics_names_to_info_) {
    metadata_.topics_with_message_count.push_back(topic.second);
    metadata_.message_count += topic.second.message_count;
  }
  // TODO(dabonnie) mark if compression is inactive (sane, defined default - null / empty string?)
  //  vs provided via the CLI
  metadata_.compression_format = compressor_->get_compression_identifier();
  metadata_.compression_mode = CompressionModeToStringMap.at(compression_options_.mode);
}

}  // namespace rosbag2
