/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BAT_ADS_INTERNAL_CREATE_CONFIRMATION_REQUEST_H_
#define BAT_ADS_INTERNAL_CREATE_CONFIRMATION_REQUEST_H_

#include <string>
#include <vector>

#include "wrapper.hpp"  // NOLINT
#include "bat/ads/mojom.h"

namespace ads {

class AdsImpl;
struct ConfirmationInfo;
struct UnblindedTokenInfo;

using challenge_bypass_ristretto::BlindedToken;

class CreateConfirmationRequest {
 public:
  CreateConfirmationRequest(
      AdsImpl* ads);

  ~CreateConfirmationRequest();

  std::string BuildUrl(
    const std::string& confirmation_id,
    const std::string& credential) const;

  UrlRequestMethod GetMethod() const;

  std::string BuildBody(
      const std::string& payload) const;

  std::vector<std::string> BuildHeaders() const;
  std::string GetAcceptHeaderValue() const;

  std::string GetContentType() const;

  std::string CreateConfirmationRequestDTO(
      const ConfirmationInfo& info,
      const std::string& build_channel,
      const std::string& platform,
      const std::string& country_code) const;

  std::string CreateCredential(
      const privacy::UnblindedTokenInfo& unblinded_token,
      const std::string& payload) const;

 private:
  bool IsLargeAnonymityCountryCode(
      const std::string& country_code) const;

  bool IsOtherCountryCode(
      const std::string& country_code) const;

  AdsImpl* ads_;  // NOT OWNED
};

}  // namespace ads

#endif  // BAT_ADS_INTERNAL_CREATE_CONFIRMATION_REQUEST_H_
