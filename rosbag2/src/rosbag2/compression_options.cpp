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

#include "rosbag2/compression_options.hpp"

#include <string>

#include "rosbag2/logging.hpp"

namespace rosbag2
{

namespace
{

constexpr const char COMPRESSION_MODE_NONE_STR[] = "NONE";
constexpr const char COMPRESSION_MODE_FILE_STR[] = "FILE";
constexpr const char COMPRESSION_MODE_MESSAGE_STR[] = "MESSAGE";

}  // namespace

CompressionMode compression_mode_from_string(const std::string & compression_mode)
{
  if (compression_mode.empty() || compression_mode == COMPRESSION_MODE_NONE_STR) {
    return CompressionMode::NONE;
  } else if (compression_mode == COMPRESSION_MODE_FILE_STR) {
    return CompressionMode::FILE;
  } else if (compression_mode == COMPRESSION_MODE_MESSAGE_STR) {
    return CompressionMode::MESSAGE;
  } else {
    ROSBAG2_LOG_ERROR_STREAM("CompressionMode: \"" << compression_mode <<
      "\" is not supported!");
    return CompressionMode::NONE;
  }
}

std::string compression_mode_to_string(const CompressionMode compression_mode)
{
  switch (compression_mode) {
    case CompressionMode::NONE:
      return COMPRESSION_MODE_NONE_STR;
    case CompressionMode::FILE:
      return COMPRESSION_MODE_FILE_STR;
    case CompressionMode::MESSAGE:
      return COMPRESSION_MODE_MESSAGE_STR;
    default:
      ROSBAG2_LOG_ERROR_STREAM("CompressionMode not supported!");
      return COMPRESSION_MODE_NONE_STR;
  }
}

}  // namespace rosbag2
