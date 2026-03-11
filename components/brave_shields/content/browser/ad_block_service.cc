// Copyright (c) 2019 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "brave/components/brave_shields/content/browser/ad_block_service.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/check_is_test.h"
#include "base/debug/leak_annotations.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/task/bind_post_task.h"
#include "base/trace_event/trace_event.h"
#include "brave/components/brave_shields/content/browser/ad_block_custom_filters_provider.h"
#include "brave/components/brave_shields/content/browser/ad_block_engine.h"
#include "brave/components/brave_shields/content/browser/ad_block_engine_wrapper.h"
#include "brave/components/brave_shields/content/browser/ad_block_localhost_filters_provider.h"
#include "brave/components/brave_shields/content/browser/ad_block_subscription_service_manager.h"
#include "brave/components/brave_shields/core/browser/ad_block_component_filters_provider.h"
#include "brave/components/brave_shields/core/browser/ad_block_component_service_manager.h"
#include "brave/components/brave_shields/core/browser/ad_block_custom_resource_provider.h"
#include "brave/components/brave_shields/core/browser/ad_block_default_resource_provider.h"
#include "brave/components/brave_shields/core/browser/ad_block_filter_list_catalog_provider.h"
#include "brave/components/brave_shields/core/browser/ad_block_filters_provider_manager.h"
#include "brave/components/brave_shields/core/common/adblock/rs/src/lib.rs.h"
#include "brave/components/brave_shields/core/common/features.h"
#include "brave/components/brave_shields/core/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "services/network/public/cpp/features.h"

namespace brave_shields {

namespace {
constexpr char kAdblockCacheDir[] = "adblock_cache";

std::optional<std::pair<DATFileDataBuffer, DATFileDataBuffer>>
ReadCachedDATFiles(base::FilePath cache_dir) {
  if (!base::DirectoryExists(cache_dir)) {
    base::CreateDirectory(cache_dir);
    return std::nullopt;
  }

  base::FilePath default_engine_dat_file = cache_dir.AppendASCII("engine0.dat");
  base::FilePath additional_engine_dat_file =
      cache_dir.AppendASCII("engine1.dat");
  if (!base::PathExists(default_engine_dat_file) ||
      !base::PathExists(additional_engine_dat_file)) {
    return std::nullopt;
  }

  auto default_engine_dat = base::ReadFileToBytes(default_engine_dat_file);
  if (!default_engine_dat) {
    return std::nullopt;
  }
  auto additional_engine_dat =
      base::ReadFileToBytes(additional_engine_dat_file);
  if (!additional_engine_dat) {
    return std::nullopt;
  }

  return std::make_optional(std::make_pair(std::move(*default_engine_dat),
                                           std::move(*additional_engine_dat)));
}
}  // namespace

AdBlockService::SourceProviderObserver::SourceProviderObserver(
    OnResourcesLoadedCallback on_resources_loaded,
    AdBlockResourceProvider* resource_provider,
    AdBlockFiltersProviderManager* filters_provider_manager,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    bool engine_is_default)
    : on_resources_loaded_(std::move(on_resources_loaded)),
      engine_is_default_(engine_is_default),
      resource_provider_(resource_provider),
      filters_provider_manager_(filters_provider_manager),
      task_runner_(std::move(task_runner)) {
  filters_provider_manager_->AddObserver(this);
  OnChanged(engine_is_default_);
}

AdBlockService::SourceProviderObserver::~SourceProviderObserver()
  filters_provider_manager_->RemoveObserver(this);
  resource_provider_->RemoveObserver(this);
}

