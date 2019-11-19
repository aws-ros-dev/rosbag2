// Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
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

#ifndef ROSBAG2__COMPRESSION_INTERFACES__BASE_DECOMPRESSOR_INTERFACE_HPP_
#define ROSBAG2__COMPRESSION_INTERFACES__BASE_DECOMPRESSOR_INTERFACE_HPP_

#include <memory>
#include <string>

#include "rosbag2/visibility_control.hpp"

/**
 * An interface for developers adding a new decompression algorithm to rosbag2. These functions
 * must be implemented to that a reader can properly decompress a file or bag message.
 * A corresponding compressor must also be implemented.
 *
 * Example file decompression usage:
 *
 * MyDecompressor my_decompressor();
 * std::string current_uri = get_current_file();
 * std::string compressed_path_uri = my_decompressor.decompress_uri(current_uri);
 * storage = storage_factory.open_read_only(compressed_path_uri, storage_options.storage_id);
 *
 * Example message decompression usage:
 *
 * MyDecompressor my_decompressor();
 * std::shared_ptr<SerializedBagMessage> bag_message = storage.read_next();
 * std::shared_ptr<SerializedBagMessage> decompressed_message =
 *   my_decompressor.decompress_serialized_bag_message(bag_message);
 */
namespace rosbag2
{

class ROSBAG2_PUBLIC BaseDecompressorInterface
{
public:
  virtual ~BaseDecompressorInterface() {}

  /**
   * Decompress a file on disk.
   *
   * \param uri Input file to decompress with file extension.
   * \return The relative path to the decompressed file without the compressed extension.
   */
  virtual std::string decompress_uri(const std::string & uri) = 0;

  /**
   * Decompress serialized_data in a serialized bag message.
   *
   * \param bag_message A serialized bag message.
   * \return A shared pointer to the bag message with decompressed serialized_data.
   */
  virtual std::shared_ptr<SerializedBagMessage> decompress_serialized_bag_message(
    std::shared_ptr<SerializedBagMessage> bag_message) = 0;

  /**
   * Get the identifier of the compression algorithm. This is appended to the relative file path.
   */
  virtual std::string get_decompression_identifier() const = 0;
};

}  // namespace rosbag2

#endif  // ROSBAG2__COMPRESSION_INTERFACES__BASE_DECOMPRESSOR_INTERFACE_HPP_
