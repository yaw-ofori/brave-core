/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BAT_ADS_INTERNAL_SERVER_GET_AD_REWARDS_GET_PAYMENTS_PAYMENTS_URL_REQUEST_BUILDER_H_
#define BAT_ADS_INTERNAL_SERVER_GET_AD_REWARDS_GET_PAYMENTS_PAYMENTS_URL_REQUEST_BUILDER_H_

#include <string>
#include <vector>

#include "bat/ads/internal/server/url_request_builder.h"
#include "bat/ads/wallet_info.h"

namespace ads {

class PaymentsUrlRequestBuilder : UrlRequestBuilder {
 public:
  PaymentsUrlRequestBuilder(
      const WalletInfo& wallet_info);

  ~PaymentsUrlRequestBuilder() override;

  UrlRequest Build() override;

 private:
  WalletInfo wallet_info_;

  std::string BuildUrl() const;

  std::string BuildBody() const;

  std::vector<std::string> BuildHeaders(
      const std::string& body) const;

  std::string GetDigestHeaderValue(
      const std::string& body) const;

  std::string GetSignatureHeaderValue(
      const std::string& body) const;
};

}  // namespace ads

#endif  // BAT_ADS_INTERNAL_SERVER_GET_AD_REWARDS_GET_PAYMENTS_PAYMENTS_URL_REQUEST_BUILDER_H_
