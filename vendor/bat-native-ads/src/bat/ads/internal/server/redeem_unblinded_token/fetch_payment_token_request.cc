/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ads/internal/server/redeem_unblinded_token/fetch_payment_token_request.h"

#include "bat/ads/internal/logging.h"
#include "bat/ads/internal/server/ads_server_util.h"
#include "bat/ads/mojom.h"

namespace ads {

FetchPaymentTokenRequest::FetchPaymentTokenRequest() = default;

FetchPaymentTokenRequest::~FetchPaymentTokenRequest() = default;

// GET /v1/confirmation/{confirmation_id}/paymentToken

std::string FetchPaymentTokenRequest::BuildUrl(
    const std::string& confirmation_id) const {
  DCHECK(!confirmation_id.empty());

  std::string endpoint = "/v1/confirmation/";
  endpoint += confirmation_id;
  endpoint += "/paymentToken";

  return server::GetDomain().append(endpoint);
}

UrlRequestMethod FetchPaymentTokenRequest::GetMethod() const {
  return UrlRequestMethod::GET;
}

}  // namespace ads