void AdBlockService::SourceProviderObserver::OnChanged(bool is_default_engine,
                                                       base::Time timestamp) {
  if (engine_is_default_ != is_default_engine) {
    // Skip updates of another engine.
    return;
  }
  int64_t cache_timestamp_value = local_state_->GetInt64(cache_timestamp_pref_);
  base::Time cache_timestamp =
      base::Time::FromMillisecondsSinceUnixEpoch(cache_timestamp_value);
  // Add 1 millisecond since `timestamp` may have microsecond resolution
  if (cache_timestamp_value != 0 &&
      (timestamp <= cache_timestamp + base::Milliseconds(1) ||
       cache_timestamp > base::Time::Now())) {
    // Skip updates that have already been cached.
    return;
  }
  auto on_loaded_cb = base::BindOnce(
      &AdBlockService::SourceProviderObserver::OnFilterSetCallbackLoaded,
      weak_factory_.GetWeakPtr(), timestamp);
  filters_provider_manager_->LoadFilterSetForEngine(is_default_engine,
                                                    std::move(on_loaded_cb));
}

void AdBlockService::SourceProviderObserver::PreloadCachedDAT(
    DATFileDataBuffer dat) {
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<AdBlockEngine> engine, DATFileDataBuffer dat) {
            if (engine) {
              // Resources will be loaded when available
              auto empty_resources = adblock::new_empty_resource_storage();
              return engine->Load(true, std::move(dat), *empty_resources);
            }
            return false;
          },
          adblock_engine_->AsWeakPtr(), std::move(dat)),
      base::BindOnce(
          &AdBlockService::SourceProviderObserver::OnPreloadCachedDAT,
          weak_factory_.GetWeakPtr()));
}

void AdBlockService::SourceProviderObserver::OnPreloadCachedDAT(bool success) {
  if (success) {
    LoadResources();
  } else {
    OnChanged(adblock_engine_->IsDefaultEngine(), base::Time());
  }
}

void AdBlockService::SourceProviderObserver::OnFilterSetCallbackLoaded(
    base::Time timestamp,
    base::OnceCallback<void(rust::Box<adblock::FilterSet>*)> cb) {
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](base::OnceCallback<void(rust::Box<adblock::FilterSet>*)> cb) {
            auto filter_set = std::make_unique<rust::Box<adblock::FilterSet>>(
                adblock::new_filter_set());
            std::move(cb).Run(filter_set.get());
            return filter_set;
          },
          std::move(cb)),
      base::BindOnce(
          &AdBlockService::SourceProviderObserver::OnFilterSetCreated,
          weak_factory_.GetWeakPtr(), timestamp));
}

void AdBlockService::SourceProviderObserver::OnFilterSetCreated(
    base::Time timestamp,
    std::unique_ptr<rust::Box<adblock::FilterSet>> filter_set) {
  TRACE_EVENT("brave.adblock", "OnFilterSetCreated");
  filter_set_ = std::move(filter_set);
  timestamp_ = std::move(timestamp);
  // multiple AddObserver calls are ignored
  resource_provider_->AddObserver(this);
  resource_provider_->LoadResources(base::BindOnce(
      &SourceProviderObserver::OnResourcesLoaded, weak_factory_.GetWeakPtr()));
}

void AdBlockService::SourceProviderObserver::OnResourcesLoaded(
    AdblockResourceStorageBox storage) {
  on_resources_loaded_.Run(engine_is_default_, std::move(filter_set_),
                           std::move(storage));
}

AdBlockComponentServiceManager* AdBlockService::component_service_manager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return component_service_manager_.get();
}

AdBlockCustomFiltersProvider* AdBlockService::custom_filters_provider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return custom_filters_provider_.get();
}

AdBlockCustomResourceProvider* AdBlockService::custom_resource_provider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return custom_resource_provider_.get();
}

AdBlockSubscriptionServiceManager*
AdBlockService::subscription_service_manager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return subscription_service_manager_.get();
}

