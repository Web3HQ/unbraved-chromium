// Copyright (c) 2021 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "brave/components/brave_shields/content/browser/ad_block_subscription_filters_provider.h"

#include <optional>
#include <string>
#include <utility>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "brave/components/brave_shields/core/browser/ad_block_filters_provider.h"
#include "brave/components/brave_shields/core/common/adblock/rs/src/lib.rs.h"
#include "brave/components/brave_shields/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "crypto/sha2.h"

namespace brave_shields {

namespace {

// static
void AddDATBufferToFilterSet(
    base::OnceCallback<void(const adblock::FilterListMetadata&)> on_metadata,
    DATFileDataBuffer buffer,
    const perfetto::Flow& flow,
    rust::Box<adblock::FilterSet>* filter_set) {
  TRACE_EVENT("brave.adblock",
              "AddDATBufferToFilterSet_SubscriptionFiltersProvider", flow);
  auto result = (*filter_set)->add_filter_list(buffer);
  if (result.result_kind == adblock::ResultKind::Success) {
    std::move(on_metadata).Run(result.value);
  } else {
    VLOG(0) << "Subscription list parsing failed: "
            << result.error_message.c_str();
  }
}

}  // namespace

AdBlockSubscriptionFiltersProvider::AdBlockSubscriptionFiltersProvider(
    PrefService* local_state,
    AdBlockFiltersProviderManager* manager,
    base::FilePath list_file,
    base::RepeatingCallback<void(const adblock::FilterListMetadata&)>
        on_metadata_retrieved)
    : AdBlockFiltersProvider(false, manager),
      list_file_(list_file),
      local_state_(local_state),
      on_metadata_retrieved_(on_metadata_retrieved) {}

AdBlockSubscriptionFiltersProvider::~AdBlockSubscriptionFiltersProvider() =
    default;

void AdBlockSubscriptionFiltersProvider::LoadFilterSet(
    base::OnceCallback<
        void(base::OnceCallback<void(rust::Box<adblock::FilterSet>*)>)> cb) {
  const auto flow = perfetto::Flow::FromPointer(this);
  TRACE_EVENT("brave.adblock",
              "AdBlockSubscriptionFiltersProvider::LoadFilterSet", flow);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&brave_component_updater::ReadDATFileData, list_file_),
      base::BindOnce(&AdBlockSubscriptionFiltersProvider::OnDATFileDataReady,
                     weak_factory_.GetWeakPtr(), std::move(cb), flow));
}

std::string AdBlockSubscriptionFiltersProvider::GetNameForDebugging() {
  return "AdBlockSubscriptionFiltersProvider";
}

std::string AdBlockSubscriptionFiltersProvider::GetCacheKey() const {
  return list_file_.BaseName().RemoveExtension().MaybeAsASCII();
}

std::optional<std::string> AdBlockSubscriptionFiltersProvider::GetContentHash()
    const {
  if (!content_hash_.empty()) {
    return content_hash_;
  }
  if (local_state_) {
    const auto& dict =
        local_state_->GetDict(prefs::kAdBlockSubscriptionFiltersCacheTimestamp);
    const std::string* stored = dict.FindString(GetCacheKey());
    if (stored) {
      return *stored;
    }
  }
  return std::nullopt;
}

void AdBlockSubscriptionFiltersProvider::OnDATFileDataReady(
    base::OnceCallback<
        void(base::OnceCallback<void(rust::Box<adblock::FilterSet>*)>)> cb,
    const perfetto::Flow& flow,
    const DATFileDataBuffer& dat_buf) {
  TRACE_EVENT("brave.adblock",
              "AdBlockSubscriptionFiltersProvider::OnDATFileDataReady", flow);
  // Compute the content hash while we have the data in hand.
  content_hash_ = base::HexEncode(
      crypto::SHA256HashString(std::string(dat_buf.begin(), dat_buf.end())));
  if (local_state_) {
    ScopedDictPrefUpdate update(
        local_state_, prefs::kAdBlockSubscriptionFiltersCacheTimestamp);
    update->Set(GetCacheKey(), content_hash_);
  }

  std::move(cb).Run(base::BindOnce(
      &AddDATBufferToFilterSet,
      base::BindOnce(
          [](scoped_refptr<base::SequencedTaskRunner> task_runner,
             base::RepeatingCallback<void(const adblock::FilterListMetadata&)>
                 on_metadata,
             const adblock::FilterListMetadata& metadata) {
            task_runner->PostTask(FROM_HERE,
                                  base::BindOnce(on_metadata, metadata));
          },
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          on_metadata_retrieved_),
      dat_buf, flow));
}

void AdBlockSubscriptionFiltersProvider::OnListAvailable() {
  // Clear the in-memory hash so ComputeCombinedHash produces a different
  // result, forcing ShouldLoadFilterState to return true and trigger a
  // reload. The new hash will be computed in OnDATFileDataReady when the
  // file content is read during filter set loading.
  content_hash_.clear();
  NotifyObservers(engine_is_default_);
}

}  // namespace brave_shields
