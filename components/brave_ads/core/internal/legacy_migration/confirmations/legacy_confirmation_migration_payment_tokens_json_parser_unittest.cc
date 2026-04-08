/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_ads/core/internal/legacy_migration/confirmations/legacy_confirmation_migration_payment_tokens_json_parser.h"

#include "base/test/task_environment.h"
#include "brave/components/brave_ads/core/internal/account/tokens/payment_tokens/payment_token_info.h"
#include "brave/components/brave_ads/core/internal/common/test/test_constants.h"
#include "brave/components/brave_ads/core/mojom/brave_ads.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

// npm run test -- brave_unit_tests --filter=BraveAds*

namespace brave_ads::json::reader {

class BraveAdsLegacyConfirmationMigrationPaymentTokensJsonParserTest
    : public ::testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(BraveAdsLegacyConfirmationMigrationPaymentTokensJsonParserTest,
       ParsePaymentTokens) {
  // Act
  std::optional<PaymentTokenList> payment_tokens = ParsePaymentTokens(R"JSON({
    "unblinded_payment_tokens": [
      {
        "transaction_id": "8b742869-6e4a-490c-ac31-31b49130098a",
        "unblinded_token": "IXDCnZnVEJ0orkbZfr2ut2NQPQ0ofdervKBmQ2hyjcClGCjA3/ExbBumO0ua5cxwo//nN0uKQ60kknru8hRXx0DWhwHwuFlxmot8WgVbnQ0XtPx7q9BG0jbI00AJStwN",
        "public_key": "QnShwT9vRebch3WDu28nqlTaNCU5MaOF1n4VV4Q3K1g=",
        "confirmation_type": "view",
        "ad_type": "ad_notification"
      },
      {
        "transaction_id": "c4ed0916-8c4d-4731-8fd6-c9a35cc0bce5",
        "unblinded_token": "IXDCnZnVEJ0orkbZfr2ut2NQPQ0ofdervKBmQ2hyjcClGCjA3/ExbBumO0ua5cxwo//nN0uKQ60kknru8hRXx0DWhwHwuFlxmot8WgVbnQ0XtPx7q9BG0jbI00AJStwN",
        "public_key": "QnShwT9vRebch3WDu28nqlTaNCU5MaOF1n4VV4Q3K1g=",
        "confirmation_type": "click",
        "ad_type": "ad_notification"
      }
    ]
  })JSON");
  ASSERT_TRUE(payment_tokens);

  // Assert
  ASSERT_EQ(2U, payment_tokens->size());
  EXPECT_EQ("8b742869-6e4a-490c-ac31-31b49130098a",
            (*payment_tokens)[0].transaction_id);
  EXPECT_TRUE((*payment_tokens)[0].unblinded_token.has_value());
  EXPECT_TRUE((*payment_tokens)[0].public_key.has_value());
  EXPECT_EQ(mojom::ConfirmationType::kViewedImpression,
            (*payment_tokens)[0].confirmation_type);
  EXPECT_EQ(mojom::AdType::kNotificationAd, (*payment_tokens)[0].ad_type);
  EXPECT_EQ("c4ed0916-8c4d-4731-8fd6-c9a35cc0bce5",
            (*payment_tokens)[1].transaction_id);
  EXPECT_EQ(mojom::ConfirmationType::kClicked,
            (*payment_tokens)[1].confirmation_type);
}

TEST_F(BraveAdsLegacyConfirmationMigrationPaymentTokensJsonParserTest,
       ParsePaymentTokensAndAssignTransactionIdWhenMissing) {
  // Act
  std::optional<PaymentTokenList> payment_tokens = ParsePaymentTokens(R"JSON({
    "unblinded_payment_tokens": [
      {
        "unblinded_token": "IXDCnZnVEJ0orkbZfr2ut2NQPQ0ofdervKBmQ2hyjcClGCjA3/ExbBumO0ua5cxwo//nN0uKQ60kknru8hRXx0DWhwHwuFlxmot8WgVbnQ0XtPx7q9BG0jbI00AJStwN",
        "public_key": "QnShwT9vRebch3WDu28nqlTaNCU5MaOF1n4VV4Q3K1g="
      }
    ]
  })JSON");
  ASSERT_TRUE(payment_tokens);

  // Assert
  ASSERT_EQ(1U, payment_tokens->size());
  EXPECT_FALSE((*payment_tokens)[0].transaction_id.empty());
}

TEST_F(BraveAdsLegacyConfirmationMigrationPaymentTokensJsonParserTest,
       ParsePaymentTokensDropsLegacyInlineContentAdType) {
  // Act
  std::optional<PaymentTokenList> payment_tokens = ParsePaymentTokens(R"JSON({
    "unblinded_payment_tokens": [
      {
        "transaction_id": "8b742869-6e4a-490c-ac31-31b49130098a",
        "unblinded_token": "IXDCnZnVEJ0orkbZfr2ut2NQPQ0ofdervKBmQ2hyjcClGCjA3/ExbBumO0ua5cxwo//nN0uKQ60kknru8hRXx0DWhwHwuFlxmot8WgVbnQ0XtPx7q9BG0jbI00AJStwN",
        "public_key": "QnShwT9vRebch3WDu28nqlTaNCU5MaOF1n4VV4Q3K1g=",
        "confirmation_type": "view",
        "ad_type": "inline_content_ad"
      }
    ]
  })JSON");
  ASSERT_TRUE(payment_tokens);

  // Assert
  EXPECT_TRUE(payment_tokens->empty());
}

