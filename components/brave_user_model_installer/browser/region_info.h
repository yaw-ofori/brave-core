/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_COMPONENTS_BRAVE_USER_MODEL_INSTALLER_BROWSER_REGION_INFO_H_
#define BRAVE_COMPONENTS_BRAVE_USER_MODEL_INSTALLER_BROWSER_REGION_INFO_H_

#include <string>

namespace brave_user_model_installer {

struct RegionInfo {
  RegionInfo();
  RegionInfo(
      const std::string& component_base64_public_key,
      const std::string& component_id);
  ~RegionInfo();

  std::string component_base64_public_key;
  std::string component_id;
};

}  // namespace brave_user_model_installer

#endif  // BRAVE_COMPONENTS_BRAVE_USER_MODEL_INSTALLER_BROWSER_REGION_INFO_H_
