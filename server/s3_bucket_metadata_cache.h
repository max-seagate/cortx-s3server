/*
 * Copyright (c) 2020 Seagate Technology LLC and/or its Affiliates
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For any questions about this software or licensing,
 * please email opensource@seagate.com or cortx-questions@seagate.com.
 *
 */

#pragma once

#include "s3_bucket_metadata.h"

#include <functional>
#include <map>
#include <memory>
#include <string>

// Forward declarations
class MotrAPI;
class S3GlobalBucketIndexMetadataFactory;
class S3MotrKVSReaderFactory;
class S3MotrKVSWriterFactory;
class S3RequestObject;

class S3BucketMetadataCache {
 public:
  S3BucketMetadataCache(
      unsigned max_cache_size, unsigned expire_interval_sec,
      unsigned refresh_interval_sec, std::shared_ptr<MotrAPI> = {},
      std::shared_ptr<S3MotrKVSReaderFactory> = {},
      std::shared_ptr<S3MotrKVSWriterFactory> = {},
      std::shared_ptr<S3GlobalBucketIndexMetadataFactory> = {});

  S3BucketMetadataCache(const S3BucketMetadataCache&) = delete;
  S3BucketMetadataCache& operator=(const S3BucketMetadataCache&) = delete;

  virtual ~S3BucketMetadataCache();

  using FetchHanderType = std::function<void(const S3BucketMetadata&)>;
  using StateHandlerType = std::function<void(S3BucketMetadataState)>;

  virtual void fetch(const S3BucketMetadata& dst, FetchHanderType callback);
  virtual void save(const S3BucketMetadata& src, StateHandlerType callback);
  virtual void update(const S3BucketMetadata& src, StateHandlerType callback);
  virtual void remove(const S3BucketMetadata& src, StateHandlerType callback);

  const unsigned max_cache_size, expire_interval_sec, refresh_interval_sec;

 private:
  std::shared_ptr<MotrAPI> s3_motr_api;
  std::shared_ptr<S3MotrKVSReaderFactory> motr_kvs_reader_factory;
  std::shared_ptr<S3MotrKVSWriterFactory> motr_kvs_writer_factory;
  std::shared_ptr<S3GlobalBucketIndexMetadataFactory>
      global_bucket_index_metadata_factory;

  void on_remove(StateHandlerType, S3BucketMetadataState, std::string);

  class Item;
  Item* get_item(const S3BucketMetadata& src);

  std::map<std::string, std::unique_ptr<Item> > items;
};
