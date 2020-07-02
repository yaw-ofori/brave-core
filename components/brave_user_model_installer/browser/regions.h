/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_COMPONENTS_BRAVE_USER_MODEL_INSTALLER_BROWSER_REGIONS_H_
#define BRAVE_COMPONENTS_BRAVE_USER_MODEL_INSTALLER_BROWSER_REGIONS_H_

#include <map>
#include <string>

#include "base/optional.h"
#include "brave/components/brave_user_model_installer/browser/region_info.h"

namespace brave_user_model_installer {

const std::map<std::string, RegionInfo> regions = {
  {
    "US", RegionInfo(
      "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA22Pjefa2d1B1Ms3n3554kp"
      "GQK9hgnoGgkKnGOODNB9+pwnXIbUBQ0UPNzfxUnqU16++y3JAbmDpLKswlioRrCY8Z"
      "X0uhnotU1ZfqtNd48MEPg/DqJGU37XDxa2lxSoUQq3ppGUm6j384Ma90WEAW05ZIwf"
      "e9fu1AUpO5RRoad79LG5C+Ol2HbIQQga5YJjpFuAM5KHqbXkrYZfoDOOEAoDiV4Ykm"
      "ZpmsrntB45LoX0eFaQAMkd7wSujzJ261jSRmc5fBpWni3DCWjeVMqYhv40tNAjtPqw"
      "wqXEG2p3QO3wlT5LLW6mIw/SXSgecW/fzcA7gKwMsoEIumN13j21WH8wIDAQABcchg",
      "ndhfgmkkfmhjhmdenpgdbcdjfmgh"
    )
  },
  {
    "GB", RegionInfo(
      "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA22Pjefa2d1B1Ms3n3554kp"
      "GQK9hgnoGgkKnGOODNB9+pwnXIbUBQ0UPNzfxUnqU16++y3JAbmDpLKswlioRrCY8Z"
      "X0uhnotU1ZfqtNd48MEPg/DqJGU37XDxa2lxSoUQq3ppGUm6j384Ma90WEAW05ZIwf"
      "e9fu1AUpO5RRoad79LG5C+Ol2HbIQQga5YJjpFuAM5KHqbXkrYZfoDOOEAoDiV4Ykm"
      "ZpmsrntB45LoX0eFaQAMkd7wSujzJ261jSRmc5fBpWni3DCWjeVMqYhv40tNAjtPqw"
      "wqXEG2p3QO3wlT5LLW6mIw/SXSgecW/fzcA7gKwMsoEIumN13j21WH8wIDAQABcchg",
      "ndhfgmkkfmhjhmdenpgdbcdjfmgh"
    )
  }
};

base::Optional<RegionInfo> GetRegionInfo(
    const std::string& country_code);

}  // namespace brave_user_model_installer

#endif  // BRAVE_COMPONENTS_BRAVE_USER_MODEL_INSTALLER_BROWSER_REGIONS_H_
