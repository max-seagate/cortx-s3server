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

#ifndef __S3_SERVER_S3_BUCKET_METADATA_H__
#define __S3_SERVER_S3_BUCKET_METADATA_H__

#include <functional>
#include <map>
#include <memory>
#include <string>

#include <gtest/gtest_prod.h>

#include "lib/types.h"  // struct m0_uint128

enum class S3BucketMetadataState {
  empty,    // Initial state, no lookup done
  present,  // Metadata exists and was read successfully
  missing,  // Metadata not present in store.
  failed,
  failed_to_launch  // pre launch operation failed
};

// Forward declarations
class S3RequestObject;

class S3BucketMetadata {
  // Holds mainly system-defined metadata (creation date etc)
  // Partially supported on need bases, some of these are placeholders
 protected:
  std::string account_name;
  std::string account_id;
  std::string user_name;
  std::string owner_canonical_id;
  std::string user_id;
  std::string encoded_acl;

  // The name for a Bucket
  // http://docs.aws.amazon.com/AmazonS3/latest/dev/BucketRestrictions.html
  std::string bucket_name;
  std::string bucket_policy;
  std::map<std::string, std::string> bucket_tags;
  std::map<std::string, std::string> system_defined_attribute;
  std::map<std::string, std::string> user_defined_attribute;

  struct m0_uint128 multipart_index_oid = {};
  struct m0_uint128 object_list_index_oid = {};
  struct m0_uint128 objects_version_list_index_oid = {};

  std::shared_ptr<S3RequestObject> request;

  S3BucketMetadataState state = S3BucketMetadataState::empty;

  std::string request_id;
  std::string stripped_request_id;

  void initialize(const std::string& bucket);

  const std::string& get_owner_name();

  void set_multipart_index_oid(struct m0_uint128 id);
  void set_object_list_index_oid(struct m0_uint128 id);
  void set_objects_version_list_index_oid(struct m0_uint128 id);

  // Load attributes
  void add_system_attribute(std::string key, std::string val);
  void add_user_defined_attribute(std::string key, std::string val);

  // Streaming to json
  virtual std::string to_json();

 protected:
  S3BucketMetadata(std::shared_ptr<S3RequestObject> req,
                   const std::string& bucket);

 public:
  virtual ~S3BucketMetadata();

  const std::string& get_request_id() const { return request_id; }

  const std::string& get_stripped_request_id() const {
    return stripped_request_id;
  }
  const std::string& get_bucket_name() const;
  const std::string& get_creation_time();
  const std::string& get_location_constraint();
  const std::string& get_owner_id();
  const std::string& get_bucket_owner_account_id();
  const std::string& get_encoded_bucket_acl() const;

  virtual bool check_bucket_tags_exists() const;

  virtual const std::string& get_owner_canonical_id();
  virtual std::string& get_policy_as_json();

  virtual std::string get_tags_as_xml();
  virtual std::string get_acl_as_xml();

  void acl_from_json(std::string acl_json_str);

  const struct m0_uint128& get_object_list_index_oid() const;
  const struct m0_uint128& get_objects_version_list_index_oid() const;
  const struct m0_uint128& get_multipart_index_oid() const;

  virtual void set_location_constraint(std::string location);

  virtual void load(std::function<void(void)> on_success,
                    std::function<void(void)> on_failed);

  // PUT KV in both global bucket list index and bucket metadata list index.
  virtual void save(std::function<void(void)> on_success,
                    std::function<void(void)> on_failed);

  // Update bucket metadata in bucket metadata list index.
  virtual void update(std::function<void(void)> on_success,
                      std::function<void(void)> on_failed);

  virtual void setpolicy(std::string policy_str);
  virtual void set_tags(const std::map<std::string, std::string>& tags_as_map);
  virtual void deletepolicy();
  virtual void delete_bucket_tags();

  virtual void setacl(const std::string& acl_str);

  virtual void remove(std::function<void(void)> on_success,
                      std::function<void(void)> on_failed);

  // returns 0 on success, -1 on parsing error
  virtual int from_json(std::string content);

  virtual S3BucketMetadataState get_state() const { return state; }

