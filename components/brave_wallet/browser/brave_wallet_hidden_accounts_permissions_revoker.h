/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_COMPONENTS_BRAVE_WALLET_BROWSER_BRAVE_WALLET_HIDDEN_ACCOUNTS_PERMISSIONS_REVOKER_H_
#define BRAVE_COMPONENTS_BRAVE_WALLET_BROWSER_BRAVE_WALLET_HIDDEN_ACCOUNTS_PERMISSIONS_REVOKER_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "brave/components/brave_wallet/common/brave_wallet.mojom.h"

namespace brave_wallet {

class BraveWalletServiceDelegate;

class BraveWalletHiddenAccountsPermissionsRevoker {
 public:
  explicit BraveWalletHiddenAccountsPermissionsRevoker(
      BraveWalletServiceDelegate& delegate);
  BraveWalletHiddenAccountsPermissionsRevoker(
      const BraveWalletHiddenAccountsPermissionsRevoker&) = delete;
  BraveWalletHiddenAccountsPermissionsRevoker& operator=(
      const BraveWalletHiddenAccountsPermissionsRevoker&) = delete;
  virtual ~BraveWalletHiddenAccountsPermissionsRevoker();
  virtual void RevokeHiddenAccountPermisson(
      const mojom::AccountIdPtr& account_id,
      base::OnceCallback<void(bool)> callback);

 private:
  void ResolveWebsitePermissionsForHiddenAccount(
      mojom::CoinType coin,
      std::string hidden_account_identifier,
      base::OnceCallback<void(bool)> callback,
      const std::vector<std::string>& websites);

  raw_ref<BraveWalletServiceDelegate> delegate_;
  base::WeakPtrFactory<BraveWalletHiddenAccountsPermissionsRevoker>
      weak_ptr_factory_{this};
};

}  // namespace brave_wallet

#endif  // BRAVE_COMPONENTS_BRAVE_WALLET_BROWSER_BRAVE_WALLET_HIDDEN_ACCOUNTS_PERMISSIONS_REVOKER_H_