TEST_F(BraveAdsLegacyConfirmationMigrationPaymentTokensJsonParserTest,
       ParsePaymentTokensDropsLegacyPromotedContentAdType) {
  // Act
  std::optional<PaymentTokenList> payment_tokens = ParsePaymentTokens(R"JSON({
    "unblinded_payment_tokens": [
      {
        "transaction_id": "8b742869-6e4a-490c-ac31-31b49130098a",
        "unblinded_token": "IXDCnZnVEJ0orkbZfr2ut2NQPQ0ofdervKBmQ2hyjcClGCjA3/ExbBumO0ua5cxwo//nN0uKQ60kknru8hRXx0DWhwHwuFlxmot8WgVbnQ0XtPx7q9BG0jbI00AJStwN",
        "public_key": "QnShwT9vRebch3WDu28nqlTaNCU5MaOF1n4VV4Q3K1g=",
        "confirmation_type": "view",
        "ad_type": "promoted_content_ad"
      }
    ]
  })JSON");
  ASSERT_TRUE(payment_tokens);

  // Assert
  EXPECT_TRUE(payment_tokens->empty());
}

TEST_F(BraveAdsLegacyConfirmationMigrationPaymentTokensJsonParserTest,
       ParsePaymentTokensSkipsMissingUnblindedToken) {
  // Act
  std::optional<PaymentTokenList> payment_tokens = ParsePaymentTokens(R"JSON({
    "unblinded_payment_tokens": [
      {
        "transaction_id": "8b742869-6e4a-490c-ac31-31b49130098a",
        "public_key": "QnShwT9vRebch3WDu28nqlTaNCU5MaOF1n4VV4Q3K1g="
      }
    ]
  })JSON");
  ASSERT_TRUE(payment_tokens);

  // Assert
  EXPECT_TRUE(payment_tokens->empty());
}

TEST_F(BraveAdsLegacyConfirmationMigrationPaymentTokensJsonParserTest,
       ParsePaymentTokensSkipsInvalidUnblindedToken) {
  // Act
  std::optional<PaymentTokenList> payment_tokens = ParsePaymentTokens(R"JSON({
    "unblinded_payment_tokens": [
      {
        "transaction_id": "8b742869-6e4a-490c-ac31-31b49130098a",
        "unblinded_token": "INVALID_TOKEN",
        "public_key": "QnShwT9vRebch3WDu28nqlTaNCU5MaOF1n4VV4Q3K1g="
      }
    ]
  })JSON");
  ASSERT_TRUE(payment_tokens);

  // Assert
  EXPECT_TRUE(payment_tokens->empty());
}

TEST_F(BraveAdsLegacyConfirmationMigrationPaymentTokensJsonParserTest,
       ParsePaymentTokensSkipsMissingPublicKey) {
  // Act
  std::optional<PaymentTokenList> payment_tokens = ParsePaymentTokens(R"JSON({
    "unblinded_payment_tokens": [
      {
        "transaction_id": "8b742869-6e4a-490c-ac31-31b49130098a",
        "unblinded_token": "IXDCnZnVEJ0orkbZfr2ut2NQPQ0ofdervKBmQ2hyjcClGCjA3/ExbBumO0ua5cxwo//nN0uKQ60kknru8hRXx0DWhwHwuFlxmot8WgVbnQ0XtPx7q9BG0jbI00AJStwN"
      }
    ]
  })JSON");
  ASSERT_TRUE(payment_tokens);

  // Assert
  EXPECT_TRUE(payment_tokens->empty());
}

TEST_F(BraveAdsLegacyConfirmationMigrationPaymentTokensJsonParserTest,
       ParsePaymentTokensSkipsInvalidPublicKey) {
  // Act
  std::optional<PaymentTokenList> payment_tokens = ParsePaymentTokens(R"JSON({
    "unblinded_payment_tokens": [
      {
        "transaction_id": "8b742869-6e4a-490c-ac31-31b49130098a",
        "unblinded_token": "IXDCnZnVEJ0orkbZfr2ut2NQPQ0ofdervKBmQ2hyjcClGCjA3/ExbBumO0ua5cxwo//nN0uKQ60kknru8hRXx0DWhwHwuFlxmot8WgVbnQ0XtPx7q9BG0jbI00AJStwN",
        "public_key": "INVALID_KEY"
      }
    ]
  })JSON");
  ASSERT_TRUE(payment_tokens);

  // Assert
  EXPECT_TRUE(payment_tokens->empty());
}

TEST_F(BraveAdsLegacyConfirmationMigrationPaymentTokensJsonParserTest,
       DoNotParsePaymentTokensIfMissingUnblindedPaymentTokensKey) {
  // Act & Assert
  EXPECT_FALSE(ParsePaymentTokens(R"JSON({"other_key": []})JSON"));
}

TEST_F(BraveAdsLegacyConfirmationMigrationPaymentTokensJsonParserTest,
       DoNotParsePaymentTokensIfMalformedJson) {
  // Act & Assert
  EXPECT_FALSE(ParsePaymentTokens(test::kMalformedJson));
}

TEST_F(BraveAdsLegacyConfirmationMigrationPaymentTokensJsonParserTest,
       ParseEmptyPaymentTokens) {
  // Act
  std::optional<PaymentTokenList> payment_tokens =
      ParsePaymentTokens(R"JSON({"unblinded_payment_tokens": []})JSON");
  ASSERT_TRUE(payment_tokens);

  // Assert
  EXPECT_TRUE(payment_tokens->empty());
}

}  // namespace brave_ads::json::reader
