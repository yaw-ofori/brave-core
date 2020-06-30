/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ads/internal/server/ad_rewards/payments/payments_url_request_builder.h"

#include <stdint.h>

#include "base/base64.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "bat/ads/internal/logging.h"
#include "bat/ads/internal/security/security_utils.h"
#include "bat/ads/internal/server/ads_server_util.h"

namespace ads {

PaymentsUrlRequestBuilder::PaymentsUrlRequestBuilder(
    const WalletInfo& wallet_info)
    : wallet_info_(wallet_info) {
  DCHECK(wallet_info_.IsValid());
}

PaymentsUrlRequestBuilder::~PaymentsUrlRequestBuilder() = default;

// GET /v1/confirmation/payment/{payment_id}

UrlRequest PaymentsUrlRequestBuilder::Build() {
  const std::string body = BuildBody();

  UrlRequest request;
  request.url = BuildUrl();
  request.headers = BuildHeaders(body);
  request.content = body;
  request.content_type = "application/json";
  request.method = UrlRequestMethod::GET;

  return request;
}

//////////////////////////////////////////////////////////////////////////////

std::string PaymentsUrlRequestBuilder::BuildUrl() const {
  return base::StringPrintf("%s/v1/confirmation/payment/%s",
      server::GetDomain().c_str(), wallet_info_.payment_id.c_str());
}

std::string PaymentsUrlRequestBuilder::BuildBody() const {
  base::Value dictionary(base::Value::Type::DICTIONARY);

  std::string json;
  base::JSONWriter::Write(dictionary, &json);

  return json;
}

std::vector<std::string> PaymentsUrlRequestBuilder::BuildHeaders(
    const std::string& body) const {
  std::string digest_header = "digest: ";
  digest_header += GetDigestHeaderValue(body);

  std::string signature_header = "signature: ";
  signature_header += GetSignatureHeaderValue(body);

  const std::string accept_header = "accept: application/json";

  return {
    digest_header,
    signature_header,
    accept_header
  };
}

std::string PaymentsUrlRequestBuilder::GetDigestHeaderValue(
    const std::string& body) const {
  std::string value;

  if (body.empty()) {
    return value;
  }

  const std::vector<uint8_t> body_sha256 = security::Sha256Hash(body);
  const std::string body_sha256_base64 = base::Base64Encode(body_sha256);

  value = "SHA-256=" + body_sha256_base64;

  return value;
}

std::string PaymentsUrlRequestBuilder::GetSignatureHeaderValue(
    const std::string& body) const {
  const std::string value = GetDigestHeaderValue(body);

  std::vector<uint8_t> private_key;
  if (!base::HexStringToBytes(wallet_info_.private_key, &private_key)) {
    return "";
  }

  return security::Sign({{"digest", value}}, "primary", private_key);
}

}  // namespace ads
