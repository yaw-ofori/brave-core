/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_user_model_installer/browser/regions.h"

namespace brave_user_model_installer {

base::Optional<RegionInfo> GetRegionInfo(
    const std::string& country_code) {
  const auto iter = regions.find(country_code);
  if (iter == regions.end()) {
    return base::nullopt;
  }

  return iter->second;
}

}  // namespace brave_user_model_installer
