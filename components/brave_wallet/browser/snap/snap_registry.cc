/* Copyright (c) 2024 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_wallet/browser/snap/snap_registry.h"

#include "base/containers/fixed_flat_map.h"
#include "base/no_destructor.h"
#include "components/grit/brave_components_resources.h"

namespace brave_wallet {

SnapManifest::SnapManifest() = default;
SnapManifest::SnapManifest(const SnapManifest&) = default;
SnapManifest& SnapManifest::operator=(const SnapManifest&) = default;
SnapManifest::SnapManifest(SnapManifest&&) = default;
SnapManifest& SnapManifest::operator=(SnapManifest&&) = default;
SnapManifest::~SnapManifest() = default;

namespace {

// Built-in snap allowlist. Extend this list as additional snaps are bundled.
const auto& GetBuiltinSnaps() {
  static const base::NoDestructor<std::vector<SnapManifest>> kBuiltinSnaps([] {
    std::vector<SnapManifest> snaps;

    // Cosmos snap — derives Cosmos keys via snap_getBip44Entropy(coinType=118).
    // Bundle is embedded via IDR_BRAVE_WALLET_COSMOS_SNAP_JS.
    SnapManifest cosmos;
    cosmos.snap_id             = "npm:@cosmsnap/snap";
    cosmos.version             = "0.1.22";
    cosmos.allowed_permissions = {"snap_getBip44Entropy"};
    cosmos.resource_id         = IDR_BRAVE_WALLET_COSMOS_SNAP_JS;
    snaps.push_back(std::move(cosmos));

    // Filsnap — derives Filecoin keys via snap_getBip44Entropy(coinType=1 for
    // testnet, coinType=461 for mainnet). Uses snap_dialog for confirmations,
    // snap_manageState for persistent state, endowment:page-home for homepage.
    // Bundle is embedded via IDR_BRAVE_WALLET_FILECOIN_SNAP_JS.
    SnapManifest filecoin;
    filecoin.snap_id             = "npm:filsnap";
    filecoin.version             = "1.6.1";
    filecoin.allowed_permissions = {"snap_getBip44Entropy", "snap_dialog",
                                    "snap_manageState"};
    filecoin.resource_id         = IDR_BRAVE_WALLET_FILECOIN_SNAP_JS;
    snaps.push_back(std::move(filecoin));

    // PolkaGate snap — Polkadot ecosystem snap with homepage support.
    // Uses snap_getBip44Entropy (coinType=354 for DOT, coinType=434 for KSM),
    // snap_dialog for confirmations, snap_manageState, endowment:page-home.
    // Bundle is embedded via IDR_BRAVE_WALLET_POLKADOT_SNAP_JS.
    SnapManifest polkadot;
    polkadot.snap_id             = "npm:@polkagate/snap";
    polkadot.version             = "2.5.1";
    polkadot.allowed_permissions = {"snap_getBip44Entropy", "snap_dialog",
                                    "snap_manageState"};
    polkadot.resource_id         = IDR_BRAVE_WALLET_POLKADOT_SNAP_JS;
    snaps.push_back(std::move(polkadot));

    return snaps;
  }());
  return *kBuiltinSnaps;
}

}  // namespace

// static
std::optional<SnapManifest> SnapRegistry::GetManifest(
    const std::string& snap_id) {
  for (const auto& manifest : GetBuiltinSnaps()) {
    if (manifest.snap_id == snap_id) {
      return manifest;
    }
  }
  return std::nullopt;
}

// static
bool SnapRegistry::IsKnownSnap(const std::string& snap_id) {
  return GetManifest(snap_id).has_value();
}

}  // namespace brave_wallet
