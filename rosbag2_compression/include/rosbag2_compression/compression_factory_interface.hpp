// Copyright 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
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

#ifndef ROSBAG2_COMPRESSION__COMPRESSION_FACTORY_INTERFACE_HPP_
#define ROSBAG2_COMPRESSION__COMPRESSION_FACTORY_INTERFACE_HPP_

#include <memory>
#include <string>

#include "rosbag2_compression/base_compressor_interface.hpp"
#include "rosbag2_compression/base_decompressor_interface.hpp"
#include "rosbag2_compression/visibility_control.hpp"

#ifdef _WIN32
# pragma warning(push)
# pragma warning(disable:4251)
#endif

namespace rosbag2_compression
{

class ROSBAG2_COMPRESSION_PUBLIC CompressionFactoryInterface
{
public:
  virtual ~CompressionFactoryInterface() = default;

  virtual std::shared_ptr<BaseCompressorInterface>
  create_compressor(const std::string & compression_format) = 0;

  virtual std::shared_ptr<BaseDecompressorInterface>
  create_decompressor(const std::string & decompression_format) = 0;
};

}  // namespace rosbag2_compression

#endif // ROSBAG2_COMPRESSION_COMPRESSION_FACTORY_INTERFACE_HPP_
