/* Copyright (c) 2023 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/brave_wallet/brave_wallet_ipfs_service_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "brave/browser/brave_wallet/brave_wallet_context_utils.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/storage_partition.h"

namespace brave_wallet {

// static
BraveWalletIpfsServiceFactory* BraveWalletIpfsServiceFactory::GetInstance() {
  static base::NoDestructor<BraveWalletIpfsServiceFactory> instance;
  return instance.get();
}

// static
BraveWalletIpfsService* BraveWalletIpfsServiceFactory::GetServiceForContext(
    content::BrowserContext* context) {
  return static_cast<BraveWalletIpfsService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

BraveWalletIpfsServiceFactory::BraveWalletIpfsServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "BraveWalletIpfsService",
          BrowserContextDependencyManager::GetInstance()) {}

BraveWalletIpfsServiceFactory::~BraveWalletIpfsServiceFactory() = default;

std::unique_ptr<KeyedService>
BraveWalletIpfsServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!CanBuildWalletServiceInstanceForBrowserContext(context)) {
    return nullptr;
  }

  return std::make_unique<BraveWalletIpfsService>(
      user_prefs::UserPrefs::Get(context));
}

content::BrowserContext* BraveWalletIpfsServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

}  // namespace brave_wallet
