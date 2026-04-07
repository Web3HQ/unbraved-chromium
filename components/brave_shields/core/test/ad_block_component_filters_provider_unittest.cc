// Copyright (c) 2026 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "brave/components/brave_shields/core/browser/ad_block_component_filters_provider.h"

#include <string>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "brave/components/brave_shields/core/browser/ad_block_filters_provider.h"
#include "brave/components/brave_shields/core/browser/ad_block_filters_provider_manager.h"
#include "brave/components/brave_shields/core/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TimestampObserver
    : public brave_shields::AdBlockFiltersProvider::Observer {
 public:
  void OnChanged(bool is_default_engine, base::Time timestamp) override {
    notified_ = true;
    last_timestamp_ = timestamp;
  }

  bool notified() const { return notified_; }
  void reset() { notified_ = false; }
  base::Time last_timestamp() const { return last_timestamp_; }

 private:
  bool notified_ = false;
  base::Time last_timestamp_;
};

std::string CachePrefPath(const std::string& component_id) {
  return base::StrCat(
      {brave_shields::prefs::kAdBlockComponentFiltersCacheTimestamp, ".",
       component_id});
}

}  // namespace

class AdBlockComponentFiltersProviderTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(temp_dir2_.CreateUniqueTempDir());
    RegisterComponentPref("component_a");
    RegisterComponentPref("component_b");
  }

  static void SimulateComponentReady(
      brave_shields::AdBlockComponentFiltersProvider& provider,
      const base::FilePath& path) {
    provider.OnComponentReady(path);
  }

 protected:
  void RegisterComponentPref(const std::string& component_id) {
    prefs_.registry()->RegisterTimePref(CachePrefPath(component_id),
                                        base::Time());
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  base::ScopedTempDir temp_dir2_;
  TestingPrefServiceSimple prefs_;
};

TEST_F(AdBlockComponentFiltersProviderTest,
       DifferentComponentsHaveSeparateTimestamps) {
  brave_shields::AdBlockFiltersProviderManager manager;

  // Set up two components in different directories with different files.
  base::FilePath list_a = temp_dir_.GetPath().AppendASCII("list.txt");
  ASSERT_TRUE(base::WriteFile(list_a, "||from-a.com^"));

  base::FilePath list_b = temp_dir2_.GetPath().AppendASCII("list.txt");
  ASSERT_TRUE(base::WriteFile(list_b, "||from-b.com^"));

  TimestampObserver observer_a;
  brave_shields::AdBlockComponentFiltersProvider provider_a(
      nullptr, &manager, "component_a", "", "Component A", 0, &prefs_,
      /*is_default_engine=*/true);
  provider_a.AddObserver(&observer_a);
  SimulateComponentReady(provider_a, temp_dir_.GetPath());
  ASSERT_TRUE(base::test::RunUntil([&]() { return observer_a.notified(); }));

  TimestampObserver observer_b;
  brave_shields::AdBlockComponentFiltersProvider provider_b(
      nullptr, &manager, "component_b", "", "Component B", 0, &prefs_,
      /*is_default_engine=*/false);
  provider_b.AddObserver(&observer_b);
  SimulateComponentReady(provider_b, temp_dir2_.GetPath());
  ASSERT_TRUE(base::test::RunUntil([&]() { return observer_b.notified(); }));

  // Each component should have its own timestamp in its own pref.
  base::Time ts_a = prefs_.GetTime(CachePrefPath("component_a"));
  base::Time ts_b = prefs_.GetTime(CachePrefPath("component_b"));

  EXPECT_NE(ts_a, base::Time());
  EXPECT_NE(ts_b, base::Time());
  EXPECT_EQ(ts_a, provider_a.timestamp());
  EXPECT_EQ(ts_b, provider_b.timestamp());

  // Updating component A should not change component B's timestamp.
  base::Time original_ts_b = ts_b;
  ASSERT_TRUE(base::WriteFile(list_a, "||updated-a.com^"));
  observer_a.reset();
  SimulateComponentReady(provider_a, temp_dir_.GetPath());
  ASSERT_TRUE(base::test::RunUntil([&]() { return observer_a.notified(); }));

  EXPECT_EQ(prefs_.GetTime(CachePrefPath("component_b")), original_ts_b);

  provider_a.RemoveObserver(&observer_a);
  provider_b.RemoveObserver(&observer_b);
}