AdBlockService::AdBlockService(
    PrefService* local_state,
    std::string locale,
    component_updater::ComponentUpdateService* cus,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    AdBlockSubscriptionDownloadManager::DownloadManagerGetter
        subscription_download_manager_getter,
    const base::FilePath& profile_dir)
    : local_state_(local_state),
      locale_(locale),
      profile_dir_(profile_dir),
      subscription_download_manager_getter_(
          std::move(subscription_download_manager_getter)),
      component_update_service_(cus),
      task_runner_(task_runner),
      list_p3a_(local_state),
      engine_wrapper_(AdBlockEngineWrapper::Create().release(),
                      base::OnTaskRunnerDeleter(task_runner_.get())) {
  TRACE_EVENT("brave.adblock", "AdBlockService");
  // Initializes adblock-rust's domain resolution implementation
  adblock::set_domain_resolver();

  if (base::FeatureList::IsEnabled(
          features::kAdblockOverrideRegexDiscardPolicy)) {
    adblock::RegexManagerDiscardPolicy policy;
    policy.cleanup_interval_secs =
        features::kAdblockOverrideRegexDiscardPolicyCleanupIntervalSec.Get();
    policy.discard_unused_secs =
        features::kAdblockOverrideRegexDiscardPolicyDiscardUnusedSec.Get();
    SetupDiscardPolicy(policy);
  }

  base::FilePath cache_dir = profile_dir_.AppendASCII(kAdblockCacheDir);
  GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&ReadCachedDATFiles, cache_dir),
      base::BindOnce(&AdBlockService::OnReadCachedDATFiles,
                     weak_factory_.GetWeakPtr()));

  auto default_resource_provider =
      std::make_unique<AdBlockDefaultResourceProvider>(
          component_update_service_);
  default_resource_provider_ = default_resource_provider.get();

  custom_resource_provider_ = new AdBlockCustomResourceProvider(
      profile_dir_, std::move(default_resource_provider));
  resource_provider_.reset(custom_resource_provider_.get());
  filter_list_catalog_provider_ =
      std::make_unique<AdBlockFilterListCatalogProvider>(
          component_update_service_);

  filters_provider_manager_ = std::make_unique<AdBlockFiltersProviderManager>();

  component_service_manager_ = std::make_unique<AdBlockComponentServiceManager>(
      local_state_, filters_provider_manager_.get(), locale_,
      component_update_service_, filter_list_catalog_provider_.get(),
      &list_p3a_);
  subscription_service_manager_ =
      std::make_unique<AdBlockSubscriptionServiceManager>(
          local_state_, filters_provider_manager_.get(),
          std::move(subscription_download_manager_getter_), profile_dir_,
          &list_p3a_);
  custom_filters_provider_ = std::make_unique<AdBlockCustomFiltersProvider>(
      local_state_, filters_provider_manager_.get());

  if (base::FeatureList::IsEnabled(
          network::features::kLocalNetworkAccessChecks) &&
      !network::features::kLocalNetworkAccessChecksWarn.Get() &&
      base::FeatureList::IsEnabled(
          network::features::kLocalNetworkAccessChecksWebSockets)) {
    // If LNA enabled and blocks request
    localhost_filters_provider_ =
        std::make_unique<AdBlockLocalhostFiltersProvider>(
            filters_provider_manager_.get());
  }

  const auto make_on_resources_loaded_callback = base::BindPostTask(
      task_runner_,
      base::BindRepeating(&AdBlockEngineWrapper::OnResourcesLoaded,
                          base::Unretained(engine_wrapper_.get())));

  default_service_observer_ = std::make_unique<SourceProviderObserver>(
      make_on_resources_loaded_callback, resource_provider_.get(),
      filters_provider_manager_.get(), task_runner_, true);
  additional_filters_service_observer_ =
      std::make_unique<SourceProviderObserver>(
          make_on_resources_loaded_callback, resource_provider_.get(),
          filters_provider_manager_.get(), task_runner_, false);
}

AdBlockService::~AdBlockService() {
  // The engines are deleted on the task runner with SKIP_ON_SHUTDOWN trait,
  // therefore they leak during shutdown.
  ANNOTATE_LEAKING_OBJECT_PTR(engine_wrapper_.get());
}

void AdBlockService::OnReadCachedDATFiles(
    std::optional<std::pair<DATFileDataBuffer, DATFileDataBuffer>>
        read_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!read_result) {
    ActivateFilterLoading();
  } else {
    default_service_observer_->PreloadCachedDAT(std::move(read_result->first));
    additional_filters_service_observer_->PreloadCachedDAT(
        std::move(read_result->second));
  }
}

