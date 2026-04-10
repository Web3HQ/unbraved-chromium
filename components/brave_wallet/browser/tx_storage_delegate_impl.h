/* Copyright (c) 2023 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_COMPONENTS_BRAVE_WALLET_BROWSER_TX_STORAGE_DELEGATE_IMPL_H_
#define BRAVE_COMPONENTS_BRAVE_WALLET_BROWSER_TX_STORAGE_DELEGATE_IMPL_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "brave/components/brave_wallet/browser/tx_storage_delegate.h"
#include "components/value_store/value_store_factory.h"
#include "components/value_store/value_store_frontend.h"

namespace value_store {
class ValueStoreFrontend;
}  // namespace value_store

namespace brave_wallet {

// Default `TxStorageDelegate` implementation. Supports reading and saving
// transaction dict into leveldb database.
class TxStorageDelegateImpl final : public TxStorageDelegate {
 public:
  explicit TxStorageDelegateImpl(const base::FilePath& wallet_base_directory);
  ~TxStorageDelegateImpl() override;
  TxStorageDelegateImpl(const TxStorageDelegateImpl&) = delete;
  TxStorageDelegateImpl& operator=(const TxStorageDelegateImpl&) = delete;

  bool IsInitialized() const override;
  void ScheduleWrite() override;
  void Clear() override;
  void SetOnInitializedCallbackForTesting(base::OnceClosure callback) override;
  void DisableWritesForTesting(bool disable);

 private:
  friend class TxStateManagerUnitTest;
  friend class TxStorageDelegateImplUnitTest;
  FRIEND_TEST_ALL_PREFIXES(TxStorageDelegateImplUnitTest, ReadWriteAndClear);
  FRIEND_TEST_ALL_PREFIXES(TxStorageDelegateImplUnitTest,
                           BraveWalletTransactionsDBFormatMigrated);
  FRIEND_TEST_ALL_PREFIXES(EthTxManagerUnitTest, Reset);

  // Read all txs from db
  void Initialize();
  void OnTxsInitialRead(std::optional<base::Value> txs);
  void RunDBMigrations();

  // Used to indicate if transactions is loaded to memory caches txs_
  bool initialized_ = false;

  bool disable_writes_for_testing_ = false;
  base::OnceClosure on_initialized_callback_for_testing_;

  std::unique_ptr<value_store::ValueStoreFrontend> store_;

  base::WeakPtrFactory<TxStorageDelegateImpl> weak_factory_{this};
};

// `TxStorageDelegate` implementation which doesn't persist anything on disk.
class TxStorageDelegateIncognitoImpl final : public TxStorageDelegate {
 public:
  TxStorageDelegateIncognitoImpl() = default;
  ~TxStorageDelegateIncognitoImpl() override = default;
  TxStorageDelegateIncognitoImpl(const TxStorageDelegateIncognitoImpl&) =
      delete;
  TxStorageDelegateIncognitoImpl& operator=(
      const TxStorageDelegateIncognitoImpl&) = delete;

  bool IsInitialized() const override;
  void ScheduleWrite() override {}
  void Clear() override {}
  void SetOnInitializedCallbackForTesting(base::OnceClosure callback) override;
};

}  // namespace brave_wallet

#endif  // BRAVE_COMPONENTS_BRAVE_WALLET_BROWSER_TX_STORAGE_DELEGATE_IMPL_H_
