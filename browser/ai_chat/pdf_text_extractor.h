// Copyright (c) 2026 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BRAVE_BROWSER_AI_CHAT_PDF_TEXT_EXTRACTOR_H_
#define BRAVE_BROWSER_AI_CHAT_PDF_TEXT_EXTRACTOR_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "brave/components/restricted_web_contents_delegate/restricted_web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace ai_chat {

// Extracts text from a PDF by loading it in a hidden background WebContents.
// The PDF viewer extension + ScreenAI OCR pipeline runs, then page text is
// extracted via PDFDocumentHelper::GetPageText().
//
// Two entry points:
//   - ExtractText(browser_context, pdf_path, callback)
//       Uses an existing file path directly (e.g. from file picker).
//   - ExtractText(browser_context, pdf_bytes, callback)
//       Writes bytes to a temp file first (e.g. from drag-and-drop).
//
// The extractor should be kept alive until the callback fires.
class PdfTextExtractor : public RestrictedWebContentsDelegate,
                         public content::WebContentsObserver {
 public:
  using ExtractTextCallback =
      base::OnceCallback<void(std::optional<std::string>)>;

  PdfTextExtractor();
  ~PdfTextExtractor() override;

  PdfTextExtractor(const PdfTextExtractor&) = delete;
  PdfTextExtractor& operator=(const PdfTextExtractor&) = delete;

  // Use an existing file path directly (no temp file created).
  void ExtractText(content::BrowserContext* browser_context,
                   const base::FilePath& pdf_path,
                   ExtractTextCallback callback);

  // Write bytes to a temp file first, then extract.
  void ExtractText(content::BrowserContext* browser_context,
                   std::vector<uint8_t> pdf_bytes,
                   ExtractTextCallback callback);

 private:
  // content::WebContentsObserver:
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;

  void OnTempFileWritten(content::BrowserContext* browser_context,
                         std::optional<base::FilePath> temp_path);
  void LoadPdfInWebContents(content::BrowserContext* browser_context,
                            const base::FilePath& pdf_path);
  void TryRegisterForDocumentLoad();
  void OnDocumentLoadComplete();
  void OnTimeout();
  void Finish(std::optional<std::string> result);
  void Cleanup();

  ExtractTextCallback callback_;
  std::unique_ptr<content::WebContents> web_contents_;
  base::FilePath temp_file_path_;
  base::OneShotTimer timeout_timer_;
  bool registered_for_load_ = false;
  base::WeakPtrFactory<PdfTextExtractor> weak_ptr_factory_{this};
};

}  // namespace ai_chat

#endif  // BRAVE_BROWSER_AI_CHAT_PDF_TEXT_EXTRACTOR_H_
