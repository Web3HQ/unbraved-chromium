/* Copyright (c) 2023 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_wallet/browser/tx_storage_delegate_impl.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "brave/components/brave_wallet/common/brave_wallet.mojom.h"
#include "components/prefs/pref_service.h"
#include "components/value_store/value_store_factory_impl.h"
#include "components/value_store/value_store_frontend.h"
#include "components/value_store/value_store_task_runner.h"

namespace brave_wallet {

// DEPRECATED 01/2024. For migration only.
std::optional<mojom::CoinType> GetCoinTypeFromPrefKey_DEPRECATED(
    std::string_view key);

namespace {

constexpr char kValueStoreDatabaseUMAClientName[] = "BraveWallet";
constexpr base::FilePath::CharType kWalletStorageName[] =
    FILE_PATH_LITERAL("Brave Wallet Storage");
// key used in transaction storage
constexpr char kStorageTransactionsKey[] = "transactions";

std::unique_ptr<value_store::ValueStoreFrontend> CreateValueStoreFrontend(
    const base::FilePath& wallet_base_directory) {
  return std::make_unique<value_store::ValueStoreFrontend>(
      base::MakeRefCounted<value_store::ValueStoreFactoryImpl>(
          wallet_base_directory),
      base::FilePath(kWalletStorageName), kValueStoreDatabaseUMAClientName,
      base::SequencedTaskRunner::GetCurrentDefault(),
      value_store::GetValueStoreTaskRunner());
}

}  // namespace

TxStorageDelegateImpl::TxStorageDelegateImpl(
    const base::FilePath& wallet_base_directory)
    : store_(CreateValueStoreFrontend(wallet_base_directory)) {
  Initialize();
}

TxStorageDelegateImpl::~TxStorageDelegateImpl() = default;

bool TxStorageDelegateImpl::IsInitialized() const {
  return initialized_;
}

void TxStorageDelegateImpl::Initialize() {
  store_->Get(kStorageTransactionsKey,
              base::BindOnce(&TxStorageDelegateImpl::OnTxsInitialRead,
                             weak_factory_.GetWeakPtr()));
}

void TxStorageDelegateImpl::OnTxsInitialRead(std::optional<base::Value> txs) {
  if (txs) {
    txs_ = std::move(txs->GetDict());
  }
  initialized_ = true;
  RunDBMigrations();
  if (on_initialized_callback_for_testing_) {
    std::move(on_initialized_callback_for_testing_).Run();
  }
}

void TxStorageDelegateImpl::RunDBMigrations() {
  bool schedule_write = false;

  // Placeholder for future DB migrations.

  if (schedule_write) {
    ScheduleWrite();
  }
}

void TxStorageDelegateImpl::ScheduleWrite() {
  if (disable_writes_for_testing_) {
    return;
  }

  DCHECK(initialized_) << "storage is not initialized yet";
  store_->Set(kStorageTransactionsKey, base::Value(txs_.Clone()));
}

void TxStorageDelegateImpl::DisableWritesForTesting(bool disable) {
  disable_writes_for_testing_ = disable;
}

void TxStorageDelegateImpl::SetOnInitializedCallbackForTesting(  // IN-TEST
    base::OnceClosure callback) {
  on_initialized_callback_for_testing_ = std::move(callback);
}

void TxStorageDelegateImpl::Clear() {
  txs_.clear();
  store_->Remove(kStorageTransactionsKey);
}

bool TxStorageDelegateIncognitoImpl::IsInitialized() const {
  return true;
}

void TxStorageDelegateIncognitoImpl::
    SetOnInitializedCallbackForTesting(  // IN-TEST
        base::OnceClosure callback) {
  std::move(callback).Run();
}

}  // namespace brave_wallet
