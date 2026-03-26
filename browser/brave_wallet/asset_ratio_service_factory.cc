/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/brave_wallet/asset_ratio_service_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "brave/browser/brave_wallet/brave_wallet_context_utils.h"
#include "brave/components/brave_wallet/browser/asset_ratio_service.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/storage_partition.h"

namespace brave_wallet {

// static
AssetRatioServiceFactory* AssetRatioServiceFactory::GetInstance() {
  static base::NoDestructor<AssetRatioServiceFactory> instance;
  return instance.get();
}

// static
AssetRatioService* AssetRatioServiceFactory::GetServiceForContext(
    content::BrowserContext* context) {
  return static_cast<AssetRatioService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

AssetRatioServiceFactory::AssetRatioServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "AssetRatioService",
          BrowserContextDependencyManager::GetInstance()) {}

AssetRatioServiceFactory::~AssetRatioServiceFactory() = default;

std::unique_ptr<KeyedService>
AssetRatioServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!CanBuildWalletServiceInstanceForBrowserContext(context)) {
    return nullptr;
  }

  return std::make_unique<AssetRatioService>(
      context->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess());
}

content::BrowserContext* AssetRatioServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

}  // namespace brave_wallet