void AdBlockService::EnableTag(const std::string& tag, bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Tags only need to be modified for the default engine.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AdBlockEngineWrapper::EnableTag,
                     base::Unretained(engine_wrapper_.get()), tag, enabled));
}

void AdBlockService::AddUserCosmeticFilter(const std::string& filter) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  custom_filters_provider_->AddUserCosmeticFilter(filter);
}

bool AdBlockService::AreAnyBlockedElementsPresent(std::string_view host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return custom_filters_provider_->AreAnyBlockedElementsPresent(host);
}

void AdBlockService::ResetCosmeticFilter(std::string_view host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  custom_filters_provider_->ResetCosmeticFilter(host);
}

void AdBlockService::GetDebugInfoAsync(GetDebugInfoCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&AdBlockEngineWrapper::GetDebugInfo,
                     base::Unretained(engine_wrapper_.get())),
      std::move(callback));
}

void AdBlockService::DiscardRegex(uint64_t regex_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AdBlockEngineWrapper::DiscardRegex,
                     base::Unretained(engine_wrapper_.get()), regex_id));
}

void AdBlockService::SetupDiscardPolicy(
    const adblock::RegexManagerDiscardPolicy& policy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AdBlockEngineWrapper::SetupDiscardPolicy,
                     base::Unretained(engine_wrapper_.get()), policy));
}

void RegisterPrefsForAdBlockService(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kAdBlockCookieListSettingTouched, false);
  registry->RegisterBooleanPref(
      prefs::kAdBlockMobileNotificationsListSettingTouched, false);
  registry->RegisterStringPref(prefs::kAdBlockCustomFilters, std::string());
  registry->RegisterInt64Pref(prefs::kAdBlockCustomFiltersLastModified, 0);
  registry->RegisterDictionaryPref(prefs::kAdBlockRegionalFilters);
  registry->RegisterDictionaryPref(prefs::kAdBlockListSubscriptions);
  registry->RegisterBooleanPref(prefs::kAdBlockCheckedDefaultRegion, false);
  registry->RegisterBooleanPref(prefs::kAdBlockCheckedAllDefaultRegions, false);
  registry->RegisterBooleanPref(prefs::kAdBlockOnlyModeEnabled, false);
  registry->RegisterBooleanPref(
      prefs::kAdBlockOnlyModeWasEnabledForSupportedLocale, false);
  registry->RegisterInt64Pref(prefs::kAdBlockDefaultCacheTimestamp, 0);
  registry->RegisterInt64Pref(prefs::kAdBlockAdditionalCacheTimestamp, 0);
}

void RegisterPrefsForAdBlockServiceForMigration(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kAdBlockCookieListOptInShown, false);
}

void MigrateObsoletePrefsForAdBlockService(PrefService* local_state) {
  // Added 2025-07-11
  local_state->ClearPref(prefs::kAdBlockCookieListOptInShown);
}

AdBlockDefaultResourceProvider* AdBlockService::default_resource_provider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return default_resource_provider_.get();
}

AdBlockEngine& AdBlockService::GetDefaultEngineForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_IS_TEST();
  return engine_wrapper_->default_engine_for_testing();  // IN-TEST
}

AdBlockEngine& AdBlockService::GetAdditionalFiltersEngineForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_IS_TEST();
  return engine_wrapper_->additional_filters_engine_for_testing();  // IN-TEST
}

AdBlockFiltersProviderManager*
AdBlockService::GetFiltersProviderManagerForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_IS_TEST();
  return filters_provider_manager_.get();
}

AdBlockDefaultResourceProvider*
AdBlockService::GetDefaultResourceProviderForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_IS_TEST();
  return default_resource_provider_.get();
}

base::SequencedTaskRunner* AdBlockService::GetTaskRunnerForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_IS_TEST();
  return task_runner_.get();
}

}  // namespace brave_shields
