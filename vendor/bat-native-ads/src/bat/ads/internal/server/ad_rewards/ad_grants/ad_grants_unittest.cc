/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "bat/ads/internal/rewards/grants/grants.h"

// npm run test -- brave_unit_tests --filter=BatAds*

namespace ads {

TEST(BatAdsGrantsTest,
    InvalidJson) {
  // Arrange
  const std::string json = "{FOOBAR}";

  Grants grants;

  // Act
  const bool is_valid = grants.SetFromJson(json);

  // Assert
  EXPECT_FALSE(is_valid);
}

TEST(BatAdsGrantsTest,
    DoubleForAmount) {
  // Arrange
  const std::string json = R"(
    {
      "type" : "ads",
      "amount" : "5.0",
      "lastClaim" : "2019-06-13T12:14:46.150Z"
    }
  )";

  Grants grants;
  grants.SetFromJson(json);

  // Act
  const double balance = grants.GetBalance();

  // Assert
  EXPECT_EQ(5.0, balance);
}

TEST(BatAdsGrantsTest,
    IntegerForAmount) {
  // Arrange
  const std::string json = R"(
    {
      "type" : "ads",
      "amount" : "5",
      "lastClaim" : "2019-06-13T12:14:46.150Z"
    }
  )";

  Grants grants;
  grants.SetFromJson(json);

  // Act
  const double balance = grants.GetBalance();

  // Assert
  EXPECT_EQ(5.0, balance);
}

TEST(BatAdsGrantsTest,
     InvalidStringForAmount) {
  // Arrange
  const std::string json = R"(
    {
      "type" : "ads",
      "amount" : "INVALID",
      "lastClaim" : "2019-06-13T12:14:46.150Z"
    }
  )";

  Grants grants;
  grants.SetFromJson(json);

  // Act
  const double balance = grants.GetBalance();

  // Assert
  EXPECT_EQ(0.0, balance);
}

TEST(BatAdsGrantsTest,
    InvalidTypeForAmount) {
  // Arrange
  const std::string json = R"(
    {
      "type" : "ads",
      "amount" : 1,
      "lastClaim" : "2019-06-13T12:14:46.150Z"
    }
  )";

  Grants grants;
  grants.SetFromJson(json);

  // Act
  const double balance = grants.GetBalance();

  // Assert
  EXPECT_EQ(0.0, balance);
}

}  // namespace ads
