/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ads/internal/server/refill_unblinded_tokens/get_signed_tokens_request.h"

#include "bat/ads/internal/logging.h"
#include "bat/ads/internal/server/ads_server_util.h"

namespace ads {

GetSignedTokensRequest::GetSignedTokensRequest() = default;

GetSignedTokensRequest::~GetSignedTokensRequest() = default;

// GET /v1/confirmation/token/{payment_id}?nonce={nonce}

std::string GetSignedTokensRequest::BuildUrl(
    const WalletInfo& wallet_info,
    const std::string& nonce) const {
  DCHECK(!wallet_info.payment_id.empty());
  DCHECK(!nonce.empty());

  std::string endpoint = "/v1/confirmation/token/";
  endpoint += wallet_info.payment_id;
  endpoint += "?nonce=";
  endpoint += nonce;

  return server::GetDomain().append(endpoint);
}

UrlRequestMethod GetSignedTokensRequest::GetMethod() const {
  return UrlRequestMethod::GET;
}

}  // namespace ads
