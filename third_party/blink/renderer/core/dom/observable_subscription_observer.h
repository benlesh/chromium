// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_OBSERVABLE_SUBSCRIPTION_OBSERVER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_OBSERVABLE_SUBSCRIPTION_OBSERVER_H_

#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/observable_internal_observer.h"

namespace blink {

// A distinct registration for each subscription, even when native callers
// reuse an observer. Cancellation is checked in O(1) during snapshot delivery.
class ObservableSubscriptionObserver final : public ObservableInternalObserver {
 public:
  ObservableSubscriptionObserver(ObservableInternalObserver* observer,
                                 AbortSignal* signal)
      : observer_(observer), signal_(signal) {}

  void Detach() { detached_ = true; }
  bool IsCanceled() const {
    // Keep checking the signal after terminal cleanup has removed the abort
    // algorithms: an earlier terminal callback can cancel a later recipient.
    return detached_ || (signal_ && signal_->aborted());
  }

  void Next(ScriptValue value) override {
    if (!IsCanceled())
      observer_->Next(value);
  }
  void Error(ScriptState* state, ScriptValue value) override {
    if (!IsCanceled())
      observer_->Error(state, value);
  }
  void Complete() override {
    if (!IsCanceled())
      observer_->Complete();
  }
  void Trace(Visitor* visitor) const override {
    visitor->Trace(observer_);
    visitor->Trace(signal_);
    ObservableInternalObserver::Trace(visitor);
  }

 private:
  Member<ObservableInternalObserver> observer_;
  Member<AbortSignal> signal_;
  bool detached_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_OBSERVABLE_SUBSCRIPTION_OBSERVER_H_
