/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_wallet/browser/brave_wallet_hidden_accounts_permissions_revoker.h"

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "base/test/test_future.h"
#include "brave/components/brave_wallet/browser/brave_wallet_service_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace brave_wallet {
namespace {

using testing::Eq;

class MockBraveWalletServiceDelegate : public BraveWalletServiceDelegate {
 public:
  MOCK_METHOD(void,
              GetWebSitesWithPermission,
              (mojom::CoinType coin,
               GetWebSitesWithPermissionCallback callback),
              (override));
  MOCK_METHOD(bool,
              ResetPermission,
              (mojom::CoinType coin,
               const url::Origin& origin,
               const std::string& account),
              (override));
  MOCK_METHOD(base::FilePath, GetWalletBaseDirectory, (), (override));
  MOCK_METHOD(bool, IsPrivateWindow, (), (override));
};

class BraveWalletHiddenAccountsPermissionsRevokerUnitTest
    : public testing::Test {};

TEST_F(BraveWalletHiddenAccountsPermissionsRevokerUnitTest,
       RevokesPermissionForHiddenAccount) {
  MockBraveWalletServiceDelegate delegate;

  const std::string address = "0x407637cC04893DA7FA4A7C0B58884F82d69eD448";
  const auto account_id = mojom::AccountId::New(
      mojom::CoinType::ETH, mojom::KeyringId::kDefault,
      mojom::AccountKind::kDerived, address, 0, "eth_unique_key");
  const auto expected_origin =
      url::Origin::Create(GURL("https://app.brave.com"));
  const std::string expected_account_identifier = base::ToLowerASCII(address);
  const std::string website = "https://app.brave.com" + address + "/";

  EXPECT_CALL(delegate,
              GetWebSitesWithPermission(mojom::CoinType::ETH, testing::_))
      .Times(1)
      .WillOnce(
          [&](mojom::CoinType,
              BraveWalletServiceDelegate::GetWebSitesWithPermissionCallback
                  cb) { std::move(cb).Run({website}); });
  EXPECT_CALL(delegate,
              ResetPermission(mojom::CoinType::ETH, Eq(expected_origin),
                              Eq(expected_account_identifier)))
      .Times(1)
      .WillOnce(testing::Return(true));

  BraveWalletHiddenAccountsPermissionsRevoker handler(delegate);
  base::test::TestFuture<bool> future;
  handler.RevokeHiddenAccountPermisson(account_id->Clone(),
                                       future.GetCallback());
  EXPECT_TRUE(future.Get());
}

TEST_F(BraveWalletHiddenAccountsPermissionsRevokerUnitTest,
       HiddenAccountWithoutPermissionsDoesNotResetAnyPermission) {
  MockBraveWalletServiceDelegate delegate;

  const std::string address = "0x407637cC04893DA7FA4A7C0B58884F82d69eD448";
  const auto account_id = mojom::AccountId::New(
      mojom::CoinType::ETH, mojom::KeyringId::kDefault,
      mojom::AccountKind::kDerived, address, 0, "eth_unique_key");

  EXPECT_CALL(delegate,
              GetWebSitesWithPermission(mojom::CoinType::ETH, testing::_))
      .Times(1)
      .WillOnce(
          [&](mojom::CoinType,
              BraveWalletServiceDelegate::GetWebSitesWithPermissionCallback
                  cb) { std::move(cb).Run({}); });
  EXPECT_CALL(delegate, ResetPermission(testing::_, testing::_, testing::_))
      .Times(0);

  BraveWalletHiddenAccountsPermissionsRevoker handler(delegate);
  base::test::TestFuture<bool> future;
  handler.RevokeHiddenAccountPermisson(account_id->Clone(),
                                       future.GetCallback());
  EXPECT_TRUE(future.Get());
}

}  // namespace
}  // namespace brave_wallet
