// Copyright (c) 2024 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "brave/components/brave_shields/core/browser/ad_block_filters_provider_manager.h"

#include <string>

#include "base/hash/hash.h"
#include "base/strings/string_number_conversions.h"
#include "brave/components/brave_shields/content/test/test_filters_provider.h"
#include "brave/components/brave_shields/core/browser/ad_block_filters_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::string HashOf(std::string_view content) {
  return base::NumberToString(base::FastHash(std::string(content)));
}

}  // namespace

class FiltersProviderManagerTestObserver
    : public brave_shields::AdBlockFiltersProvider::Observer {
 public:
  FiltersProviderManagerTestObserver() = default;

  // AdBlockFiltersProvider::Observer
  void OnChanged(bool is_default_engine) override { changed_count += 1; }

  int changed_count = 0;
};

TEST(AdBlockFiltersProviderManagerTest, WaitUntilInitialized) {
  FiltersProviderManagerTestObserver test_observer;
  brave_shields::AdBlockFiltersProviderManager m;
  m.AddObserver(&test_observer);

  brave_shields::TestFiltersProvider provider1("", true, 0);
  EXPECT_EQ(test_observer.changed_count, 0);
  provider1.RegisterAsSourceProvider(&m);
  EXPECT_EQ(test_observer.changed_count, 1);
  brave_shields::TestFiltersProvider provider2("", true, 0);
  EXPECT_EQ(test_observer.changed_count, 1);
  provider2.RegisterAsSourceProvider(&m);
  EXPECT_EQ(test_observer.changed_count, 2);
}

TEST(AdBlockFiltersProviderManagerTest, ForceNotifyObserverCombinesHashes) {
  brave_shields::AdBlockFiltersProviderManager m;

  // Create providers with specific content hashes
  brave_shields::TestFiltersProvider provider1("rules_a", true, 0);
  provider1.RegisterAsSourceProvider(&m);

  brave_shields::TestFiltersProvider provider2("rules_b", true, 0);
  provider2.RegisterAsSourceProvider(&m);

  // Create a separate observer for ForceNotifyObserver
  FiltersProviderManagerTestObserver force_observer;
  m.ForceNotifyObserver(force_observer, true);

  // Should have been notified once
  EXPECT_EQ(force_observer.changed_count, 1);

  // The combined hash should be the sorted, pipe-joined hashes
  std::string hash_a = HashOf("rules_a");
  std::string hash_b = HashOf("rules_b");
  std::string expected;
  if (hash_a < hash_b) {
    expected = hash_a + "|" + hash_b;
  } else {
    expected = hash_b + "|" + hash_a;
  }
  EXPECT_EQ(m.ComputeCombinedHash(true).value(), expected);
}

TEST(AdBlockFiltersProviderManagerTest,
     ForceNotifyObserverDoesNotNotifyWhenNoProviders) {
  brave_shields::AdBlockFiltersProviderManager m;

  FiltersProviderManagerTestObserver observer;
  m.ForceNotifyObserver(observer, true);

  // Should not be notified when there are no providers
  EXPECT_EQ(observer.changed_count, 0);
}

TEST(AdBlockFiltersProviderManagerTest, ForceNotifyObserverRespectsEngineType) {
  brave_shields::AdBlockFiltersProviderManager m;

  // Create a default engine provider
  brave_shields::TestFiltersProvider default_provider("default_rules", true, 0);
  default_provider.RegisterAsSourceProvider(&m);

  // Create an additional engine provider
  brave_shields::TestFiltersProvider additional_provider("additional_rules",
                                                         false, 0);
  additional_provider.RegisterAsSourceProvider(&m);

  // Observer for default engine only
  FiltersProviderManagerTestObserver default_observer;
  m.ForceNotifyObserver(default_observer, true);
  EXPECT_EQ(default_observer.changed_count, 1);

  // Observer for additional engine only
  FiltersProviderManagerTestObserver additional_observer;
  m.ForceNotifyObserver(additional_observer, false);
  EXPECT_EQ(additional_observer.changed_count, 1);
}

TEST(AdBlockFiltersProviderManagerTest, OnChangedCombinesProviderHashes) {
  brave_shields::AdBlockFiltersProviderManager m;

  FiltersProviderManagerTestObserver observer;
  m.AddObserver(&observer);

  // Create and initialize a provider
  brave_shields::TestFiltersProvider provider("test_rules", true, 0);
  provider.RegisterAsSourceProvider(&m);

  EXPECT_EQ(observer.changed_count, 1);
  // With a single provider, combined hash is just that provider's hash
  EXPECT_EQ(m.ComputeCombinedHash(true).value(), HashOf("test_rules"));
}
