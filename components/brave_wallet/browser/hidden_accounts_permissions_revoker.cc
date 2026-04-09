/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_wallet/browser/hidden_accounts_permissions_revoker.h"

#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "brave/components/brave_wallet/browser/brave_wallet_service_delegate.h"
#include "brave/components/brave_wallet/browser/brave_wallet_utils.h"
#include "brave/components/brave_wallet/browser/keyring_service.h"
#include "brave/components/brave_wallet/browser/permission_utils.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace brave_wallet {

BraveWalletHiddenAccountsPermissionsRevoker::
    BraveWalletHiddenAccountsPermissionsRevoker(
        KeyringService& keyring_service,
        BraveWalletServiceDelegate& delegate)
    : keyring_service_(keyring_service), delegate_(delegate) {
  keyring_service_->AddObserver(
      keyring_service_observer_receiver_.BindNewPipeAndPassRemote());
  for (const auto& account : keyring_service_->GetHiddenAccountsSync()) {
    hidden_account_identifiers_.insert(
        GetAccountPermissionIdentifier(account->account_id));
  }
}

BraveWalletHiddenAccountsPermissionsRevoker::
    ~BraveWalletHiddenAccountsPermissionsRevoker() = default;

void BraveWalletHiddenAccountsPermissionsRevoker::AccountsChanged() {
  RevokePermissionsForHiddenAccounts(keyring_service_->GetHiddenAccountsSync());
}

void BraveWalletHiddenAccountsPermissionsRevoker::
    RevokePermissionsForHiddenAccounts(
        std::vector<mojom::AccountInfoPtr> hidden_accounts) {
  base::flat_set<std::string> new_hidden_account_identifiers;
  for (const auto& account : hidden_accounts) {
    const std::string hidden_account_identifier =
        GetAccountPermissionIdentifier(account->account_id);
    new_hidden_account_identifiers.insert(hidden_account_identifier);

    if (!hidden_account_identifiers_.contains(hidden_account_identifier)) {
      delegate_->GetWebSitesWithPermission(
          account->account_id->coin,
          base::BindOnce(&BraveWalletHiddenAccountsPermissionsRevoker::
                             ResolveWebsitePermissionsForHiddenAccount,
                         weak_ptr_factory_.GetWeakPtr(),
                         account->account_id->coin, hidden_account_identifier));
    }
  }

  hidden_account_identifiers_ = std::move(new_hidden_account_identifiers);
}

void BraveWalletHiddenAccountsPermissionsRevoker::
    ResolveWebsitePermissionsForHiddenAccount(
        mojom::CoinType coin,
        std::string hidden_account_identifier,
        const std::vector<std::string>& websites) {
  const auto request_type = CoinTypeToPermissionRequestType(coin);
  if (!request_type) {
    return;
  }

  for (const std::string& website : websites) {
    const GURL website_url(website);
    if (!website_url.is_valid()) {
      continue;
    }

    url::Origin requesting_origin;
    std::string account_identifier;
    if (!ParseRequestingOriginFromSubRequest(
            *request_type, url::Origin::Create(website_url), &requesting_origin,
            &account_identifier)) {
      continue;
    }

    if (!base::EqualsCaseInsensitiveASCII(account_identifier,
                                          hidden_account_identifier)) {
      continue;
    }

    delegate_->ResetPermission(coin, requesting_origin, account_identifier);
  }
}

}  // namespace brave_wallet
