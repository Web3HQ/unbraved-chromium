/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_ads/core/internal/legacy_migration/confirmations/legacy_confirmation_migration_confirmations_json_parser.h"

#include "base/test/task_environment.h"
#include "brave/components/brave_ads/core/internal/account/confirmations/confirmation_info.h"
#include "brave/components/brave_ads/core/internal/common/test/test_constants.h"
#include "brave/components/brave_ads/core/mojom/brave_ads.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

// npm run test -- brave_unit_tests --filter=BraveAds*

namespace brave_ads::json::reader {

class BraveAdsLegacyConfirmationMigrationConfirmationsJsonParserTest
    : public ::testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

namespace {

// Minimal non-reward confirmation JSON fragment for use in tests.
constexpr char kConfirmationJson[] = R"JSON({
  "confirmations": {
    "queue": [
      {
        "transaction_id": "8b742869-6e4a-490c-ac31-31b49130098a",
        "creative_instance_id": "546fe7b0-5047-4f28-a11c-81f14edcf0f6",
        "type": "view",
        "ad_type": "ad_notification",
        "created_at": 23317004800000000.0,
        "user_data": {}
      }
    ]
  }
})JSON";

}  // namespace

TEST_F(BraveAdsLegacyConfirmationMigrationConfirmationsJsonParserTest,
       ParseConfirmations) {
  // Act
  std::optional<ConfirmationList> confirmations =
      ParseConfirmations(kConfirmationJson);
  ASSERT_TRUE(confirmations);

  // Assert
  ASSERT_EQ(1U, confirmations->size());
  EXPECT_EQ("8b742869-6e4a-490c-ac31-31b49130098a",
            (*confirmations)[0].transaction_id);
  EXPECT_EQ("546fe7b0-5047-4f28-a11c-81f14edcf0f6",
            (*confirmations)[0].creative_instance_id);
  EXPECT_EQ(mojom::ConfirmationType::kViewedImpression,
            (*confirmations)[0].type);
  EXPECT_EQ(mojom::AdType::kNotificationAd, (*confirmations)[0].ad_type);
  EXPECT_FALSE((*confirmations)[0].reward.has_value());
}

TEST_F(BraveAdsLegacyConfirmationMigrationConfirmationsJsonParserTest,
       ParseConfirmationsSkipsLegacyInlineContentAdType) {
  // Act
  std::optional<ConfirmationList> confirmations = ParseConfirmations(R"JSON({
    "confirmations": {
      "queue": [
        {
          "transaction_id": "8b742869-6e4a-490c-ac31-31b49130098a",
          "creative_instance_id": "546fe7b0-5047-4f28-a11c-81f14edcf0f6",
          "type": "view",
          "ad_type": "inline_content_ad",
          "created_at": 23317004800000000.0,
          "user_data": {}
        }
      ]
    }
  })JSON");
  ASSERT_TRUE(confirmations);

  // Assert
  EXPECT_TRUE(confirmations->empty());
}

TEST_F(BraveAdsLegacyConfirmationMigrationConfirmationsJsonParserTest,
       ParseConfirmationsSkipsLegacyPromotedContentAdType) {
  // Act
  std::optional<ConfirmationList> confirmations = ParseConfirmations(R"JSON({
    "confirmations": {
      "queue": [
        {
          "transaction_id": "8b742869-6e4a-490c-ac31-31b49130098a",
          "creative_instance_id": "546fe7b0-5047-4f28-a11c-81f14edcf0f6",
          "type": "view",
          "ad_type": "promoted_content_ad",
          "created_at": 23317004800000000.0,
          "user_data": {}
        }
      ]
    }
  })JSON");
  ASSERT_TRUE(confirmations);

  // Assert
  EXPECT_TRUE(confirmations->empty());
}

TEST_F(BraveAdsLegacyConfirmationMigrationConfirmationsJsonParserTest,
       ParseConfirmationsSkipsMissingTransactionId) {
  // Act
  std::optional<ConfirmationList> confirmations = ParseConfirmations(R"JSON({
    "confirmations": {
      "queue": [
        {
          "creative_instance_id": "546fe7b0-5047-4f28-a11c-81f14edcf0f6",
          "type": "view",
          "ad_type": "ad_notification",
          "created_at": 23317004800000000.0,
          "user_data": {}
        }
      ]
    }
  })JSON");
  ASSERT_TRUE(confirmations);

  // Assert
  EXPECT_TRUE(confirmations->empty());
}

