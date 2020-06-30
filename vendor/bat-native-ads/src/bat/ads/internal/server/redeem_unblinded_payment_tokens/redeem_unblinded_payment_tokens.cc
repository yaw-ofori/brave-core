/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ads/internal/server/redeem_unblinded_payment_tokens/redeem_unblinded_payment_tokens.h"

#include "brave_base/random.h"
#include "net/http/http_status_code.h"
#include "bat/ads/internal/ads_impl.h"
#include "bat/ads/internal/logging.h"
#include "bat/ads/internal/server/redeem_unblinded_payment_tokens/redeem_unblinded_payment_tokens_request.h"
#include "bat/ads/internal/time_util.h"

using std::placeholders::_1;

namespace ads {

RedeemUnblindedPaymentTokens::RedeemUnblindedPaymentTokens(
    AdsImpl* ads)
    : ads_(ads) {
  DCHECK(ads_);
}

RedeemUnblindedPaymentTokens::~RedeemUnblindedPaymentTokens() = default;

void RedeemUnblindedPaymentTokens::set_delegate(
    RedeemUnblindedPaymentTokensDelegate* delegate) {
  DCHECK(delegate);

  delegate_ = delegate;
}

void RedeemUnblindedPaymentTokens::RedeemAfterDelay(
    const WalletInfo& wallet_info) {
  if (retry_timer_.IsRunning()) {
    return;
  }

  wallet_info_ = wallet_info;
  if (!wallet_info_.IsValid()) {
    BLOG(0, "Failed to redeem unblinded payment tokens due to invalid wallet");
    return;
  }

  const uint64_t delay = CalculateTokenRedemptionDelay();

  const base::Time time = timer_.Start(delay,
      base::BindOnce(&RedeemUnblindedPaymentTokens::Redeem,
          base::Unretained(this)));

  BLOG(1, "Redeem unblinded payment tokens " << FriendlyDateAndTime(time));
}

uint64_t RedeemUnblindedPaymentTokens::get_token_redemption_timestamp() const {
  return token_redemption_timestamp_in_seconds_;
}

void RedeemUnblindedPaymentTokens::set_token_redemption_timestamp(
    const uint64_t timestamp_in_seconds) {
  token_redemption_timestamp_in_seconds_ = timestamp_in_seconds;
}

///////////////////////////////////////////////////////////////////////////////

void RedeemUnblindedPaymentTokens::Redeem() {
  BLOG(1, "RedeemUnblindedPaymentTokens");

  if (ads_->get_unblinded_payment_tokens()->IsEmpty()) {
    BLOG(1, "No unblinded payment tokens to redeem");
    ScheduleNextTokenRedemption();
    return;
  }

  BLOG(1, "PUT /v1/confirmation/payment/{payment_id}");
  RedeemUnblindedPaymentTokensRequest request;

  auto tokens = ads_->get_unblinded_payment_tokens()->GetAllTokens();

  auto payload = request.CreatePayload(wallet_info_);

  auto url = request.BuildUrl(wallet_info_);
  auto method = request.GetMethod();
  auto body = request.BuildBody(tokens, payload);
  auto headers = request.BuildHeaders();
  auto content_type = request.GetContentType();

  auto callback = std::bind(&RedeemUnblindedPaymentTokens::OnRedeem, this, _1);

  BLOG(5, UrlRequestToString(url, headers, body, content_type, method));
  ads_->get_ads_client()->UrlRequest(url, headers, body, content_type,
      method, callback);
}

void RedeemUnblindedPaymentTokens::OnRedeem(
    const UrlResponse& url_response) {
  BLOG(1, "OnRedeemUnblindedPaymentTokens");

  BLOG(6, UrlResponseToString(url_response));

  if (url_response.status_code != net::HTTP_OK) {
    BLOG(1, "Failed to redeem unblinded payment tokens");
    OnRedeemUnblindedPaymentTokens(FAILED);
    return;
  }

  OnRedeemUnblindedPaymentTokens(SUCCESS);
}

void RedeemUnblindedPaymentTokens::OnRedeemUnblindedPaymentTokens(
    const Result result) {
  if (result != SUCCESS) {
    if (delegate_) {
      delegate_->OnFailedToRedeemUnblindedPaymentTokens();
    }

    const base::Time time = retry_timer_.StartWithPrivacy(
        kRetryRedeemUnblindedPaymentTokensAfterSeconds,
            base::BindOnce(&RedeemUnblindedPaymentTokens::OnRetry,
                base::Unretained(this)));

    BLOG(1, "Retry redeeming unblinded payment tokens "
        << FriendlyDateAndTime(time));

    return;
  }

  ads_->AddUnredeemedTransactionsToPendingRewards();
  ads_->get_unblinded_payment_tokens()->RemoveAllTokens();

  ads_->UpdateAdsRewards(true);

  retry_timer_.Stop();

  ScheduleNextTokenRedemption();

  if (delegate_) {
    delegate_->OnDidRedeemUnblindedPaymentTokens();
  }
}

void RedeemUnblindedPaymentTokens::ScheduleNextTokenRedemption() {
  UpdateNextTokenRedemptionDate();
  RedeemAfterDelay(wallet_info_);
}

void RedeemUnblindedPaymentTokens::OnRetry() {
  if (delegate_) {
    delegate_->OnDidRetryRedeemingUnblindedPaymentTokens();
  }

  Redeem();
}

uint64_t RedeemUnblindedPaymentTokens::CalculateTokenRedemptionDelay() {
  if (token_redemption_timestamp_in_seconds_ == 0) {
    UpdateNextTokenRedemptionDate();
  }

  const uint64_t now_in_seconds =
      static_cast<uint64_t>(base::Time::Now().ToDoubleT());

  uint64_t delay;
  if (now_in_seconds >= token_redemption_timestamp_in_seconds_) {
    // Browser was launched after the token redemption date
    delay = 1 * base::Time::kSecondsPerMinute;
  } else {
    delay = token_redemption_timestamp_in_seconds_ - now_in_seconds;
  }

  return delay;
}

void RedeemUnblindedPaymentTokens::UpdateNextTokenRedemptionDate() {
  const uint64_t now_in_seconds =
      static_cast<uint64_t>(base::Time::Now().ToDoubleT());

  uint64_t delay;

  if (!_is_debug) {
    delay = kNextTokenRedemptionAfterSeconds;
  } else {
    delay = kDebugNextTokenRedemptionAfterSeconds;
  }

  const uint64_t rand_delay = brave_base::random::Geometric(delay);

  token_redemption_timestamp_in_seconds_ = now_in_seconds + rand_delay;
  ads_->SaveState();
}

}  // namespace ads
