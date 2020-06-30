/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BAT_ADS_TRANSACTIONS_INFO_
#define BAT_ADS_TRANSACTIONS_INFO_

#include <stdint.h>

#include <string>

#include "base/values.h"
#include "bat/ads/export.h"
#include "bat/ads/transaction_info.h"

namespace ads {

struct ADS_EXPORT TransactionsInfo {
  TransactionsInfo();
  TransactionsInfo(
      const TransactionsInfo& info);
  ~TransactionsInfo();

  std::string ToJson() const;
  bool FromJson(
      const std::string& json);

  double estimated_pending_rewards = 0.0;
  uint64_t next_payment_date_in_seconds = 0;
  uint64_t ad_notifications_received_this_month = 0;
  TransactionList transactions;

 private:
  double GetEstimatedPendingRewardsFromJson(
      base::DictionaryValue* dictionary) const;

  uint64_t GetNextPaymentDateInSecondsFromJson(
      base::DictionaryValue* dictionary) const;

  uint64_t GetAdNotificationsReceivedThisMonthFromJson(
      base::DictionaryValue* dictionary) const;

  base::Value GetTransactionsAsList() const;
  TransactionList GetTransactionsFromJson(
      base::DictionaryValue* dictionary) const;
};

}  // namespace ads

#endif  // BAT_ADS_TRANSACTIONS_INFO_
