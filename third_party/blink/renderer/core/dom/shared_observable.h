// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SHARED_OBSERVABLE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SHARED_OBSERVABLE_H_

#include <optional>

#include "third_party/blink/renderer/core/dom/observable.h"

namespace blink {

struct IDLAny;
class V8UnionSharedObservableConfigOrSubscribeCallback;

class CORE_EXPORT SharedObservable final : public Observable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  struct Settings {
    uint64_t max_replay_values = 0;
    bool close_on_complete = false;
    bool close_on_error = false;
    bool close_on_last_unsubscribe = false;
    bool keep_alive_on_last_unsubscribe = false;
    bool operator==(const Settings&) const = default;
  };

  static SharedObservable* Create(
      ScriptState*,
      const V8UnionSharedObservableConfigOrSubscribeCallback*,
      ExceptionState&);
  static SharedObservable* Share(ScriptState*,
                                 Observable*,
                                 const ShareConfig*,
                                 ExceptionState&);

  SharedObservable(ExecutionContext*, V8SubscribeCallback*, const Settings&);
  SharedObservable(ExecutionContext*, SubscribeDelegate*, const Settings&);
  SharedObservable* AsSharedObservable() override { return this; }

  uint64_t activeSubscribers() const;
  const FrozenArray<IDLAny>* replayValues() const;
  uint64_t maxReplayValues() const { return settings_.max_replay_values; }

  void SubscribeShared(ScriptState*,
                       ObservableSubscriptionObserver*,
                       SubscribeOptions*);
  void RecordNext(Subscriber*, ScriptValue);
  void EndExecution(Subscriber*,
                    bool terminal,
                    bool is_error,
                    bool close,
                    std::optional<ScriptValue> reason);
  bool KeepAlive() const { return settings_.keep_alive_on_last_unsubscribe; }

  void Trace(Visitor*) const override;

 private:
  HeapVector<ScriptValue> ReplaySnapshot() const;

  enum class Terminal { kNone, kComplete, kError };

  const Settings settings_;
  // Keep the execution, including its upstream cancellation machinery, alive
  // while the shared instance is reachable, even with no current observers.
  Member<Subscriber> current_subscriber_;
  HeapVector<ScriptValue> replay_values_;
  wtf_size_t replay_start_ = 0;
  Terminal terminal_ = Terminal::kNone;
  ScriptValue terminal_error_;
  // Closed subscriptions use independent cancellation state without acquiring
  // another producer execution. Count them until terminal delivery begins.
  HeapVector<Member<Subscriber>> closed_replay_subscribers_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SHARED_OBSERVABLE_H_
