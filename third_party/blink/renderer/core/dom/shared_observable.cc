// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/shared_observable.h"

#include <cmath>

#include "third_party/blink/renderer/bindings/core/v8/frozen_array.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_share_config.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_shared_observable_config.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_subscribe_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_sharedobservableconfig_subscribecallback.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/observable_internal_observer.h"
#include "third_party/blink/renderer/core/dom/subscriber.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"

namespace blink {
namespace {

template <typename Config>
bool NormalizeSettings(ScriptState* state,
                       const Config* config,
                       SharedObservable::Settings& settings,
                       ExceptionState& exception_state) {
  if (config->hasMaxReplayValues() &&
      !config->maxReplayValues().V8Value()->IsUndefined()) {
    const double capacity =
        NativeValueTraits<IDLUnrestrictedDouble>::NativeValue(
            state->GetIsolate(), config->maxReplayValues().V8Value(),
            exception_state);
    if (exception_state.HadException())
      return false;
    if (!std::isfinite(capacity) || capacity < 0 ||
        std::floor(capacity) != capacity || capacity > 9007199254740991.0) {
      exception_state.ThrowRangeError(
          "maxReplayValues must be a finite, nonnegative safe integer.");
      return false;
    }
    settings.max_replay_values = static_cast<uint64_t>(capacity);
  }
  if (config->hasCloseOnComplete())
    settings.close_on_complete = config->closeOnComplete();
  if (config->hasCloseOnError())
    settings.close_on_error = config->closeOnError();
  if (config->hasCloseOnLastUnsubscribe())
    settings.close_on_last_unsubscribe = config->closeOnLastUnsubscribe();
  if (config->hasKeepAliveOnLastUnsubscribe())
    settings.keep_alive_on_last_unsubscribe =
        config->keepAliveOnLastUnsubscribe();
  if (settings.close_on_last_unsubscribe &&
      settings.keep_alive_on_last_unsubscribe) {
    exception_state.ThrowTypeError(
        "closeOnLastUnsubscribe and keepAliveOnLastUnsubscribe cannot both be "
        "true.");
    return false;
  }
  return true;
}

class SharedForwardingDelegate final : public Observable::SubscribeDelegate {
 public:
  explicit SharedForwardingDelegate(Observable* source) : source_(source) {}
  void OnSubscribe(Subscriber* subscriber, ScriptState* state) override {
    auto* options = MakeGarbageCollected<SubscribeOptions>();
    options->setSignal(subscriber->signal());
    source_->SubscribeWithNativeObserver(
        state, MakeGarbageCollected<Forwarder>(subscriber, state), options);
  }
  void Trace(Visitor* visitor) const override {
    visitor->Trace(source_);
    Observable::SubscribeDelegate::Trace(visitor);
  }

 private:
  class Forwarder final : public ObservableInternalObserver {
   public:
    Forwarder(Subscriber* subscriber, ScriptState* state)
        : subscriber_(subscriber), state_(state) {}
    void Next(ScriptValue value) override { subscriber_->next(value); }
    void Error(ScriptState* state, ScriptValue error) override {
      subscriber_->error(state, error);
    }
    void Complete() override { subscriber_->complete(state_); }
    void Trace(Visitor* visitor) const override {
      visitor->Trace(subscriber_);
      visitor->Trace(state_);
      ObservableInternalObserver::Trace(visitor);
    }