  // Google tests
  FRIEND_TEST(S3BucketMetadataTest, ConstructorTest);
  FRIEND_TEST(S3BucketMetadataTest, GetSystemAttributesTest);
  FRIEND_TEST(S3BucketMetadataTest, GetSetOIDsPolicyAndLocation);
  FRIEND_TEST(S3BucketMetadataTest, DeletePolicy);
  FRIEND_TEST(S3BucketMetadataTest, GetTagsAsXml);
  FRIEND_TEST(S3BucketMetadataTest, GetSpecialCharTagsAsXml);
  FRIEND_TEST(S3BucketMetadataTest, AddSystemAttribute);
  FRIEND_TEST(S3BucketMetadataTest, AddUserDefinedAttribute);
  FRIEND_TEST(S3BucketMetadataTest, Load);
  FRIEND_TEST(S3BucketMetadataTest, FetchBucketListIndexOid);
  FRIEND_TEST(S3BucketMetadataTest,
              FetchBucketListIndexOIDSuccessStateIsSaving);
  FRIEND_TEST(S3BucketMetadataTest,
              FetchBucketListIndexOIDSuccessStateIsFetching);
  FRIEND_TEST(S3BucketMetadataTest,
              FetchBucketListIndexOIDSuccessStateIsDeleting);
  FRIEND_TEST(S3BucketMetadataTest, FetchBucketListIndexOIDFailedState);
  FRIEND_TEST(S3BucketMetadataTest, FetchBucketListIndexOIDFailedStateIsSaving);
  FRIEND_TEST(S3BucketMetadataTest,
              FetchBucketListIndexOIDFailedStateIsFetching);
  FRIEND_TEST(S3BucketMetadataTest,
              FetchBucketListIndexOIDFailedIndexMissingStateSaved);
  FRIEND_TEST(S3BucketMetadataTest,
              FetchBucketListIndexOIDFailedIndexFailedStateIsFetching);
  FRIEND_TEST(S3BucketMetadataTest, FetchBucketListIndexOIDFailedIndexFailed);
  FRIEND_TEST(S3BucketMetadataTest, LoadBucketInfo);
  FRIEND_TEST(S3BucketMetadataTest, SetBucketPolicy);
  FRIEND_TEST(S3BucketMetadataTest, SetAcl);
  FRIEND_TEST(S3BucketMetadataTest, LoadBucketInfoFailedJsonParsingFailed);
  FRIEND_TEST(S3BucketMetadataTest, LoadBucketInfoFailedMetadataMissing);
  FRIEND_TEST(S3BucketMetadataTest, LoadBucketInfoFailedMetadataFailed);
  FRIEND_TEST(S3BucketMetadataTest, LoadBucketInfoFailedMetadataFailedToLaunch);
  FRIEND_TEST(S3BucketMetadataTest, SaveMeatdataMissingIndexOID);
  FRIEND_TEST(S3BucketMetadataTest, SaveMeatdataIndexOIDPresent);
  FRIEND_TEST(S3BucketMetadataTest, CreateObjectIndexOIDNotPresent);
  FRIEND_TEST(S3BucketMetadataTest, CreateBucketListIndexCollisionCount0);
  FRIEND_TEST(S3BucketMetadataTest, CreateBucketListIndexCollisionCount1);
  FRIEND_TEST(S3BucketMetadataTest, CreateBucketListIndexSuccessful);
  FRIEND_TEST(S3BucketMetadataTest,
              CreateBucketListIndexFailedCollisionHappened);
  FRIEND_TEST(S3BucketMetadataTest, CreateBucketListIndexFailed);
  FRIEND_TEST(S3BucketMetadataTest, CreateBucketListIndexFailedToLaunch);
  FRIEND_TEST(S3BucketMetadataTest, HandleCollision);
  FRIEND_TEST(S3BucketMetadataTest, HandleCollisionMaxAttemptExceeded);
  FRIEND_TEST(S3BucketMetadataTest, RegeneratedNewIndexName);
  FRIEND_TEST(S3BucketMetadataTest, SaveBucketListIndexOid);
  FRIEND_TEST(S3BucketMetadataTest, SaveBucketListIndexOidSucessfulStateSaving);
  FRIEND_TEST(S3BucketMetadataTest, SaveBucketListIndexOidSucessful);
  FRIEND_TEST(S3BucketMetadataTest,
              SaveBucketListIndexOidSucessfulBcktMetaMissing);
  FRIEND_TEST(S3BucketMetadataTest, SaveBucketListIndexOIDFailed);
  FRIEND_TEST(S3BucketMetadataTest, SaveBucketListIndexOIDFailedToLaunch);
  FRIEND_TEST(S3BucketMetadataTest, SaveBucketInfo);
  FRIEND_TEST(S3BucketMetadataTest, SaveBucketInfoSuccess);
  FRIEND_TEST(S3BucketMetadataTest, SaveBucketInfoFailed);
  FRIEND_TEST(S3BucketMetadataTest, SaveBucketInfoFailedToLaunch);
  FRIEND_TEST(S3BucketMetadataTest, RemovePresentMetadata);
  FRIEND_TEST(S3BucketMetadataTest, RemoveAfterFetchingBucketListIndexOID);
  FRIEND_TEST(S3BucketMetadataTest, RemoveBucketInfo);
  FRIEND_TEST(S3BucketMetadataTest, RemoveBucketInfoSuccessful);
  FRIEND_TEST(S3BucketMetadataTest, RemoveBucketInfoFailed);
  FRIEND_TEST(S3BucketMetadataTest, RemoveBucketInfoFailedToLaunch);
  FRIEND_TEST(S3BucketMetadataTest, CreateDefaultAcl);
  FRIEND_TEST(S3BucketMetadataTest, ToJson);
  FRIEND_TEST(S3BucketMetadataTest, FromJson);
  FRIEND_TEST(S3BucketMetadataTest, GetEncodedBucketAcl);
  FRIEND_TEST(S3BucketMetadataTest, CreateMultipartListIndexCollisionCount0);
  FRIEND_TEST(S3BucketMetadataTest, CreateObjectListIndexCollisionCount0);
  FRIEND_TEST(S3BucketMetadataTest, CreateObjectListIndexSuccessful);
  FRIEND_TEST(S3BucketMetadataTest,
              CreateObjectListIndexFailedCollisionHappened);
  FRIEND_TEST(S3BucketMetadataTest,
              CreateMultipartListIndexFailedCollisionHappened);
  FRIEND_TEST(S3BucketMetadataTest, CreateObjectListIndexFailed);
  FRIEND_TEST(S3BucketMetadataTest, CreateObjectListIndexFailedToLaunch);
  FRIEND_TEST(S3BucketMetadataTest, CreateMultipartListIndexFailed);
  FRIEND_TEST(S3BucketMetadataTest, CreateMultipartListIndexFailedToLaunch);
};

#endif
