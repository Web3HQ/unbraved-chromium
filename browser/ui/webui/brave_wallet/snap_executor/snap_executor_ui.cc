/* Copyright (c) 2024 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/ui/webui/brave_wallet/snap_executor/snap_executor_ui.h"

#include "brave/components/constants/webui_url_constants.h"
#include "brave/components/snap_executor/resources/grit/snap_executor_generated_map.h"
#include "components/grit/brave_components_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"

namespace snap_executor {

UntrustedSnapExecutorUI::UntrustedSnapExecutorUI(content::WebUI* web_ui)
    : ui::UntrustedWebUIController(web_ui) {
  auto* untrusted_source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(), kUntrustedSnapExecutorURL);

  untrusted_source->SetDefaultResource(IDR_BRAVE_WALLET_SNAP_EXECUTOR_HTML);
  untrusted_source->AddResourcePaths(kSnapExecutorGenerated);
  // Cosmos snap bundle — fetched by snap_executor.ts via fetch('snap-bundles/cosmos.js').
  // Served from this origin so WebUIDataSource handles gzip decompression automatically.
  untrusted_source->AddResourcePath("snap-bundles/cosmos.js",
                                    IDR_BRAVE_WALLET_COSMOS_SNAP_JS);
  // Filecoin snap bundle — fetched via fetch('snap-bundles/filecoin.js').
  untrusted_source->AddResourcePath("snap-bundles/filecoin.js",
                                    IDR_BRAVE_WALLET_FILECOIN_SNAP_JS);
  // Polkadot snap bundle — fetched via fetch('snap-bundles/polkadot.js').
  untrusted_source->AddResourcePath("snap-bundles/polkadot.js",
                                    IDR_BRAVE_WALLET_POLKADOT_SNAP_JS);

  // Only allow embedding from wallet page and wallet panel.
  untrusted_source->AddFrameAncestor(GURL(kBraveUIWalletPageURL));
  untrusted_source->AddFrameAncestor(GURL(kBraveUIWalletPanelURL));

  // Strict CSP: snaps must not make network requests or load sub-frames.
  // 'unsafe-eval' is required because SES Compartment.evaluate() uses
  // new Function() internally to run snap code in an isolated scope.
  untrusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src 'self' 'unsafe-eval';");
  // Disable Trusted Types — SES Compartment passes a plain string to
  // new Function() which violates the default TT policy on WebUI pages.
  untrusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::RequireTrustedTypesFor,
      "");
  // 'self' for snap bundle fetch(); '*' for snap endowment:network-access
  // (Cosmos snap queries chain RPC/REST endpoints).
  untrusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ConnectSrc,
      "connect-src 'self' *;");
  untrusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameSrc,
      "frame-src 'none';");
}

UntrustedSnapExecutorUI::~UntrustedSnapExecutorUI() = default;

std::unique_ptr<content::WebUIController>
UntrustedSnapExecutorUIConfig::CreateWebUIController(content::WebUI* web_ui,
                                                     const GURL& url) {
  return std::make_unique<UntrustedSnapExecutorUI>(web_ui);
}

UntrustedSnapExecutorUIConfig::UntrustedSnapExecutorUIConfig()
    : WebUIConfig(content::kChromeUIUntrustedScheme,
                  kUntrustedSnapExecutorHost) {}

}  // namespace snap_executor
