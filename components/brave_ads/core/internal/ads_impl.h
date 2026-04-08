/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_COMPONENTS_BRAVE_ADS_CORE_INTERNAL_ADS_IMPL_H_
#define BRAVE_COMPONENTS_BRAVE_ADS_CORE_INTERNAL_ADS_IMPL_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "brave/components/brave_ads/core/internal/account/tokens/token_generator_interface.h"
#include "brave/components/brave_ads/core/internal/account/wallet/wallet_info.h"
#include "brave/components/brave_ads/core/internal/global_state/global_state.h"
#include "brave/components/brave_ads/core/mojom/brave_ads.mojom-forward.h"
#include "brave/components/brave_ads/core/public/ads.h"
#include "brave/components/brave_ads/core/public/ads_callback.h"
#include "brave/components/brave_ads/core/public/common/functional/once_closure_task_queue.h"

namespace base {
class Time;
}  // namespace base

namespace brave_ads {

namespace database {
class Maintenance;
}  // namespace database

class AdsImpl final : public Ads {
 public:
  AdsImpl(AdsClient& ads_client,
          const base::FilePath& database_path,
          std::unique_ptr<TokenGeneratorInterface> token_generator);

  AdsImpl(const AdsImpl&) = delete;
  AdsImpl& operator=(const AdsImpl&) = delete;

  ~AdsImpl() override;

  // Ads:
  void AddObserver(std::unique_ptr<AdsObserver> observer) override;

  void SetSysInfo(mojom::SysInfoPtr mojom_sys_info) override;
  void SetBuildChannel(mojom::BuildChannelInfoPtr mojom_build_channel) override;
  void SetCommandLineSwitches(
      mojom::CommandLineSwitchesPtr mojom_command_line_switches) override;
  void SetContentSettings(
      mojom::ContentSettingsPtr mojom_content_settings) override;

  void Initialize(mojom::WalletInfoPtr mojom_wallet,
                  InitializeCallback callback) override;
  void Shutdown(ShutdownCallback callback) override;

  void GetInternals(GetInternalsCallback callback) override;

  // TODO(https://github.com/brave/brave-browser/issues/42034): Transition
  // diagnostics from brave://rewards-internals to brave://ads-internals.
  void GetDiagnostics(GetDiagnosticsCallback callback) override;

  void GetStatementOfAccounts(GetStatementOfAccountsCallback callback) override;

  void ParseAndSaveNewTabPageAds(
      base::DictValue dict,
      ParseAndSaveNewTabPageAdsCallback callback) override;
  void MaybeServeNewTabPageAd(MaybeServeNewTabPageAdCallback callback) override;
  void TriggerNewTabPageAdEvent(
      const std::string& placement_id,
      const std::string& creative_instance_id,
      mojom::NewTabPageAdMetricType mojom_ad_metric_type,
      mojom::NewTabPageAdEventType mojom_ad_event_type,
      TriggerAdEventCallback callback) override;

  void MaybeGetNotificationAd(const std::string& placement_id,
                              MaybeGetNotificationAdCallback callback) override;
  void TriggerNotificationAdEvent(
      const std::string& placement_id,
      mojom::NotificationAdEventType mojom_ad_event_type,
      TriggerAdEventCallback callback) override;

  void MaybeGetSearchResultAd(const std::string& placement_id,
                              MaybeGetSearchResultAdCallback callback) override;
  void TriggerSearchResultAdEvent(
      mojom::CreativeSearchResultAdInfoPtr mojom_creative_ad,
      mojom::SearchResultAdEventType mojom_ad_event_type,
      TriggerAdEventCallback callback) override;

  void PurgeOrphanedAdEventsForType(
      mojom::AdType mojom_ad_type,
      PurgeOrphanedAdEventsForTypeCallback callback) override;

  void GetAdHistory(base::Time from_time,
                    base::Time to_time,
                    GetAdHistoryForUICallback callback) override;

  void ToggleLikeAd(mojom::ReactionInfoPtr mojom_reaction,
                    ToggleReactionCallback callback) override;
  void ToggleDislikeAd(mojom::ReactionInfoPtr mojom_reaction,
                       ToggleReactionCallback callback) override;
  void ToggleLikeSegment(mojom::ReactionInfoPtr mojom_reaction,
                         ToggleReactionCallback callback) override;
  void ToggleDislikeSegment(mojom::ReactionInfoPtr mojom_reaction,
                            ToggleReactionCallback callback) override;
  void ToggleSaveAd(mojom::ReactionInfoPtr mojom_reaction,
                    ToggleReactionCallback callback) override;
  void ToggleMarkAdAsInappropriate(mojom::ReactionInfoPtr mojom_reaction,
                                   ToggleReactionCallback callback) override;

 private:
  void CreateOrOpenDatabase(InitializeCallback callback);
  void CreateOrOpenDatabaseCallback(InitializeCallback callback, bool success);

  void FailedToInitialize(InitializeCallback callback);
  void SuccessfullyInitialized(InitializeCallback callback);

  // TODO(https://github.com/brave/brave-browser/issues/39795): Transition away
  // from using JSON state to a more efficient data approach.
  void MigrateClientStateCallback(InitializeCallback callback, bool success);
  void LoadClientStateCallback(InitializeCallback callback, bool success);
  void MigrateConfirmationStateCallback(InitializeCallback callback,
                                        bool success);
  void LoadConfirmationStateCallback(InitializeCallback callback, bool success);

  bool is_initialized_ = false;

  // Set on startup; `std::nullopt` when the user has not joined Brave Rewards.
  std::optional<WalletInfo> wallet_;

  // TODO(https://github.com/brave/brave-browser/issues/37622): Deprecate global
  // state.
  GlobalState global_state_;

  OnceClosureTaskQueue task_queue_;

  // Handles database maintenance tasks, such as purging.
  std::unique_ptr<database::Maintenance> database_maintenance_;

  base::WeakPtrFactory<AdsImpl> weak_factory_{this};
};

}  // namespace brave_ads

#endif  // BRAVE_COMPONENTS_BRAVE_ADS_CORE_INTERNAL_ADS_IMPL_H_
