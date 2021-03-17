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

#include <cassert>
#include <queue>

#define S3_BUCKET_METADATA_CACHE_DEFINITION
#define S3_BUCKET_METADATA_V1_DEFINITION

#include "s3_bucket_metadata_cache.h"
#include "s3_bucket_metadata_v1.h"
#include "s3_factory.h"
#include "s3_log.h"
#include "s3_request_object.h"

class S3BucketMetadataCache::Item {

  enum class CurrentOp {
    none,
    fetching,
    saving,
    deleting
  };
  using RemoveHandlerType =
      std::function<void(S3BucketMetadataState, std::string)>;

 public:
  Item(const S3BucketMetadata& src, std::shared_ptr<MotrAPI>,
       std::shared_ptr<S3MotrKVSReaderFactory>,
       std::shared_ptr<S3MotrKVSWriterFactory>,
       std::shared_ptr<S3GlobalBucketIndexMetadataFactory>);

  virtual ~Item();

  virtual void fetch(FetchHanderType callback);

  virtual void save(const S3BucketMetadata& src, StateHandlerType callback);
  virtual void update(const S3BucketMetadata& src, StateHandlerType callback);
  virtual void remove(RemoveHandlerType callback);

 private:
  void on_done(S3BucketMetadataState state);

  CurrentOp current_op = CurrentOp::none;
  // For save, update and remove operations
  StateHandlerType on_changed;
  RemoveHandlerType on_removed;
  // For fetch operation
  std::queue<FetchHanderType> fetch_waiters;

  std::unique_ptr<S3BucketMetadataV1> p_engine;
};

S3BucketMetadataCache::Item::Item(
    const S3BucketMetadata& src, std::shared_ptr<MotrAPI> s3_motr_api,
    std::shared_ptr<S3MotrKVSReaderFactory> kvs_reader_factory,
    std::shared_ptr<S3MotrKVSWriterFactory> kvs_writer_factory,
    std::shared_ptr<S3GlobalBucketIndexMetadataFactory>
        global_bucket_index_metadata_factory)
    : p_engine(new S3BucketMetadataV1(
          src, std::move(s3_motr_api), std::move(kvs_reader_factory),
          std::move(kvs_writer_factory),
          std::move(global_bucket_index_metadata_factory))) {

  s3_log(S3_LOG_DEBUG, "", "%s Ctor", __func__);
}

S3BucketMetadataCache::Item::~Item() = default;

void S3BucketMetadataCache::Item::on_done(S3BucketMetadataState state) {
  s3_log(S3_LOG_DEBUG, nullptr, "%s Entry", __func__);

  assert(CurrentOp::none != current_op);
  assert(p_engine != nullptr);

  const auto current_op = this->current_op;
  this->current_op = CurrentOp::none;

  if (CurrentOp::fetching == current_op) {
    assert(!fetch_waiters.empty());

    while (!fetch_waiters.empty()) {

      fetch_waiters.front()(*p_engine);
      fetch_waiters.pop();
    }
  } else if (CurrentOp::deleting == current_op) {

    assert(this->on_removed);
    auto on_removed = std::move(this->on_removed);
    on_removed(state, p_engine->get_bucket_name());

  } else {
    assert(this->on_changed);
    auto on_changed = std::move(this->on_changed);
    on_changed(state);
  }
  s3_log(S3_LOG_DEBUG, nullptr, "%s Exit", __func__);
}

void S3BucketMetadataCache::Item::fetch(FetchHanderType callback) {
  s3_log(S3_LOG_DEBUG, nullptr, "%s Entry", __func__);

  if (CurrentOp::deleting == current_op) {
    // TODO: something ...
    callback(*p_engine);
    return;
  }
  if (CurrentOp::saving == current_op ||
      (CurrentOp::none == current_op &&
       p_engine->get_state() == S3BucketMetadataState::present)) {

    callback(*p_engine);
    return;
  }
  fetch_waiters.push(std::move(callback));

  if (CurrentOp::fetching != current_op) {
    current_op = CurrentOp::fetching;
    p_engine->load(std::bind(&S3BucketMetadataCache::Item::on_done, this,
                             std::placeholders::_1));
  }
  s3_log(S3_LOG_DEBUG, nullptr, "%s Exit", __func__);
}

void S3BucketMetadataCache::Item::save(const S3BucketMetadata& src,
                                       StateHandlerType callback) {
  s3_log(S3_LOG_DEBUG, nullptr, "%s Entry", __func__);

  if (current_op != CurrentOp::none) {
    s3_log(S3_LOG_ERROR, "", "Another operation is beeing made");
    callback(S3BucketMetadataState::failed);
    return;
  }
  current_op = CurrentOp::saving;
  on_changed = std::move(callback);

  p_engine->save(src, std::bind(&S3BucketMetadataCache::Item::on_done, this,
                                std::placeholders::_1));

  s3_log(S3_LOG_DEBUG, nullptr, "%s Exit", __func__);
}

void S3BucketMetadataCache::Item::update(const S3BucketMetadata& src,
                                         StateHandlerType callback) {
  s3_log(S3_LOG_DEBUG, nullptr, "%s Entry", __func__);

  if (current_op != CurrentOp::none) {
    s3_log(S3_LOG_ERROR, "", "Another operation is beeing made");
    callback(S3BucketMetadataState::failed);
    return;
  }
  current_op = CurrentOp::saving;
  on_changed = std::move(callback);

  p_engine->update(src, std::bind(&S3BucketMetadataCache::Item::on_done, this,
                                  std::placeholders::_1));

  s3_log(S3_LOG_DEBUG, nullptr, "%s Exit", __func__);
}

