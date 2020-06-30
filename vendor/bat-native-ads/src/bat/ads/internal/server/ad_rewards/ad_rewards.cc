/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ads/internal/server/ad_rewards/ad_rewards.h"

#include <utility>

#include "brave_base/random.h"
#include "net/http/http_status_code.h"
#include "bat/ads/internal/ads_impl.h"
#include "bat/ads/internal/logging.h"
#include "bat/ads/internal/server/ad_rewards/ad_grants/ad_grants_url_request_builder.h"
#include "bat/ads/internal/server/ad_rewards/payments/payments_url_request_builder.h"
#include "bat/ads/internal/static_values.h"
#include "bat/ads/internal/time_util.h"

using std::placeholders::_1;

namespace ads {

AdRewards::AdRewards(
    AdsImpl* ads)
    : ads_(ads),
      ad_grants_(std::make_unique<AdGrants>()),
      payments_(std::make_unique<Payments>()) {
  DCHECK(ads_);
}

AdRewards::~AdRewards() = default;

void AdRewards::Update(
    const WalletInfo& wallet_info,
    const bool should_reconcile_with_server) {
  Update();

  if (!should_reconcile_with_server) {
    return;
  }

  if (retry_timer_.IsRunning()) {
    return;
  }

  wallet_info_ = wallet_info;
  if (!wallet_info_.IsValid()) {
    BLOG(0, "Failed to get ad rewards due to invalid wallet");
    return;
  }

  BLOG(1, "Reconcile ad rewards with server");
  GetPayments();
}

base::Value AdRewards::GetAsDictionary() {
  base::Value dictionary(base::Value::Type::DICTIONARY);

  auto grants_balance = ad_grants_->GetBalance();
  dictionary.SetKey("grants_balance", base::Value(grants_balance));

  auto payments = payments_->GetAsList();
  dictionary.SetKey("payments", base::Value(std::move(payments)));

  return dictionary;
}

bool AdRewards::SetFromDictionary(
    base::DictionaryValue* dictionary) {
  DCHECK(dictionary);

  auto* ads_rewards_value = dictionary->FindKey("ads_rewards");
  if (!ads_rewards_value || !ads_rewards_value->is_dict()) {
    Update();
    return false;
  }

  base::DictionaryValue* ads_rewards_dictionary;
  if (!ads_rewards_value->GetAsDictionary(&ads_rewards_dictionary)) {
    Update();
    return false;
  }

  auto success = true;

  if (!ad_grants_->SetFromDictionary(ads_rewards_dictionary) ||
      !payments_->SetFromDictionary(ads_rewards_dictionary)) {
    success = false;
  }

  Update();

  return success;
}

///////////////////////////////////////////////////////////////////////////////

void AdRewards::GetPayments() {
  BLOG(1, "GetPayments");
  BLOG(2, "GET /v1/confirmation/payment/{payment_id}");

  PaymentsUrlRequestBuilder url_request_builder(wallet_info_);
  const UrlRequest url_request = url_request_builder.Build();
  BLOG(5, UrlRequestToString(url_request));

  auto callback = std::bind(&AdRewards::OnGetPayments, this, _1);
  ads_->get_ads_client()->UrlRequest(url_request, callback);
}

void AdRewards::OnGetPayments(
    const UrlResponse& url_response) {
  BLOG(1, "OnGetPayments");

  BLOG(6, UrlResponseToString(url_response));

  if (url_response.status_code != net::HTTP_OK) {
    BLOG(1, "Failed to get payment balance");
    OnAdRewards(FAILED);
    return;
  }

  if (!payments_->SetFromJson(url_response.body)) {
    BLOG(0, "Failed to parse payment balance: " << url_response.body);
    OnAdRewards(FAILED);
    return;
  }

  GetAdGrants();
}

void AdRewards::GetAdGrants() {
  BLOG(1, "GetAdGrants");
  BLOG(2, "GET /v1/promotions/ads/grants/summary?paymentId={payment_id}");

  AdGrantsUrlRequestBuilder url_request_builder(wallet_info_);
  const UrlRequest url_request = url_request_builder.Build();
  BLOG(5, UrlRequestToString(url_request));

  auto callback = std::bind(&AdRewards::OnGetAdGrants, this, _1);
  ads_->get_ads_client()->UrlRequest(url_request, callback);
}

void AdRewards::OnGetAdGrants(
    const UrlResponse& url_response) {
  BLOG(1, "OnGetGrants");

  BLOG(6, UrlResponseToString(url_response));

  if (url_response.status_code == net::HTTP_NO_CONTENT) {
    ad_grants_ = std::make_unique<AdGrants>();

    OnAdRewards(SUCCESS);
    return;
  }

  if (url_response.status_code != net::HTTP_OK) {
    BLOG(1, "Failed to get ad grants");
    OnAdRewards(FAILED);
    return;
  }

  if (!ad_grants_->SetFromJson(url_response.body)) {
    BLOG(0, "Failed to parse ad grants: " << url_response.body);
    OnAdRewards(FAILED);
    return;
  }

  OnAdRewards(SUCCESS);
}

void AdRewards::OnAdRewards(
    const Result result) {
  if (result != SUCCESS) {
    BLOG(1, "Failed to get ad rewards");

    const base::Time time = retry_timer_.StartWithPrivacy(
        kRetryAdRewardsAfterSeconds, base::BindOnce(&AdRewards::Retry,
            base::Unretained(this)));

    BLOG(1, "Retry getting ad grants " << FriendlyDateAndTime(time));

    return;
  }

  BLOG(1, "Successfully retrieved ad rewards");

  retry_timer_.Stop();

  Update();
}

void AdRewards::Retry() {
  BLOG(1, "Retrying getting ad rewards");

  GetPayments();
}

void AdRewards::Update() {
  auto estimated_pending_rewards = CalculateEstimatedPendingRewards();

  auto now = base::Time::Now();
  auto next_payment_date = payments_->CalculateNextPaymentDate(now,
      ads_->GetNextTokenRedemptionDateInSeconds());
  uint64_t next_payment_date_in_seconds =
      static_cast<uint64_t>(next_payment_date.ToDoubleT());

  ads_->UpdateAdsRewards(estimated_pending_rewards,
      next_payment_date_in_seconds);
}

double AdRewards::CalculateEstimatedPendingRewards() const {
  auto estimated_pending_rewards = payments_->GetBalance();

  estimated_pending_rewards -= ad_grants_->GetBalance();
  if (estimated_pending_rewards < 0.0) {
    estimated_pending_rewards = 0.0;
  }

  return estimated_pending_rewards;
}

}  // namespace ads
