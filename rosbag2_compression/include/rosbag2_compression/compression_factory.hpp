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

#ifndef ROSBAG2_CPP__COMPRESSION_FACTORY_HPP_
#define ROSBAG2_CPP__COMPRESSION_FACTORY_HPP_

#include <memory>
#include <string>

#include "rosbag2_compression/base_compressor_interface.hpp"
#include "rosbag2_compression/base_decompressor_interface.hpp"
#include "rosbag2_compression/compression_factory_interface.hpp"
#include "rosbag2_compression/visibility_control.hpp"

namespace rosbag2_compression
{

class CompressionFactoryImpl;

class ROSBAG2_COMPRESSION_PUBLIC CompressionFactory : public CompressionFactoryInterface
{
public:
  CompressionFactory();
  ~CompressionFactory() override;

  std::shared_ptr<BaseCompressorInterface>
  create_compressor(const std::string & compression_format) override;

  std::shared_ptr<BaseDecompressorInterface>
  create_decompressor(const std::string & decompression_format) override;

private:
  std::unique_ptr<CompressionFactoryImpl> impl_;
};
}  // namespace rosbag2_compression

#endif // ROSBAG2_CPP__COMPRESSION_FACTORY_HPP_
