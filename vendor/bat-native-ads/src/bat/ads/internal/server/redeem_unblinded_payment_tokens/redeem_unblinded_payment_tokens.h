/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BAT_ADS_INTERNAL_REDEEM_UNBLINDED_PAYMENT_TOKENS_H_
#define BAT_ADS_INTERNAL_REDEEM_UNBLINDED_PAYMENT_TOKENS_H_

#include <stdint.h>

#include "bat/ads/internal/backoff_timer.h"
#include "bat/ads/internal/server/redeem_unblinded_payment_tokens/redeem_unblinded_payment_tokens_delegate.h"
#include "bat/ads/internal/timer.h"
#include "bat/ads/mojom.h"
#include "bat/ads/result.h"
#include "bat/ads/wallet_info.h"

namespace ads {

class AdsImpl;

class RedeemUnblindedPaymentTokens {
 public:
  RedeemUnblindedPaymentTokens(
      AdsImpl* ads);

  ~RedeemUnblindedPaymentTokens();

  void set_delegate(
      RedeemUnblindedPaymentTokensDelegate* delegate);

  void RedeemAfterDelay(
      const WalletInfo& wallet_info);

  uint64_t get_token_redemption_timestamp() const;
  void set_token_redemption_timestamp(
      const uint64_t timestamp_in_seconds);

 private:
  WalletInfo wallet_info_;

  Timer timer_;

  void Redeem();
  void OnRedeem(
      const UrlResponse& url_response);

  void OnRedeemUnblindedPaymentTokens(
      const Result result);

  void ScheduleNextTokenRedemption();
  uint64_t token_redemption_timestamp_in_seconds_ = 0;

  BackoffTimer retry_timer_;
  void OnRetry();

  uint64_t CalculateTokenRedemptionDelay();
  void UpdateNextTokenRedemptionDate();

  AdsImpl* ads_;  // NOT OWNED

  RedeemUnblindedPaymentTokensDelegate* delegate_ = nullptr;
};

}  // namespace ads

#endif  // BAT_ADS_INTERNAL_REDEEM_UNBLINDED_PAYMENT_TOKENS_H_
