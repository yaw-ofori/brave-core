/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BAT_ADS_INTERNAL_FETCH_PAYMENT_TOKEN_REQUEST_H_
#define BAT_ADS_INTERNAL_FETCH_PAYMENT_TOKEN_REQUEST_H_

#include <string>

#include "bat/ads/mojom.h"

namespace ads {

class FetchPaymentTokenRequest {
 public:
  FetchPaymentTokenRequest();
  ~FetchPaymentTokenRequest();

  std::string BuildUrl(
      const std::string& confirmation_id) const;

  UrlRequestMethod GetMethod() const;
};

}  // namespace ads

#endif  // BAT_ADS_INTERNAL_FETCH_PAYMENT_TOKEN_REQUEST_H_
