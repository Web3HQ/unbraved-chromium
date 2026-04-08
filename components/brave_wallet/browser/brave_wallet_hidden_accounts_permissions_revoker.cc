/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_wallet/browser/brave_wallet_hidden_accounts_permissions_revoker.h"

#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "brave/components/brave_wallet/browser/brave_wallet_service_delegate.h"
#include "brave/components/brave_wallet/browser/brave_wallet_utils.h"
#include "brave/components/brave_wallet/browser/permission_utils.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace brave_wallet {

BraveWalletHiddenAccountsPermissionsRevoker::
    BraveWalletHiddenAccountsPermissionsRevoker(
        BraveWalletServiceDelegate& delegate)
    : delegate_(delegate) {}

BraveWalletHiddenAccountsPermissionsRevoker::
    ~BraveWalletHiddenAccountsPermissionsRevoker() = default;

void BraveWalletHiddenAccountsPermissionsRevoker::RevokeHiddenAccountPermisson(
    const mojom::AccountIdPtr& account_id,
    base::OnceCallback<void(bool)> callback) {
  const auto request_type = CoinTypeToPermissionRequestType(account_id->coin);
  if (!request_type) {
    std::move(callback).Run(false);
    return;
  }

  const std::string hidden_account_identifier =
      GetAccountPermissionIdentifier(account_id);
  delegate_->GetWebSitesWithPermission(
      account_id->coin,
      base::BindOnce(&BraveWalletHiddenAccountsPermissionsRevoker::
                         ResolveWebsitePermissionsForHiddenAccount,
                     weak_ptr_factory_.GetWeakPtr(), account_id->coin,
                     hidden_account_identifier, std::move(callback)));
}

void BraveWalletHiddenAccountsPermissionsRevoker::
    ResolveWebsitePermissionsForHiddenAccount(
        mojom::CoinType coin,
        std::string hidden_account_identifier,
        base::OnceCallback<void(bool)> callback,
        const std::vector<std::string>& websites) {
  const auto request_type = CoinTypeToPermissionRequestType(coin);
  if (!request_type) {
    std::move(callback).Run(false);
    return;
  }

  bool revoke_success = true;
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

    revoke_success &=
        delegate_->ResetPermission(coin, requesting_origin, account_identifier);
  }
  std::move(callback).Run(revoke_success);
}

}  // namespace brave_wallet
