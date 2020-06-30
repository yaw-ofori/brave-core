/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BAT_ADS_INTERNAL_SERVER_GET_AD_REWARDS_GET_AD_REWARDS_H_
#define BAT_ADS_INTERNAL_SERVER_GET_AD_REWARDS_GET_AD_REWARDS_H_

#include <memory>

#include "base/values.h"
#include "bat/ads/internal/backoff_timer.h"
#include "bat/ads/internal/server/ad_rewards/ad_grants/ad_grants.h"
#include "bat/ads/internal/server/ad_rewards/payments/payments.h"
#include "bat/ads/mojom.h"
#include "bat/ads/result.h"
#include "bat/ads/wallet_info.h"

namespace ads {

class AdsImpl;

class AdRewards {
 public:
  AdRewards(
      AdsImpl* ads);

  ~AdRewards();

  void Update(
      const WalletInfo& wallet_info,
      const bool should_reconcile_with_server);

  base::Value GetAsDictionary();
  bool SetFromDictionary(
      base::DictionaryValue* dictionary);

 private:
  WalletInfo wallet_info_;

  void GetPayments();
  void OnGetPayments(
      const UrlResponse& url_response);

  void GetAdGrants();
  void OnGetAdGrants(
      const UrlResponse& url_response);

  void OnAdRewards(
      const Result result);

  BackoffTimer retry_timer_;
  void Retry();

  void Update();
  double CalculateEstimatedPendingRewards() const;

  AdsImpl* ads_;  // NOT OWNED

  std::unique_ptr<AdGrants> ad_grants_;
  std::unique_ptr<Payments> payments_;
};

}  // namespace ads

#endif  // BAT_ADS_INTERNAL_SERVER_GET_AD_REWARDS_GET_AD_REWARDS_H_
