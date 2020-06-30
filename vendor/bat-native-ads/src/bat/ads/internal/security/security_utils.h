/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BAT_ADS_INTERNAL_SECURITY_SECURITY_UTILS_H_
#define BAT_ADS_INTERNAL_SECURITY_SECURITY_UTILS_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

namespace ads {
namespace security {

std::string Sign(
    const std::map<std::string, std::string>& headers,
    const std::string& key_id,
    const std::vector<uint8_t>& private_key);

std::vector<uint8_t> Sha256Hash(
    const std::string& string);

}  // namespace security
}  // namespace ads

#endif  // BAT_ADS_INTERNAL_SECURITY_SECURITY_UTILS_H_
