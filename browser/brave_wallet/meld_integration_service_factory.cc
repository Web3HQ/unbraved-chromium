/* Copyright (c) 2024 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/brave_wallet/meld_integration_service_factory.h"

#include <memory>

#include "brave/browser/brave_wallet/brave_wallet_context_utils.h"
#include "brave/components/brave_wallet/browser/meld_integration_service.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

namespace brave_wallet {

// static
MeldIntegrationServiceFactory* MeldIntegrationServiceFactory::GetInstance() {
  static base::NoDestructor<MeldIntegrationServiceFactory> instance;
  return instance.get();
}

// static
MeldIntegrationService* MeldIntegrationServiceFactory::GetServiceForContext(
    content::BrowserContext* context) {
  return static_cast<MeldIntegrationService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

MeldIntegrationServiceFactory::MeldIntegrationServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "MeldIntegrationService",
          BrowserContextDependencyManager::GetInstance()) {}

MeldIntegrationServiceFactory::~MeldIntegrationServiceFactory() = default;

std::unique_ptr<KeyedService>
MeldIntegrationServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!CanBuildWalletServiceInstanceForBrowserContext(context)) {
    return nullptr;
  }

  return std::make_unique<MeldIntegrationService>(
      context->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess());
}

content::BrowserContext* MeldIntegrationServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

}  // namespace brave_wallet
