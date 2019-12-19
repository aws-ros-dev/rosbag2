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

#include "rosbag2/writers/sequential_writer.hpp"

#include <algorithm>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "rosbag2_compression/zstd_compressor.hpp"

#include "rosbag2/info.hpp"
#include "rosbag2/storage_options.hpp"
#include "rosbag2_storage/filesystem_helper.hpp"

namespace rosbag2
{
namespace writers
{

namespace
{
std::string format_storage_uri(const std::string & base_folder, uint64_t storage_count)
{
  // Right now `base_folder_` is always just the folder name for where to install the bagfile.
  // The name of the folder needs to be queried in case
  // SequentialWriter is opened with a relative path.
  std::stringstream storage_file_name;
  storage_file_name << rosbag2_storage::FilesystemHelper::get_folder_name(base_folder) <<
    "_" << storage_count;

  return rosbag2_storage::FilesystemHelper::concat({base_folder, storage_file_name.str()});
}
}  // namespace

SequentialWriter::SequentialWriter(
  std::unique_ptr<rosbag2_storage::StorageFactoryInterface> storage_factory,
  std::shared_ptr<SerializationFormatConverterFactoryInterface> converter_factory,
  std::unique_ptr<rosbag2_storage::MetadataIo> metadata_io)
: storage_factory_(std::move(storage_factory)),
  converter_factory_(std::move(converter_factory)),
  storage_(nullptr),
  metadata_io_(std::move(metadata_io)),
  converter_(nullptr),
  max_bagfile_size_(rosbag2_storage::storage_interfaces::MAX_BAGFILE_SIZE_NO_SPLIT),
  topics_names_to_info_(),
  metadata_(),
  compression_mode_{CompressionMode::NONE}
{}

SequentialWriter::~SequentialWriter()
{
  reset();
}

void SequentialWriter::init_metadata()
{
  metadata_ = rosbag2_storage::BagMetadata{};
  metadata_.storage_identifier = storage_->get_storage_identifier();
  metadata_.starting_time = std::chrono::time_point<std::chrono::high_resolution_clock>(
    std::chrono::nanoseconds::max());
  metadata_.relative_file_paths = {storage_->get_relative_file_path()};
  metadata_.compression_mode = compression_mode_to_string(compression_mode_);
}

void SequentialWriter::open(
  const StorageOptions & storage_options,
  const ConverterOptions & converter_options,
  const CompressionOptions & compression_options)
{
  compression_mode_ = compression_options.compression_mode;
  max_bagfile_size_ = storage_options.max_bagfile_size;
  base_folder_ = storage_options.uri;

  if (converter_options.output_serialization_format !=
    converter_options.input_serialization_format)
  {
    converter_ = std::make_unique<Converter>(converter_options, converter_factory_);
  }

  // TODO(zmichaels11): replace this with a "compressor_factory" that can select ZstdCompressor.
  if (compression_options.compression_format == "zstd") {
    compressor_ = std::make_unique<rosbag2_compression::ZstdCompressor>();
  }

  const auto storage_uri = format_storage_uri(base_folder_, 0);

  storage_ = storage_factory_->open_read_write(storage_uri, storage_options.storage_id);
  if (!storage_) {
    throw std::runtime_error("No storage could be initialized. Abort");
  }

  init_metadata();
}

void SequentialWriter::reset()
{
  if (!base_folder_.empty()) {
    finalize_metadata();
    metadata_io_->write_metadata(base_folder_, metadata_);
  }

  if (compressor_ && compression_mode_ == CompressionMode::FILE) {
    // Get the uri of the last rosbag segment and pop it off
    const auto uncompressed_uri = metadata_.relative_file_paths.back();
    metadata_.relative_file_paths.pop_back();

    // Compress the uri of the last rosbag segment and push it on
    const auto compressed_uri = compressor_->compress_uri(uncompressed_uri);
    metadata_.relative_file_paths.push_back(compressed_uri);
  }

  compressor_.reset();
  storage_.reset();  // Necessary to ensure that the storage is destroyed before the factory
  storage_factory_.reset();
}

void SequentialWriter::create_topic(const TopicMetadata & topic_with_type)
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

void SequentialWriter::remove_topic(const TopicMetadata & topic_with_type)
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

void SequentialWriter::split_bagfile()
{
  const auto storage_uri = format_storage_uri(
    base_folder_,
    metadata_.relative_file_paths.size());

  const auto old_storage_uri = storage_->get_relative_file_path();

  storage_ = storage_factory_->open_read_write(storage_uri, metadata_.storage_identifier);

  if (!storage_) {
    std::stringstream errmsg;
    errmsg << "Failed to rollover bagfile to new file: \"" << storage_uri << "\"!";

    throw std::runtime_error(errmsg.str());
  }

  if (compressor_ && compression_mode_ == CompressionMode::FILE) {
    // Get the uri of the last rosbag segment and pop the uri
    const auto uncompressed_uri = metadata_.relative_file_paths.back();
    metadata_.relative_file_paths.pop_back();

    // Compress the last rosbag and push the new uri
    const auto compressed_uri = compressor_->compress_uri(uncompressed_uri);
    metadata_.relative_file_paths.push_back(compressed_uri);
  }

  // Push the uri for the new rosbag segment
  metadata_.relative_file_paths.push_back(storage_->get_relative_file_path());

  // Re-register all topics since we rolled-over to a new bagfile.
  for (const auto & topic : topics_names_to_info_) {
    storage_->create_topic(topic.second.topic_metadata);
  }
}

void SequentialWriter::write(std::shared_ptr<SerializedBagMessage> message)
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

  if (compressor_ && compression_mode_ == CompressionMode::MESSAGE) {
    auto converted_message = converter_ ? converter_->convert(message) : message;

    compressor_->compress_serialized_bag_message(converted_message.get());
  }

  storage_->write(converter_ ? converter_->convert(message) : message);
}

bool SequentialWriter::should_split_bagfile() const
{
  if (max_bagfile_size_ == rosbag2_storage::storage_interfaces::MAX_BAGFILE_SIZE_NO_SPLIT) {
    return false;
  } else {
    return storage_->get_bagfile_size() > max_bagfile_size_;
  }
}

void SequentialWriter::finalize_metadata()
{
  if (compressor_) {
    metadata_.compression_format = compressor_->get_compression_identifier();
  }

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
}

}  // namespace writers
}  // namespace rosbag2
