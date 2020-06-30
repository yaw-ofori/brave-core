/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BAT_ADS_INTERNAL_REFILL_UNBLINDED_TOKENS_H_
#define BAT_ADS_INTERNAL_REFILL_UNBLINDED_TOKENS_H_

#include <string>
#include <vector>

#include "wrapper.hpp"  // NOLINT
#include "bat/ads/ads_client.h"
#include "bat/ads/internal/backoff_timer.h"
#include "bat/ads/internal/server/refill_unblinded_tokens/refill_unblinded_tokens_delegate.h"
#include "bat/ads/internal/privacy/unblinded_tokens/unblinded_tokens.h"
#include "bat/ads/mojom.h"
#include "bat/ads/wallet_info.h"

namespace ads {

class AdsImpl;

using challenge_bypass_ristretto::Token;
using challenge_bypass_ristretto::BlindedToken;

class RefillUnblindedTokens {
 public:
  RefillUnblindedTokens(
      AdsImpl* ads);

  ~RefillUnblindedTokens();

  void set_delegate(
      RefillUnblindedTokensDelegate* delegate);

  void Refill(
      const WalletInfo& wallet_info,
      const std::string& public_key);

 private:
  WalletInfo wallet_info_;

  std::string public_key_;

  std::string nonce_;

  std::vector<Token> tokens_;
  std::vector<BlindedToken> blinded_tokens_;

  void RequestSignedTokens();
  void OnRequestSignedTokens(
      const UrlResponse& url_response);

  void GetSignedTokens();
  void OnGetSignedTokens(
      const UrlResponse& url_response);

  void OnRefill(
      const Result result,
      const bool should_retry = true);

  BackoffTimer retry_timer_;
  void Retry();

  bool ShouldRefillUnblindedTokens() const;
  int CalculateAmountOfTokensToRefill() const;

  void GenerateAndBlindTokens(const int count);

  AdsImpl* ads_;  // NOT OWNED

  RefillUnblindedTokensDelegate* delegate_ = nullptr;
};

}  // namespace ads

#endif  // BAT_ADS_INTERNAL_REFILL_UNBLINDED_TOKENS_H_
