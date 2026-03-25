/* Copyright (c) 2024 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_COMPONENTS_BRAVE_WALLET_BROWSER_SNAP_SNAP_CONTROLLER_H_
#define BRAVE_COMPONENTS_BRAVE_WALLET_BROWSER_SNAP_SNAP_CONTROLLER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "brave/components/brave_wallet/common/brave_wallet.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace brave_wallet {

class KeyringService;

// SnapController is the C++ orchestrator for Brave Wallet Snaps.
//
// Responsibilities:
//   - Implements mojom::SnapRequestHandler so the wallet page TS can relay
//     snap.request() calls back to C++.
//   - Holds a mojom::SnapBridge remote to load/invoke/unload snap iframes
//     on the wallet page.
//   - Provides InvokeSnap() for EthereumProviderImpl to handle
//     wallet_invokeSnap / wallet_snap RPC methods.
class SnapController : public mojom::SnapRequestHandler {
 public:
  using SnapResultCallback =
      base::OnceCallback<void(std::optional<base::Value> result,
                              std::optional<std::string> error)>;

  explicit SnapController(KeyringService* keyring_service);
  ~SnapController() override;

  SnapController(const SnapController&) = delete;
  SnapController& operator=(const SnapController&) = delete;

  // Binds the SnapRequestHandler receiver endpoint so TS can call us.
  void BindSnapRequestHandler(
      mojo::PendingReceiver<mojom::SnapRequestHandler> receiver);

  // Sets the SnapBridge remote (wallet page TS). Called during wallet page
  // initialisation (see step 12 / PageHandlerFactory::CreatePageHandler).
  void SetSnapBridge(mojo::PendingRemote<mojom::SnapBridge> bridge);

  // Invoked by EthereumProviderImpl for wallet_invokeSnap / wallet_snap.
  // Loads the snap if not already loaded, then calls InvokeSnap on the bridge.
  void InvokeSnap(const std::string& snap_id,
                  const std::string& method,
                  base::Value params,
                  SnapResultCallback callback);

  // mojom::SnapRequestHandler --------------------------------------------
  //
  // Called by the wallet page TS when snap code executes snap.request().
  // Dispatches the request to the appropriate C++ service and returns the
  // result back through the Mojo callback.
  void HandleSnapRequest(const std::string& snap_id,
                         const std::string& method,
                         base::Value params,
                         HandleSnapRequestCallback callback) override;

 private:
  // HandleSnapRequest dispatch targets.
  void HandleGetBip44Entropy(const std::string& snap_id,
                             base::Value params,
                             HandleSnapRequestCallback callback);
  void HandleGetEntropy(const std::string& snap_id,
                        base::Value params,
                        HandleSnapRequestCallback callback);

  // Called when the SnapBridge Mojo pipe disconnects. Fails all pending
  // callbacks so their EthereumProvider responders are not destroyed silently.
  void OnSnapBridgeDisconnected();

  // Callbacks from the SnapBridge remote.
  void OnLoadSnapResult(size_t callback_index,
                        const std::string& snap_id,
                        const std::string& method,
                        base::Value params,
                        bool success,
                        const std::optional<std::string>& error);
  void OnInvokeSnapResult(size_t callback_index,
                          std::optional<base::Value> result,
                          const std::optional<std::string>& error);

  // Drains one pending callback with an error. No-op if index is stale.
  void FailPendingCallback(size_t index, const std::string& error);

  raw_ptr<KeyringService> keyring_service_;
  mojo::Remote<mojom::SnapBridge> snap_bridge_;
  mojo::Receiver<mojom::SnapRequestHandler> receiver_{this};

  // Pending SnapResultCallbacks indexed by slot. A null entry means the slot
  // was already consumed (invoke completed or failed).
  std::vector<SnapResultCallback> pending_callbacks_;

  base::WeakPtrFactory<SnapController> weak_ptr_factory_{this};
};

}  // namespace brave_wallet

#endif  // BRAVE_COMPONENTS_BRAVE_WALLET_BROWSER_SNAP_SNAP_CONTROLLER_H_
