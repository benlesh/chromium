// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/observable.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class ObservableTest : public PageTestBase {};

TEST_F(ObservableTest, BackForwardCachedDocumentIsNotRunnable) {
  auto* window = GetDocument().domWindow();
  ASSERT_TRUE(GetDocument().IsActive());
  ASSERT_TRUE(Observable::IsContextRunnable(window));

  window->SetIsInBackForwardCache(true);
  // IsActive alone is insufficient: BFCache does not shut the Document down.
  EXPECT_TRUE(GetDocument().IsActive());
  EXPECT_FALSE(Observable::IsContextRunnable(window));

  window->SetIsInBackForwardCache(false);
  EXPECT_TRUE(Observable::IsContextRunnable(window));
}

}  // namespace blink
