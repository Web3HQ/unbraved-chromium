/* Copyright (c) 2023 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/brave_wallet/simulation_service_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "brave/browser/brave_wallet/brave_wallet_context_utils.h"
#include "brave/browser/brave_wallet/brave_wallet_service_factory.h"
#include "brave/components/brave_wallet/browser/simulation_service.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/storage_partition.h"

namespace brave_wallet {

// static
SimulationServiceFactory* SimulationServiceFactory::GetInstance() {
  static base::NoDestructor<SimulationServiceFactory> instance;
  return instance.get();
}

// static
SimulationService* SimulationServiceFactory::GetServiceForContext(
    content::BrowserContext* context) {
  return static_cast<SimulationService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

SimulationServiceFactory::SimulationServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "SimulationService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(BraveWalletServiceFactory::GetInstance());
}

SimulationServiceFactory::~SimulationServiceFactory() = default;

std::unique_ptr<KeyedService>
SimulationServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!CanBuildWalletServiceInstanceForBrowserContext(context)) {
    return nullptr;
  }

  return std::make_unique<SimulationService>(
      context->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess(),
      BraveWalletServiceFactory::GetServiceForContext(context));
}

content::BrowserContext* SimulationServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

}  // namespace brave_wallet
