/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_ads/core/internal/legacy_migration/confirmations/legacy_confirmation_migration_confirmation_tokens_json_parser.h"

#include "base/test/task_environment.h"
#include "brave/components/brave_ads/core/internal/account/tokens/confirmation_tokens/confirmation_token_info.h"
#include "brave/components/brave_ads/core/internal/account/wallet/test/wallet_test_util.h"
#include "brave/components/brave_ads/core/internal/account/wallet/wallet_info.h"
#include "brave/components/brave_ads/core/internal/common/test/test_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

// npm run test -- brave_unit_tests --filter=BraveAds*

namespace brave_ads::json::reader {

class BraveAdsLegacyConfirmationMigrationConfirmationTokensJsonParserTest
    : public ::testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(BraveAdsLegacyConfirmationMigrationConfirmationTokensJsonParserTest,
       ParseConfirmationTokens) {
  // Act
  std::optional<ConfirmationTokenList> confirmation_tokens =
      ParseConfirmationTokens(R"JSON({
    "unblinded_tokens": [
      {
        "unblinded_token": "IXDCnZnVEJ0orkbZfr2ut2NQPQ0ofdervKBmQ2hyjcClGCjA3/ExbBumO0ua5cxwo//nN0uKQ60kknru8hRXx0DWhwHwuFlxmot8WgVbnQ0XtPx7q9BG0jbI00AJStwN",
        "public_key": "QnShwT9vRebch3WDu28nqlTaNCU5MaOF1n4VV4Q3K1g=",
        "signature": "QDgYBeSL9ZWx8+/EHa3x3/LjVaQfiVu1ix7pnlTEgAzJw4Sc1e48Jyrj0/p/+NPqk8udleb4jZWlWDjy+m7fCA=="
      },
      {
        "unblinded_token": "hfrMEltWLuzbKQ02Qixh5C/DWiJbdOoaGaidKZ7Mv+cRq5fyxJqemE/MPlARPhl6NgXPHUeyaxzd6/Lk6YHlfXbBA023DYvGMHoKm15NP/nWnZ1V3iLkgOOHZuk80Z4K",
        "public_key": "QnShwT9vRebch3WDu28nqlTaNCU5MaOF1n4VV4Q3K1g=",
        "signature": "WeBTGGAvueivHOo33UKGTgDRw7fF/Hp9+tNZYDlUjc9CIKt/+ksh4X+mVxSMXc2E1chUWqUDME7DFFuDhasmCg=="
      }
    ]
  })JSON",
                              test::Wallet());
  ASSERT_TRUE(confirmation_tokens);

  // Assert
  ASSERT_EQ(2U, confirmation_tokens->size());
  EXPECT_TRUE((*confirmation_tokens)[0].unblinded_token.has_value());
  EXPECT_TRUE((*confirmation_tokens)[0].public_key.has_value());
  EXPECT_EQ(
      "QDgYBeSL9ZWx8+/EHa3x3/LjVaQfiVu1ix7pnlTEgAzJw4Sc1e48Jyrj0/p/"
      "+NPqk8udleb4jZWlWDjy+m7fCA==",
      (*confirmation_tokens)[0].signature_base64);
  EXPECT_TRUE((*confirmation_tokens)[1].unblinded_token.has_value());
  EXPECT_TRUE((*confirmation_tokens)[1].public_key.has_value());
  EXPECT_EQ(
      "WeBTGGAvueivHOo33UKGTgDRw7fF/Hp9+tNZYDlUjc9CIKt/+ksh4X+mVxSMXc2E1chUW"
      "qUDME7DFFuDhasmCg==",
      (*confirmation_tokens)[1].signature_base64);
}

TEST_F(BraveAdsLegacyConfirmationMigrationConfirmationTokensJsonParserTest,
       ParseConfirmationTokensSkipsInvalidSignature) {
  // Act
  std::optional<ConfirmationTokenList> confirmation_tokens =
      ParseConfirmationTokens(R"JSON({
    "unblinded_tokens": [
      {
        "unblinded_token": "IXDCnZnVEJ0orkbZfr2ut2NQPQ0ofdervKBmQ2hyjcClGCjA3/ExbBumO0ua5cxwo//nN0uKQ60kknru8hRXx0DWhwHwuFlxmot8WgVbnQ0XtPx7q9BG0jbI00AJStwN",
        "public_key": "QnShwT9vRebch3WDu28nqlTaNCU5MaOF1n4VV4Q3K1g=",
        "signature": "V7Gilxl5TNv7pTqq8Sftmu+O+HgJ44Byn8PhDkDIwnsgncGiCduuoRMNagUnN7AXalaQdy1GedKj5thKFeyUcQ=="
      }
    ]
  })JSON",
                              test::Wallet());
  ASSERT_TRUE(confirmation_tokens);

  // Assert
  EXPECT_TRUE(confirmation_tokens->empty());
}

TEST_F(BraveAdsLegacyConfirmationMigrationConfirmationTokensJsonParserTest,
       ParseConfirmationTokensSkipsInvalidUnblindedToken) {
  // Act
  std::optional<ConfirmationTokenList> confirmation_tokens =
      ParseConfirmationTokens(R"JSON({
    "unblinded_tokens": [
      {
        "unblinded_token": "INVALID_TOKEN",
        "public_key": "QnShwT9vRebch3WDu28nqlTaNCU5MaOF1n4VV4Q3K1g=",
        "signature": "V7Gilxl5TNv7pTqq8Sftmu+O+HgJ44Byn8PhDkDIwnsgncGiCduuoRMNagUnN7AXalaQdy1GedKj5thKFeyUcQ=="
      }
    ]
  })JSON",
                              test::Wallet());
  ASSERT_TRUE(confirmation_tokens);

  // Assert
  EXPECT_TRUE(confirmation_tokens->empty());
}

