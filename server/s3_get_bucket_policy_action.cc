/*
 * COPYRIGHT 2016 SEAGATE LLC
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF SEAGATE TECHNOLOGY
 * LIMITED, ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF SEAGATE TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF SEAGATE LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF SEAGATE'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A SEAGATE REPRESENTATIVE
 * http://www.seagate.com/contact
 *
 * Original author:  Rajesh Nambiar   <rajesh.nambiar@seagate.com>
 * Original creation date: 27-May-2016
 */

#include "s3_get_bucket_policy_action.h"
#include "s3_error_codes.h"
#include "s3_log.h"

S3GetBucketPolicyAction::S3GetBucketPolicyAction(
    std::shared_ptr<S3RequestObject> req,
    std::shared_ptr<S3BucketMetadataFactory> bucket_meta_factory)
    : S3Action(req) {
  s3_log(S3_LOG_DEBUG, request_id, "Constructor\n");

  s3_log(S3_LOG_INFO, request_id, "S3 API: Get Bucket Policy. Bucket[%s]\n",
         request->get_bucket_name().c_str());

  if (bucket_meta_factory) {
    bucket_metadata_factory = bucket_meta_factory;
  } else {
    bucket_metadata_factory = std::make_shared<S3BucketMetadataFactory>();
  }
  setup_steps();
}

void S3GetBucketPolicyAction::setup_steps() {
  s3_log(S3_LOG_DEBUG, request_id, "Setting up the action\n");
  add_task(std::bind(&S3GetBucketPolicyAction::get_metadata, this));
  add_task(
      std::bind(&S3GetBucketPolicyAction::send_response_to_s3_client, this));
  // ...
}

void S3GetBucketPolicyAction::get_metadata() {
  s3_log(S3_LOG_INFO, request_id, "Fetching bucket metadata\n");
  bucket_metadata =
      bucket_metadata_factory->create_bucket_metadata_obj(request);

  // bypass shutdown signal check for next task
  check_shutdown_signal_for_next_task(false);
  bucket_metadata->load(
      std::bind(&S3GetBucketPolicyAction::get_metadata_successful, this),
      std::bind(&S3GetBucketPolicyAction::get_metadata_failed, this));
}

void S3GetBucketPolicyAction::get_metadata_successful() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  std::string response_json = bucket_metadata->get_policy_as_json();
  if (response_json.empty()) {
    set_s3_error("NoSuchBucketPolicy");
  }
  next();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3GetBucketPolicyAction::get_metadata_failed() {
  s3_log(S3_LOG_DEBUG, request_id, "Entering get_metadata_failed\n");
  if (bucket_metadata->get_state() == S3BucketMetadataState::failed_to_launch) {
    s3_log(S3_LOG_ERROR, request_id,
           "Bucket metadata load operation failed due to pre launch failure\n");
    set_s3_error("ServiceUnavailable");
  } else if (bucket_metadata->get_state() == S3BucketMetadataState::missing) {
    s3_log(S3_LOG_ERROR, request_id, "Bucket metadata load operation failed\n");
    set_s3_error("NoSuchBucket");
  } else {
    set_s3_error("InternalError");
    s3_log(S3_LOG_ERROR, request_id,
           "Bucket metadata load operation failed due to internal error\n");
  }
  send_response_to_s3_client();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3GetBucketPolicyAction::send_response_to_s3_client() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");

  if (reject_if_shutting_down() ||
      (is_error_state() && !get_s3_error_code().empty())) {
    S3Error error(get_s3_error_code(), request->get_request_id(),
                  request->get_object_uri());
    std::string& response_xml = error.to_xml();
    request->set_out_header_value("Content-Type", "application/xml");
    request->set_out_header_value("Content-Length",
                                  std::to_string(response_xml.length()));

    if (get_s3_error_code() == "ServiceUnavailable" ||
        get_s3_error_code() == "InternalError") {
      request->set_out_header_value("Connection", "close");
    }

    if (get_s3_error_code() == "ServiceUnavailable") {
      request->set_out_header_value("Retry-After", "1");
    }

    request->send_response(error.get_http_status_code(), response_xml);

  } else {
    request->send_response(S3HttpSuccess200,
                           bucket_metadata->get_policy_as_json());
  }

  done();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}
