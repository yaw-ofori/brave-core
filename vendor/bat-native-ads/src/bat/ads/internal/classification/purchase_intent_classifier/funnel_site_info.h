/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BAT_ADS_INTERNAL_CLASSIFICATION_PURCHASE_INTENT_CLASSIFIER_FUNNEL_SITE_INFO_H_
#define BAT_ADS_INTERNAL_CLASSIFICATION_PURCHASE_INTENT_CLASSIFIER_FUNNEL_SITE_INFO_H_

#include <stdint.h>

#include <string>

#include "bat/ads/internal/classification/purchase_intent_classifier/segment_keyword_info.h"

namespace ads {
namespace classification {

struct FunnelSiteInfo {
 public:
  FunnelSiteInfo();
  FunnelSiteInfo(
      const PurchaseIntentSegmentList& segments,
      const std::string& url_netloc,
      const uint16_t weight);
  FunnelSiteInfo(
      const FunnelSiteInfo& info);
  ~FunnelSiteInfo();

  bool operator==(
      const FunnelSiteInfo& rhs) const;
  bool operator!=(
      const FunnelSiteInfo& rhs) const;

  PurchaseIntentSegmentList segments;
  std::string url_netloc;
  uint16_t weight = 0;
};

}  // namespace classification
}  // namespace ads

#endif  // BAT_ADS_INTERNAL_CLASSIFICATION_PURCHASE_INTENT_CLASSIFIER_FUNNEL_SITE_INFO_H_
