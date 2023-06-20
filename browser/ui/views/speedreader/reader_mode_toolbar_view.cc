/* Copyright (c) 2023 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/ui/views/speedreader/reader_mode_toolbar_view.h"

#include <memory>

#include "brave/components/constants/webui_url_constants.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "content/public/browser/browser_context.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "url/gurl.h"

namespace content {
struct ContextMenuParams;
}

namespace {

class Toolbar : public views::WebView {
 public:
  explicit Toolbar(content::BrowserContext* browser_context)
      : views::WebView(browser_context) {
    LoadInitialURL(GURL(kSpeedreaderPanelURL));
    EnableSizingFromWebContents(gfx::Size(10, 10), gfx::Size(10000, 500));
  }

 private:
  // WebView:
  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) override {
    // Ignore context menu.
    return true;
  }

  gfx::Size CalculatePreferredSize() const override {
    // Returning non-empty size to enable sizing from web contents on Mac.
    return gfx::Size(1, 1);
  }
};

}  // namespace

ReaderModeToolbarView::ReaderModeToolbarView(
    content::BrowserContext* browser_context) {
  SetBackground(views::CreateThemedSolidBackground(kColorToolbar));
  SetBorder(views::CreateThemedSolidSidedBorder(
      gfx::Insets::TLBR(0, 0, 1, 0), kColorToolbarContentAreaSeparator));

  toolbar_ = std::make_unique<Toolbar>(browser_context);
  AddChildView(toolbar_.get());
}

ReaderModeToolbarView::~ReaderModeToolbarView() = default;

content::WebContents* ReaderModeToolbarView::GetWebContentsForTesting() {
  return toolbar_->web_contents();
}

gfx::Size ReaderModeToolbarView::CalculatePreferredSize() const {
  return toolbar_->GetPreferredSize();
}

void ReaderModeToolbarView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  views::View::OnBoundsChanged(previous_bounds);
  UpdateToolbarBounds();
}

void ReaderModeToolbarView::ChildPreferredSizeChanged(views::View* view) {
  if (view == toolbar_.get()) {
    UpdateToolbarBounds();
  }
}

void ReaderModeToolbarView::UpdateToolbarBounds() {
  auto toolbar_size = toolbar_->GetPreferredSize();

  auto toolbar_bounds = GetLocalBounds();
  toolbar_size.SetToMin(bounds().size());
  toolbar_bounds.ClampToCenteredSize(toolbar_size);
  toolbar_->SetBoundsRect(toolbar_bounds);
}
