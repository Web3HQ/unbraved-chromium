/* Copyright (c) 2024 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_wallet/browser/snap/snap_controller.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/values.h"
#include "brave/components/brave_wallet/browser/keyring_service.h"
#include "brave/components/brave_wallet/browser/snap/snap_registry.h"

namespace brave_wallet {

SnapController::SnapController(KeyringService* keyring_service)
    : keyring_service_(keyring_service) {
  DCHECK(keyring_service_);
}

SnapController::~SnapController() = default;

void SnapController::BindSnapRequestHandler(
    mojo::PendingReceiver<mojom::SnapRequestHandler> receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

void SnapController::SetSnapBridge(
    mojo::PendingRemote<mojom::SnapBridge> bridge) {
  LOG(ERROR) << "XXXZZZ SnapController::SetSnapBridge called, pending_callbacks="
             << pending_callbacks_.size();
  snap_bridge_.reset();
  snap_bridge_.Bind(std::move(bridge));
  snap_bridge_.set_disconnect_handler(
      base::BindOnce(&SnapController::OnSnapBridgeDisconnected,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SnapController::OnSnapBridgeDisconnected() {
  LOG(ERROR) << "XXXZZZ SnapController::OnSnapBridgeDisconnected — failing "
             << pending_callbacks_.size() << " pending callback(s)";
  for (size_t i = 0; i < pending_callbacks_.size(); ++i) {
    FailPendingCallback(i, "SnapBridge disconnected");
  }
}

void SnapController::FailPendingCallback(size_t index,
                                         const std::string& error) {
  if (index >= pending_callbacks_.size() || !pending_callbacks_[index]) {
    return;
  }
  std::move(pending_callbacks_[index]).Run(std::nullopt, error);
}

void SnapController::InvokeSnap(const std::string& snap_id,
                                const std::string& method,
                                base::Value params,
                                SnapResultCallback callback) {
  LOG(ERROR) << "XXXZZZ SnapController::InvokeSnap snap_id=" << snap_id
             << " method=" << method;

  if (!snap_bridge_.is_connected()) {
    LOG(ERROR) << "XXXZZZ SnapController::InvokeSnap ERROR: SnapBridge not connected";
    std::move(callback).Run(std::nullopt, "SnapBridge is not connected");
    return;
  }

  auto manifest = SnapRegistry::GetManifest(snap_id);
  if (!manifest) {
    LOG(ERROR) << "XXXZZZ SnapController::InvokeSnap ERROR: unknown snap " << snap_id;
    std::move(callback).Run(std::nullopt, "Unknown snap: " + snap_id);
    return;
  }

  // Store the callback so OnSnapBridgeDisconnected can fail it if the
  // SnapBridge pipe closes before InvokeSnap completes.
  const size_t cb_index = pending_callbacks_.size();
  pending_callbacks_.push_back(std::move(callback));

  // The snap bundle is served by chrome-untrusted://snap-executor/ and fetched
  // directly by snap_executor.ts — no source string passes through Mojo.
  snap_bridge_->LoadSnap(
      snap_id,
      base::BindOnce(&SnapController::OnLoadSnapResult,
                     weak_ptr_factory_.GetWeakPtr(), cb_index, snap_id, method,
                     std::move(params)));
}

void SnapController::HandleSnapRequest(const std::string& snap_id,
                                       const std::string& method,
                                       base::Value params,
                                       HandleSnapRequestCallback callback) {
  LOG(ERROR) << "XXXZZZ SnapController::HandleSnapRequest snap_id=" << snap_id
             << " method=" << method;
  if (method == "snap_getBip44Entropy") {
    HandleGetBip44Entropy(snap_id, std::move(params), std::move(callback));
  } else if (method == "snap_getEntropy") {
    HandleGetEntropy(snap_id, std::move(params), std::move(callback));
  } else if (method == "snap_dialog" || method == "snap_confirm") {
    // snap_confirm is the legacy name for snap_dialog with type "confirmation".
    // Both auto-confirm for now — real UI TBD.
    LOG(ERROR) << "XXXZZZ SnapController::HandleSnapRequest " << method
               << " auto-confirmed";
    std::move(callback).Run(base::Value(true), std::nullopt);
  } else if (method == "snap_notify") {
    // Notifications are fire-and-forget; acknowledge silently.
    LOG(ERROR) << "XXXZZZ SnapController::HandleSnapRequest snap_notify received";
    std::move(callback).Run(std::nullopt, std::nullopt);
  } else {
    LOG(ERROR) << "XXXZZZ SnapController::HandleSnapRequest unsupported method="
               << method;
    std::move(callback).Run(std::nullopt,
                            "Unsupported snap method: " + method);
  }
}

// Private ------------------------------------------------------------------

void SnapController::HandleGetBip44Entropy(
    const std::string& snap_id,
    base::Value params,
    HandleSnapRequestCallback callback) {
  LOG(ERROR) << "XXXZZZ SnapController::HandleGetBip44Entropy snap_id="
             << snap_id;
  // params: { "coinType": <uint32> }
  const base::Value::Dict* dict = params.GetIfDict();
  if (!dict) {
    LOG(ERROR) << "XXXZZZ SnapController::HandleGetBip44Entropy ERROR: params not a dict";
    std::move(callback).Run(std::nullopt,
                            "snap_getBip44Entropy: params must be a dict");
    return;
  }

  std::optional<int> coin_type = dict->FindInt("coinType");
  if (!coin_type || *coin_type < 0) {
    LOG(ERROR) << "XXXZZZ SnapController::HandleGetBip44Entropy ERROR: missing/invalid coinType";
    std::move(callback).Run(std::nullopt,
                            "snap_getBip44Entropy: missing or invalid coinType");
    return;
  }
  LOG(ERROR) << "XXXZZZ SnapController::HandleGetBip44Entropy coinType=" << *coin_type;

  keyring_service_->GetBip44EntropyForSnap(
      static_cast<uint32_t>(*coin_type),
      base::BindOnce(
          [](HandleSnapRequestCallback cb,
             std::optional<base::Value> result) {
            if (!result) {
              LOG(ERROR) << "XXXZZZ SnapController::HandleGetBip44Entropy ERROR: "
                            "key derivation failed (wallet locked?)";
              std::move(cb).Run(
                  std::nullopt,
                  "snap_getBip44Entropy: key derivation failed "
                  "(wallet may be locked)");
              return;
            }
            LOG(ERROR) << "XXXZZZ SnapController::HandleGetBip44Entropy: entropy derived OK";
            std::move(cb).Run(std::move(result), std::nullopt);
          },
          std::move(callback)));
}

void SnapController::HandleGetEntropy(const std::string& snap_id,
                                      base::Value params,
                                      HandleSnapRequestCallback callback) {
  // TODO(snap): derive snap-specific entropy from KeyringService using the
  // snap's ID as a salt, and return it.
  // Placeholder until the full key derivation flow is implemented.
  std::move(callback).Run(std::nullopt, "snap_getEntropy not yet implemented");
}

void SnapController::OnLoadSnapResult(
    size_t cb_index,
    const std::string& snap_id,
    const std::string& method,
    base::Value params,
    bool success,
    const std::optional<std::string>& error) {
  LOG(ERROR) << "XXXZZZ SnapController::OnLoadSnapResult snap_id=" << snap_id
             << " method=" << method << " success=" << success
             << (error ? " error=" + *error : "");
  if (!success) {
    LOG(ERROR) << "XXXZZZ SnapController::OnLoadSnapResult ERROR: "
               << error.value_or("unknown error");
    FailPendingCallback(
        cb_index, error.value_or("Failed to load snap: unknown error"));
    return;
  }

  if (!snap_bridge_.is_connected()) {
    LOG(ERROR) << "XXXZZZ SnapController::OnLoadSnapResult ERROR: "
                  "SnapBridge disconnected before InvokeSnap";
    FailPendingCallback(cb_index, "SnapBridge disconnected");
    return;
  }

  snap_bridge_->InvokeSnap(
      snap_id, method, std::move(params),
      base::BindOnce(&SnapController::OnInvokeSnapResult,
                     weak_ptr_factory_.GetWeakPtr(), cb_index));
}

void SnapController::OnInvokeSnapResult(
    size_t cb_index,
    std::optional<base::Value> result,
    const std::optional<std::string>& error) {
  LOG(ERROR) << "XXXZZZ SnapController::OnInvokeSnapResult"
             << " has_result=" << result.has_value()
             << (error ? " error=" + *error : "");
  if (cb_index >= pending_callbacks_.size() || !pending_callbacks_[cb_index]) {
    return;  // Already failed by OnSnapBridgeDisconnected.
  }
  std::move(pending_callbacks_[cb_index]).Run(std::move(result), error);
}

}  // namespace brave_wallet
