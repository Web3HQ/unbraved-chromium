// Copyright (c) 2026 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "brave/components/brave_shields/core/browser/ad_block_component_filters_provider.h"

#include <string>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "brave/components/brave_shields/core/browser/ad_block_filters_provider.h"
#include "brave/components/brave_shields/core/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "brave/components/brave_shields/core/browser/ad_block_filters_provider_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class HashObserver : public brave_shields::AdBlockFiltersProvider::Observer {
 public:
  void OnChanged(bool is_default_engine) override { notified_ = true; }

  bool notified() const { return notified_; }
  void reset() { notified_ = false; }

 private:
  bool notified_ = false;
};

}  // namespace

class AdBlockComponentFiltersProviderTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(temp_dir2_.CreateUniqueTempDir());
    prefs_.registry()->RegisterDictionaryPref(
        brave_shields::prefs::kAdBlockComponentFiltersCacheHash);
  }

  static void SimulateComponentReady(
      brave_shields::AdBlockComponentFiltersProvider& provider,
      const base::FilePath& path) {
    provider.OnComponentReady(path);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  base::ScopedTempDir temp_dir2_;
  TestingPrefServiceSimple prefs_;
};

TEST_F(AdBlockComponentFiltersProviderTest,
       DifferentComponentsHaveSeparateHashes) {
  brave_shields::AdBlockFiltersProviderManager manager;

  // Set up two components in different directories with different files.
  base::FilePath list_a = temp_dir_.GetPath().AppendASCII("list.txt");
  ASSERT_TRUE(base::WriteFile(list_a, "||from-a.com^"));

  base::FilePath list_b = temp_dir2_.GetPath().AppendASCII("list.txt");
  ASSERT_TRUE(base::WriteFile(list_b, "||from-b.com^"));

  HashObserver observer_a;
  brave_shields::AdBlockComponentFiltersProvider provider_a(
      nullptr, &manager, "component_a", "", "Component A", 0, &prefs_,
      /*is_default_engine=*/true);
  provider_a.AddObserver(&observer_a);
  SimulateComponentReady(provider_a, temp_dir_.GetPath());
  ASSERT_TRUE(base::test::RunUntil([&]() { return observer_a.notified(); }));

  HashObserver observer_b;
  brave_shields::AdBlockComponentFiltersProvider provider_b(
      nullptr, &manager, "component_b", "", "Component B", 0, &prefs_,
      /*is_default_engine=*/false);
  provider_b.AddObserver(&observer_b);
  SimulateComponentReady(provider_b, temp_dir2_.GetPath());
  ASSERT_TRUE(base::test::RunUntil([&]() { return observer_b.notified(); }));

  // Each component should have its own content hash.
  ASSERT_TRUE(provider_a.GetContentHash().has_value());
  ASSERT_TRUE(provider_b.GetContentHash().has_value());
  std::string hash_a = provider_a.GetContentHash().value();
  std::string hash_b = provider_b.GetContentHash().value();

  EXPECT_FALSE(hash_a.empty());
  EXPECT_FALSE(hash_b.empty());
  EXPECT_NE(hash_a, hash_b);

  // Updating component A should change its hash but not component B's.
  std::string original_hash_b = hash_b;
  ASSERT_TRUE(base::WriteFile(list_a, "||updated-a.com^"));
  observer_a.reset();
  SimulateComponentReady(provider_a, temp_dir_.GetPath());
  ASSERT_TRUE(base::test::RunUntil([&]() { return observer_a.notified(); }));

  EXPECT_NE(provider_a.GetContentHash().value(), hash_a);
  EXPECT_EQ(provider_b.GetContentHash().value(), original_hash_b);

  provider_a.RemoveObserver(&observer_a);
  provider_b.RemoveObserver(&observer_b);
}
