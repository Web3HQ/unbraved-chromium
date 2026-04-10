/* Copyright (c) 2023 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_COMPONENTS_BRAVE_WALLET_BROWSER_TX_STORAGE_DELEGATE_H_
#define BRAVE_COMPONENTS_BRAVE_WALLET_BROWSER_TX_STORAGE_DELEGATE_H_

#include "base/functional/callback.h"
#include "base/values.h"

namespace brave_wallet {

class TxStorageDelegate {
 public:
  virtual ~TxStorageDelegate() = default;

  virtual bool IsInitialized() const = 0;
  virtual void ScheduleWrite() = 0;
  virtual void Clear() = 0;
  virtual void SetOnInitializedCallbackForTesting(
      base::OnceClosure callback) = 0;

  base::DictValue& GetTxs() { return txs_; }

 protected:
  // In memory txs which will be read during initialization from db and schedule
  // write to it when changed. We only hold 500 confirmed and 500 rejected
  // txs, once the limit is reached we will retire oldest entries.
  base::DictValue txs_;
};

}  // namespace brave_wallet
   //
#endif  // BRAVE_COMPONENTS_BRAVE_WALLET_BROWSER_TX_STORAGE_DELEGATE_H_