   private:
    Member<Subscriber> subscriber_;
    Member<ScriptState> state_;
  };
  Member<Observable> source_;
};

}  // namespace

SharedObservable* SharedObservable::Create(
    ScriptState* state,
    const V8UnionSharedObservableConfigOrSubscribeCallback* input,
    ExceptionState& exception_state) {
  Settings settings;
  if (input->IsSubscribeCallback()) {
    return MakeGarbageCollected<SharedObservable>(
        ExecutionContext::From(state), input->GetAsSubscribeCallback(),
        settings);
  }
  auto* config = input->GetAsSharedObservableConfig();
  if (!NormalizeSettings(state, config, settings, exception_state))
    return nullptr;
  auto* observable = MakeGarbageCollected<SharedObservable>(
      ExecutionContext::From(state), config->producer(), settings);
  if (config->sharedSources())
    observable->InitializeSharedSources(*config->sharedSources());
  return observable;
}

SharedObservable* SharedObservable::Share(ScriptState* state,
                                          Observable* source,
                                          const ShareConfig* config,
                                          ExceptionState& exception_state) {
  auto* shared = source->AsSharedObservable();
  Settings settings = shared ? shared->settings_ : Settings();
  // Conversion and validation must precede the identity return, even for a
  // closed receiver. The dictionary getters have already run in the bindings.
  if (!NormalizeSettings(state, config, settings, exception_state))
    return nullptr;
  if (shared && settings == shared->settings_)
    return shared;
  auto* result = MakeGarbageCollected<SharedObservable>(
      ExecutionContext::From(state),
      MakeGarbageCollected<SharedForwardingDelegate>(source), settings);
  source->PropagateSharedSources(result);
  return result;
}

SharedObservable::SharedObservable(ExecutionContext* context,
                                   V8SubscribeCallback* producer,
                                   const Settings& settings)
    : Observable(context, producer), settings_(settings) {}

SharedObservable::SharedObservable(ExecutionContext* context,
                                   SubscribeDelegate* producer,
                                   const Settings& settings)
    : Observable(context, producer), settings_(settings) {}

uint64_t SharedObservable::activeSubscribers() const {
  uint64_t count =
      current_subscriber_ ? current_subscriber_->ObserverCount() : 0;
  for (auto& subscriber : closed_replay_subscribers_) {
    count += subscriber->ObserverCount();
  }
  return count;
}

const FrozenArray<IDLAny>* SharedObservable::replayValues() const {
  return MakeGarbageCollected<FrozenArray<IDLAny>>(ReplaySnapshot());
}

HeapVector<ScriptValue> SharedObservable::ReplaySnapshot() const {
  HeapVector<ScriptValue> snapshot;
  snapshot.ReserveInitialCapacity(replay_values_.size());
  for (wtf_size_t i = replay_start_; i < replay_values_.size(); ++i)
    snapshot.push_back(replay_values_[i]);
  for (wtf_size_t i = 0; i < replay_start_; ++i)
    snapshot.push_back(replay_values_[i]);
  return snapshot;
}

void SharedObservable::SubscribeShared(ScriptState* state,
                                       ObservableSubscriptionObserver* observer,
                                       SubscribeOptions* options) {
  AbortSignal* signal = options->hasSignal() ? options->signal() : nullptr;
  if (signal && signal->aborted())
    return;

  // Freeze the replay iteration before invoking any author code.
  const HeapVector<ScriptValue> history = ReplaySnapshot();
  if (terminal_ != Terminal::kNone) {
    auto* replay = CreateSubscriber(state, observer, options);
    closed_replay_subscribers_.push_back(replay);
    replay->ReplayTo(observer, history);
    closed_replay_subscribers_.pop_back();
    if (replay->active()) {
      if (terminal_ == Terminal::kError)
        replay->error(state, terminal_error_);
      else
        replay->complete(state);
    }
    return;
  }

  Subscriber* subscriber = current_subscriber_;
  const bool start = !subscriber;
  if (start) {
    subscriber = CreateSubscriber(state, observer, options, this);
    current_subscriber_ = subscriber;
  } else {
    subscriber->RegisterNewObserver(state, observer, options);
  }
  // Registration precedes replay, so nested live values interleave with this
  // fixed snapshot. The subscriber checks both execution and observer guards.
  subscriber->ReplayTo(observer, history);
  if (start)
    StartProducer(subscriber, state);
}

void SharedObservable::RecordNext(Subscriber* subscriber, ScriptValue value) {
  if (current_subscriber_ != subscriber || !settings_.max_replay_values)
    return;
  // Grow only as values arrive. Once full, overwrite the oldest slot in O(1).
  if (replay_values_.size() < settings_.max_replay_values) {
    replay_values_.push_back(value);
  } else {
    replay_values_[replay_start_] = value;
    if (++replay_start_ == replay_values_.size())
      replay_start_ = 0;
  }
}

void SharedObservable::EndExecution(Subscriber* subscriber,
                                    bool terminal,
                                    bool is_error,
                                    bool close,
                                    std::optional<ScriptValue> reason) {
  if (current_subscriber_ != subscriber)
    return;
  // This runs before any abort event, upstream cleanup, or teardown callback.
  current_subscriber_ = nullptr;
  const bool retain = terminal
                          ? close || (is_error ? settings_.close_on_error
                                               : settings_.close_on_complete)
                          : settings_.close_on_last_unsubscribe;
  if (retain) {
    terminal_ = terminal && is_error ? Terminal::kError : Terminal::kComplete;
    if (terminal_ == Terminal::kError)
      terminal_error_ = *reason;
  } else {
    replay_values_.clear();
    replay_start_ = 0;
  }
}

void SharedObservable::Trace(Visitor* visitor) const {
  visitor->Trace(current_subscriber_);
  visitor->Trace(replay_values_);
  visitor->Trace(terminal_error_);
  visitor->Trace(closed_replay_subscribers_);
  Observable::Trace(visitor);
}

}  // namespace blink