void S3BucketMetadataCache::Item::remove(RemoveHandlerType callback) {
  s3_log(S3_LOG_DEBUG, nullptr, "%s Entry", __func__);

  if (current_op != CurrentOp::none) {
    s3_log(S3_LOG_ERROR, "", "Another operation is beeing made");
    callback(S3BucketMetadataState::failed, p_engine->get_bucket_name());
    return;
  }
  current_op = CurrentOp::deleting;
  on_removed = std::move(callback);

  p_engine->remove(std::bind(&S3BucketMetadataCache::Item::on_done, this,
                             std::placeholders::_1));

  s3_log(S3_LOG_DEBUG, nullptr, "%s Exit", __func__);
}

// ************************************************************************* //
// class S3BucketMetadataCache
// ************************************************************************* //

S3BucketMetadataCache::S3BucketMetadataCache(
    unsigned max_cache_size, unsigned expire_interval_sec,
    unsigned refresh_interval_sec, std::shared_ptr<MotrAPI> s3_motr_api,
    std::shared_ptr<S3MotrKVSReaderFactory> motr_kvs_reader_factory,
    std::shared_ptr<S3MotrKVSWriterFactory> motr_kvs_writer_factory,
    std::shared_ptr<S3GlobalBucketIndexMetadataFactory>
        global_bucket_index_metadata_factory)
    : max_cache_size(max_cache_size),
      expire_interval_sec(expire_interval_sec),
      refresh_interval_sec(refresh_interval_sec) {

  this->s3_motr_api = s3_motr_api ? std::move(s3_motr_api)
                                  : std::make_shared<ConcreteMotrAPI>();
  this->motr_kvs_reader_factory =
      motr_kvs_reader_factory ? std::move(motr_kvs_reader_factory)
                              : std::make_shared<S3MotrKVSReaderFactory>();
  this->motr_kvs_writer_factory =
      motr_kvs_writer_factory ? std::move(motr_kvs_writer_factory)
                              : std::make_shared<S3MotrKVSWriterFactory>();
  this->global_bucket_index_metadata_factory =
      global_bucket_index_metadata_factory
          ? std::move(global_bucket_index_metadata_factory)
          : std::make_shared<S3GlobalBucketIndexMetadataFactory>();
}

S3BucketMetadataCache::~S3BucketMetadataCache() = default;

S3BucketMetadataCache::Item* S3BucketMetadataCache::get_item(
    const S3BucketMetadata& src) {

  const auto& bucket_name = src.get_bucket_name();
  auto& sptr = items[bucket_name];

  if (sptr) {
    s3_log(S3_LOG_DEBUG, src.get_request_id(),
           "Metadata for \"%s\" bucket is cached", bucket_name.c_str());
  } else {
    s3_log(S3_LOG_DEBUG, src.get_request_id(),
           "Metadata for \"%s\" bucket is absent in cache",
           bucket_name.c_str());

    // Don't use std::move() below!
    sptr.reset(new Item(src, s3_motr_api, motr_kvs_reader_factory,
                        motr_kvs_writer_factory,
                        global_bucket_index_metadata_factory));
  }
  if (items.size() > max_cache_size) {
    // TODO: remove at least one item from the cache
  }
  return sptr.get();
}

void S3BucketMetadataCache::fetch(const S3BucketMetadata& src,
                                  FetchHanderType callback) {

  s3_log(S3_LOG_DEBUG, src.get_stripped_request_id(), "%s Entry", __func__);

  Item* p_item = get_item(src);
  p_item->fetch(std::move(callback));

  s3_log(S3_LOG_DEBUG, "", "%s Exit", __func__);
}

void S3BucketMetadataCache::save(const S3BucketMetadata& src,
                                 StateHandlerType callback) {

  s3_log(S3_LOG_DEBUG, src.get_stripped_request_id(), "%s Entry", __func__);

  Item* p_item = get_item(src);
  p_item->save(src, std::move(callback));

  s3_log(S3_LOG_DEBUG, "", "%s Exit", __func__);
}

void S3BucketMetadataCache::update(const S3BucketMetadata& src,
                                   StateHandlerType callback) {

  s3_log(S3_LOG_DEBUG, src.get_stripped_request_id(), "%s Entry", __func__);

  Item* p_item = get_item(src);
  p_item->update(src, std::move(callback));

  s3_log(S3_LOG_DEBUG, "", "%s Exit", __func__);
}

void S3BucketMetadataCache::on_remove(StateHandlerType callback,
                                      S3BucketMetadataState state,
                                      std::string bucket_name) {

  s3_log(S3_LOG_DEBUG, "", "%s Entry", __func__);

  if (S3BucketMetadataState::missing == state) {
    items.erase(bucket_name);
  }
  callback(state);
}

void S3BucketMetadataCache::remove(const S3BucketMetadata& src,
                                   StateHandlerType callback) {

  s3_log(S3_LOG_DEBUG, src.get_stripped_request_id(), "%s Entry", __func__);

  Item* p_item = get_item(src);

  p_item->remove(std::bind(&S3BucketMetadataCache::on_remove, this,
                           std::move(callback), std::placeholders::_1,
                           std::placeholders::_2));

  s3_log(S3_LOG_DEBUG, "", "%s Exit", __func__);
}