TEST_F(BraveAdsLegacyConfirmationMigrationConfirmationsJsonParserTest,
       ParseConfirmationsSkipsMissingCreativeInstanceId) {
  // Act
  std::optional<ConfirmationList> confirmations = ParseConfirmations(R"JSON({
    "confirmations": {
      "queue": [
        {
          "transaction_id": "8b742869-6e4a-490c-ac31-31b49130098a",
          "type": "view",
          "ad_type": "ad_notification",
          "created_at": 23317004800000000.0,
          "user_data": {}
        }
      ]
    }
  })JSON");
  ASSERT_TRUE(confirmations);

  // Assert
  EXPECT_TRUE(confirmations->empty());
}

TEST_F(BraveAdsLegacyConfirmationMigrationConfirmationsJsonParserTest,
       ParseConfirmationsSkipsMissingType) {
  // Act
  std::optional<ConfirmationList> confirmations = ParseConfirmations(R"JSON({
    "confirmations": {
      "queue": [
        {
          "transaction_id": "8b742869-6e4a-490c-ac31-31b49130098a",
          "creative_instance_id": "546fe7b0-5047-4f28-a11c-81f14edcf0f6",
          "ad_type": "ad_notification",
          "created_at": 23317004800000000.0,
          "user_data": {}
        }
      ]
    }
  })JSON");
  ASSERT_TRUE(confirmations);

  // Assert
  EXPECT_TRUE(confirmations->empty());
}

TEST_F(BraveAdsLegacyConfirmationMigrationConfirmationsJsonParserTest,
       ParseConfirmationsSkipsMissingAdType) {
  // Act
  std::optional<ConfirmationList> confirmations = ParseConfirmations(R"JSON({
    "confirmations": {
      "queue": [
        {
          "transaction_id": "8b742869-6e4a-490c-ac31-31b49130098a",
          "creative_instance_id": "546fe7b0-5047-4f28-a11c-81f14edcf0f6",
          "type": "view",
          "created_at": 23317004800000000.0,
          "user_data": {}
        }
      ]
    }
  })JSON");
  ASSERT_TRUE(confirmations);

  // Assert
  EXPECT_TRUE(confirmations->empty());
}

TEST_F(BraveAdsLegacyConfirmationMigrationConfirmationsJsonParserTest,
       ParseConfirmationsSkipsMissingCreatedAt) {
  // Act
  std::optional<ConfirmationList> confirmations = ParseConfirmations(R"JSON({
    "confirmations": {
      "queue": [
        {
          "transaction_id": "8b742869-6e4a-490c-ac31-31b49130098a",
          "creative_instance_id": "546fe7b0-5047-4f28-a11c-81f14edcf0f6",
          "type": "view",
          "ad_type": "ad_notification",
          "user_data": {}
        }
      ]
    }
  })JSON");
  ASSERT_TRUE(confirmations);

  // Assert
  EXPECT_TRUE(confirmations->empty());
}

TEST_F(BraveAdsLegacyConfirmationMigrationConfirmationsJsonParserTest,
       ParseConfirmationsSkipsMissingUserData) {
  // Act
  std::optional<ConfirmationList> confirmations = ParseConfirmations(R"JSON({
    "confirmations": {
      "queue": [
        {
          "transaction_id": "8b742869-6e4a-490c-ac31-31b49130098a",
          "creative_instance_id": "546fe7b0-5047-4f28-a11c-81f14edcf0f6",
          "type": "view",
          "ad_type": "ad_notification",
          "created_at": 23317004800000000.0
        }
      ]
    }
  })JSON");
  ASSERT_TRUE(confirmations);

  // Assert
  EXPECT_TRUE(confirmations->empty());
}

TEST_F(BraveAdsLegacyConfirmationMigrationConfirmationsJsonParserTest,
       DoNotParseConfirmationsIfMissingConfirmationsKey) {
  // Act & Assert
  EXPECT_FALSE(ParseConfirmations(R"JSON({"other_key": {}})JSON"));
}

TEST_F(BraveAdsLegacyConfirmationMigrationConfirmationsJsonParserTest,
       DoNotParseConfirmationsIfMissingQueueKey) {
  // Act & Assert
  EXPECT_FALSE(ParseConfirmations(R"JSON({"confirmations": {}})JSON"));
}

TEST_F(BraveAdsLegacyConfirmationMigrationConfirmationsJsonParserTest,
       DoNotParseConfirmationsIfMalformedJson) {
  // Act & Assert
  EXPECT_FALSE(ParseConfirmations(test::kMalformedJson));
}

TEST_F(BraveAdsLegacyConfirmationMigrationConfirmationsJsonParserTest,
       ParseEmptyConfirmationQueue) {
  // Act
  std::optional<ConfirmationList> confirmations =
      ParseConfirmations(R"JSON({"confirmations": {"queue": []}})JSON");
  ASSERT_TRUE(confirmations);

  // Assert
  EXPECT_TRUE(confirmations->empty());
}

}  // namespace brave_ads::json::reader