TEST_F(BraveAdsLegacyConfirmationMigrationConfirmationTokensJsonParserTest,
       ParseConfirmationTokensSkipsInvalidPublicKey) {
  // Act
  std::optional<ConfirmationTokenList> confirmation_tokens =
      ParseConfirmationTokens(R"JSON({
    "unblinded_tokens": [
      {
        "unblinded_token": "IXDCnZnVEJ0orkbZfr2ut2NQPQ0ofdervKBmQ2hyjcClGCjA3/ExbBumO0ua5cxwo//nN0uKQ60kknru8hRXx0DWhwHwuFlxmot8WgVbnQ0XtPx7q9BG0jbI00AJStwN",
        "public_key": "INVALID_KEY",
        "signature": "V7Gilxl5TNv7pTqq8Sftmu+O+HgJ44Byn8PhDkDIwnsgncGiCduuoRMNagUnN7AXalaQdy1GedKj5thKFeyUcQ=="
      }
    ]
  })JSON",
                              test::Wallet());
  ASSERT_TRUE(confirmation_tokens);

  // Assert
  EXPECT_TRUE(confirmation_tokens->empty());
}

TEST_F(BraveAdsLegacyConfirmationMigrationConfirmationTokensJsonParserTest,
       ParseConfirmationTokensSkipsMissingUnblindedToken) {
  // Act
  std::optional<ConfirmationTokenList> confirmation_tokens =
      ParseConfirmationTokens(R"JSON({
    "unblinded_tokens": [
      {
        "public_key": "QnShwT9vRebch3WDu28nqlTaNCU5MaOF1n4VV4Q3K1g=",
        "signature": "V7Gilxl5TNv7pTqq8Sftmu+O+HgJ44Byn8PhDkDIwnsgncGiCduuoRMNagUnN7AXalaQdy1GedKj5thKFeyUcQ=="
      }
    ]
  })JSON",
                              test::Wallet());
  ASSERT_TRUE(confirmation_tokens);

  // Assert
  EXPECT_TRUE(confirmation_tokens->empty());
}

TEST_F(BraveAdsLegacyConfirmationMigrationConfirmationTokensJsonParserTest,
       ParseConfirmationTokensSkipsMissingPublicKey) {
  // Act
  std::optional<ConfirmationTokenList> confirmation_tokens =
      ParseConfirmationTokens(R"JSON({
    "unblinded_tokens": [
      {
        "unblinded_token": "IXDCnZnVEJ0orkbZfr2ut2NQPQ0ofdervKBmQ2hyjcClGCjA3/ExbBumO0ua5cxwo//nN0uKQ60kknru8hRXx0DWhwHwuFlxmot8WgVbnQ0XtPx7q9BG0jbI00AJStwN",
        "signature": "V7Gilxl5TNv7pTqq8Sftmu+O+HgJ44Byn8PhDkDIwnsgncGiCduuoRMNagUnN7AXalaQdy1GedKj5thKFeyUcQ=="
      }
    ]
  })JSON",
                              test::Wallet());
  ASSERT_TRUE(confirmation_tokens);

  // Assert
  EXPECT_TRUE(confirmation_tokens->empty());
}

TEST_F(BraveAdsLegacyConfirmationMigrationConfirmationTokensJsonParserTest,
       ParseConfirmationTokensSkipsMissingSignature) {
  // Act
  std::optional<ConfirmationTokenList> confirmation_tokens =
      ParseConfirmationTokens(R"JSON({
    "unblinded_tokens": [
      {
        "unblinded_token": "IXDCnZnVEJ0orkbZfr2ut2NQPQ0ofdervKBmQ2hyjcClGCjA3/ExbBumO0ua5cxwo//nN0uKQ60kknru8hRXx0DWhwHwuFlxmot8WgVbnQ0XtPx7q9BG0jbI00AJStwN",
        "public_key": "QnShwT9vRebch3WDu28nqlTaNCU5MaOF1n4VV4Q3K1g="
      }
    ]
  })JSON",
                              test::Wallet());
  ASSERT_TRUE(confirmation_tokens);

  // Assert
  EXPECT_TRUE(confirmation_tokens->empty());
}

TEST_F(BraveAdsLegacyConfirmationMigrationConfirmationTokensJsonParserTest,
       DoNotParseConfirmationTokensIfMissingUnblindedTokensKey) {
  // Act & Assert
  EXPECT_FALSE(
      ParseConfirmationTokens(R"JSON({"other_key": []})JSON", test::Wallet()));
}

TEST_F(BraveAdsLegacyConfirmationMigrationConfirmationTokensJsonParserTest,
       DoNotParseConfirmationTokensIfMalformedJson) {
  // Act & Assert
  EXPECT_FALSE(ParseConfirmationTokens(test::kMalformedJson, test::Wallet()));
}

TEST_F(BraveAdsLegacyConfirmationMigrationConfirmationTokensJsonParserTest,
       ParseEmptyConfirmationTokens) {
  // Act
  std::optional<ConfirmationTokenList> confirmation_tokens =
      ParseConfirmationTokens(R"JSON({"unblinded_tokens": []})JSON",
                              test::Wallet());
  ASSERT_TRUE(confirmation_tokens);

  // Assert
  EXPECT_TRUE(confirmation_tokens->empty());
}

}  // namespace brave_ads::json::reader
