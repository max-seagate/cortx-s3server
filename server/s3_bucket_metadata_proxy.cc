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

#include "s3_bucket_metadata_cache.h"
#include "s3_bucket_metadata_proxy.h"
#include "s3_log.h"
#include "s3_request_object.h"

extern S3BucketMetadataCache* p_bucket_metadata_cache;

S3BucketMetadataProxy::S3BucketMetadataProxy(
    std::shared_ptr<S3RequestObject> s3_req_obj, const std::string& bucket)
    : S3BucketMetadata(std::move(s3_req_obj), bucket) {}

void S3BucketMetadataProxy::on_load(const S3BucketMetadata& src) {

  s3_log(S3_LOG_DEBUG, stripped_request_id, "%s Entry\n", __func__);

  auto request_backup = std::move(this->request);

  static_cast<S3BucketMetadata&>(*this) = src;

  request = std::move(request_backup);

  request_id = request->get_request_id();
  stripped_request_id = request->get_stripped_request_id();

  if (S3BucketMetadataState::present == state) {
    handler_on_success();
  } else {
    handler_on_failed();
  }
}

void S3BucketMetadataProxy::on_save(S3BucketMetadataState state) {
  s3_log(S3_LOG_DEBUG, stripped_request_id, "%s Entry\n", __func__);
  this->state = state;

  if (S3BucketMetadataState::present == state) {
    handler_on_success();
  } else {
    handler_on_failed();
  }
}

void S3BucketMetadataProxy::on_update(S3BucketMetadataState state) {
  s3_log(S3_LOG_DEBUG, stripped_request_id, "%s Entry\n", __func__);
  this->state = state;

  if (S3BucketMetadataState::present == state) {
    handler_on_success();
  } else {
    handler_on_failed();
  }
}

void S3BucketMetadataProxy::on_remove(S3BucketMetadataState state) {
  s3_log(S3_LOG_DEBUG, stripped_request_id, "%s Entry\n", __func__);
  this->state = state;

  if (S3BucketMetadataState::missing == state) {
    handler_on_success();
  } else {
    handler_on_failed();
  }
}

void S3BucketMetadataProxy::load(std::function<void(void)> on_success,
                                 std::function<void(void)> on_failed) {

  s3_log(S3_LOG_DEBUG, stripped_request_id, "%s Entry\n", __func__);

  handler_on_success = std::move(on_success);
  handler_on_failed = std::move(on_failed);

  p_bucket_metadata_cache->fetch(
      *this,
      std::bind(&S3BucketMetadataProxy::on_load, this, std::placeholders::_1));
}

void S3BucketMetadataProxy::save(std::function<void(void)> on_success,
                                 std::function<void(void)> on_failed) {

  s3_log(S3_LOG_DEBUG, stripped_request_id, "%s Entry\n", __func__);

  handler_on_success = std::move(on_success);
  handler_on_failed = std::move(on_failed);

  p_bucket_metadata_cache->save(
      *this,
      std::bind(&S3BucketMetadataProxy::on_save, this, std::placeholders::_1));
}

void S3BucketMetadataProxy::update(std::function<void(void)> on_success,
                                   std::function<void(void)> on_failed) {

  s3_log(S3_LOG_DEBUG, stripped_request_id, "%s Entry\n", __func__);

  handler_on_success = std::move(on_success);
  handler_on_failed = std::move(on_failed);

  p_bucket_metadata_cache->update(
      *this, std::bind(&S3BucketMetadataProxy::on_update, this,
                       std::placeholders::_1));
}

void S3BucketMetadataProxy::remove(std::function<void(void)> on_success,
                                   std::function<void(void)> on_failed) {

  s3_log(S3_LOG_DEBUG, stripped_request_id, "%s Entry\n", __func__);

  handler_on_success = std::move(on_success);
  handler_on_failed = std::move(on_failed);

  p_bucket_metadata_cache->remove(
      *this, std::bind(&S3BucketMetadataProxy::on_remove, this,
                       std::placeholders::_1));
}
