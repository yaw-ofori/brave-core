/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BAT_ADS_INTERNAL_REDEEM_UNBLINDED_PAYMENT_TOKENS_REQUEST_H_
#define BAT_ADS_INTERNAL_REDEEM_UNBLINDED_PAYMENT_TOKENS_REQUEST_H_

#include <string>
#include <vector>

#include "base/values.h"
#include "bat/ads/internal/privacy/unblinded_tokens/unblinded_token_info.h"
#include "bat/ads/mojom.h"
#include "bat/ads/wallet_info.h"

namespace ads {

class RedeemUnblindedPaymentTokensRequest {
 public:
  RedeemUnblindedPaymentTokensRequest();
  ~RedeemUnblindedPaymentTokensRequest();

  std::string BuildUrl(
      const WalletInfo& wallet_info) const;

  UrlRequestMethod GetMethod() const;

  std::string BuildBody(
      const privacy::UnblindedTokenList& unblinded_tokens,
      const std::string& payload) const;

  std::string CreatePayload(
      const WalletInfo& wallet_info) const;

  std::vector<std::string> BuildHeaders() const;
  std::string GetAcceptHeaderValue() const;

  std::string GetContentType() const;

 private:
  base::Value CreatePaymentRequestDTO(
      const privacy::UnblindedTokenList& unblinded_tokens,
      const std::string& payload) const;

  base::Value CreateCredential(
      const privacy::UnblindedTokenInfo& unblinded_token,
      const std::string& payload) const;
};

}  // namespace ads

#endif  // BAT_ADS_INTERNAL_REDEEM_UNBLINDED_PAYMENT_TOKENS_REQUEST_H_
