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

#ifndef ROSBAG2_COMPRESSION__COMPRESSION_FACTORY_IMPL_HPP_
#define ROSBAG2_COMPRESSION__COMPRESSION_FACTORY_IMPL_HPP_

#include <algorithm>
#include <memory>
#include <string>

#include "pluginlib/class_loader.hpp"

#include "rosbag2_compression/base_compressor_interface.hpp"
#include "rosbag2_compression/base_decompressor_interface.hpp"
#include "logging.hpp"

namespace rosbag2_compression
{

constexpr const char kCompressorInterfaceName[] = "rosbag2_compression::BaseCompressorInterface";
constexpr const char kDecompressorInterfaceName[] = "rosbag2_compression::BaseDecompressorInterface";
constexpr const char kCompressionProjectName[] = "rosbag2_compression";
constexpr const char kCompressorSuffix[] = "_compressor";

namespace
{
bool is_plugin_not_registered(
  const std::string & compression_format,
  const std::vector<std::string> & registered_compressor_classes,
  const std::vector<std::string> & registered_interface_classes)
{
  auto class_exists_in_compression = std::find(
    registered_compressor_classes.begin(), registered_compressor_classes.end(),
    compression_format);
  auto class_exists_in_deserializers = std::find(
    registered_interface_classes.begin(), registered_interface_classes.end(),
    compression_format);
  return class_exists_in_compression == registered_compressor_classes.end() &&
         class_exists_in_deserializers == registered_interface_classes.end();
}
}

class CompressionFactoryImpl
{
public:
  CompressionFactoryImpl()
  {
    try {
      compressor_class_loader_ =
        std::make_shared<pluginlib::ClassLoader<BaseCompressorInterface>>(
          kCompressionProjectName, kCompressorInterfaceName);
    } catch (const std::exception & e) {
      ROSBAG2_COMPRESSION_LOG_ERROR_STREAM("Unable to create compressor class loader instance: " << e.what());
      throw e;
    }

    try {
      decompressor_class_loader_ =
        std::make_shared<pluginlib::ClassLoader<BaseDecompressorInterface>>(
          kCompressionProjectName, kDecompressorInterfaceName);
    } catch (const std::exception & e) {
      ROSBAG2_COMPRESSION_LOG_ERROR_STREAM("Unable to create decompressor class loader instance: " << e.what());
      throw e;
    }
  }
  ~CompressionFactoryImpl() = default;

  std::shared_ptr<BaseCompressorInterface>
  create_compressor(const std::string & compression_format)
  {
    return load_compressor_interface(compression_format, compressor_class_loader_);
  }

  std::shared_ptr<BaseDecompressorInterface>
  create_decompressor(const std::string & decompression_format)
  {
    return load_decompressor_interface(decompression_format, decompressor_class_loader_);
  }

private:
  template<typename ICompressionFormat>
  std::unique_ptr<ICompressionFormat> load_compressor_interface(
    const std::string & compression_format,
    std::shared_ptr<pluginlib::ClassLoader<ICompressionFormat>> class_loader)
  {
    auto compressor_id = compression_format + kCompressorSuffix;

    if (is_plugin_not_registered(
        compressor_id,
        compressor_class_loader_->getDeclaredClasses(),
        class_loader->getDeclaredClasses()))
    {
      ROSBAG2_COMPRESSION_LOG_ERROR_STREAM(
        "Requested compressor for format '" << compression_format << "' does not exist");
      return nullptr;
    }

    try {
      return std::unique_ptr<ICompressionFormat>(
        class_loader->createUnmanagedInstance(compressor_id));
    } catch (const std::runtime_error & ex) {
      ROSBAG2_COMPRESSION_LOG_ERROR_STREAM(
        "Unable to load instance of compressor interface: " << ex.what());
      return nullptr;
    }
  }

  template<typename ICompressionFormat>
  std::unique_ptr<ICompressionFormat> load_decompressor_interface(
    const std::string & compression_format,
    std::shared_ptr<pluginlib::ClassLoader<ICompressionFormat>> class_loader)
  {
    auto compressor_id = compression_format + kCompressorSuffix;

    if (is_plugin_not_registered(
      compressor_id,
      decompressor_class_loader_->getDeclaredClasses(),
      class_loader->getDeclaredClasses()))
    {
      ROSBAG2_COMPRESSION_LOG_ERROR_STREAM(
        "Requested decompressor for format '" << compression_format << "' does not exist");
      return nullptr;
    }

    try {
      return std::unique_ptr<ICompressionFormat>(
        class_loader->createUnmanagedInstance(compressor_id));
    } catch (const std::runtime_error & ex) {
      ROSBAG2_COMPRESSION_LOG_ERROR_STREAM(
        "Unable to load instance of decompressor interface: " << ex.what());
      return nullptr;
    }
  }

  std::shared_ptr<pluginlib::ClassLoader<BaseCompressorInterface>> compressor_class_loader_;
  std::shared_ptr<pluginlib::ClassLoader<BaseDecompressorInterface>> decompressor_class_loader_;
};
}  // namespace rosbag2_compression

#endif // ROSBAG2_COMPRESSION__COMPRESSION_FACTORY_IMPL_HPP_
