/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BAT_ADS_INTERNAL_GET_SIGNED_TOKENS_REQUEST_H_
#define BAT_ADS_INTERNAL_GET_SIGNED_TOKENS_REQUEST_H_

#include <string>

#include "bat/ads/mojom.h"
#include "bat/ads/wallet_info.h"

namespace ads {

class GetSignedTokensRequest {
 public:
  GetSignedTokensRequest();
  ~GetSignedTokensRequest();

  std::string BuildUrl(
      const WalletInfo& wallet_info,
      const std::string& nonce) const;

  UrlRequestMethod GetMethod() const;
};

}  // namespace ads

#endif  // BAT_ADS_INTERNAL_GET_SIGNED_TOKENS_REQUEST_H_
