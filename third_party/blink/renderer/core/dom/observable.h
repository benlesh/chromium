// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_OBSERVABLE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_OBSERVABLE_H_

#include "base/types/pass_key.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"

namespace blink {

class ExecutionContext;
class ExceptionState;
template <typename IDLType>
class FrozenArray;
class ObservableInternalObserver;
class ObservableSubscriptionObserver;
class ScriptState;
class Subscriber;
class SharedObservable;
class ShareConfig;
class V8UnionObservableConfigOrSubscribeCallback;
class SubscribeOptions;
class V8CatchCallback;
class V8Mapper;
class V8Predicate;
class V8SubscribeCallback;
class V8UnionObservableInspectorOrObserverCallback;
class V8UnionObserverOrObserverCallback;
class V8VoidFunction;

// Implementation of the DOM `Observable` API. See
// https://github.com/WICG/observable and
// https://docs.google.com/document/d/1NEobxgiQO-fTSocxJBqcOOOVZRmXcTFg9Iqrhebb7bg/edit.
class CORE_EXPORT Observable : public ScriptWrappable,
                               public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // This delegate is an internal (non-web-exposed) version of
  // `V8SubscribeCallback` for `Observer`s that are created natively by the web
  // platform. `OnSubscribe()` owns the passed-in `Subscriber` so that the
  // delegate, like an actual JS `V8SubscribeCallback`, can forward events to
  // the underlying observable subscriber.
  class SubscribeDelegate : public GarbageCollected<SubscribeDelegate> {
   public:
    virtual ~SubscribeDelegate() = default;
    virtual void OnSubscribe(Subscriber*, ScriptState*) = 0;
    virtual void Trace(Visitor* visitor) const {}
  };

  // Called by v8 bindings.
  static Observable* Create(ScriptState*,
                            const V8UnionObservableConfigOrSubscribeCallback*);

  Observable(ExecutionContext*, V8SubscribeCallback*);
  Observable(ExecutionContext*, SubscribeDelegate*);

  // API methods:
  void subscribe(ScriptState*,
                 V8UnionObserverOrObserverCallback*,
                 SubscribeOptions*);

  static Observable* from(ScriptState* script_state,
                          ScriptValue value,
                          ExceptionState& exception_state);

  // Observable-returning operators. See
  // https://wicg.github.io/observable/#observable-returning-operators.
  Observable* takeUntil(ScriptState*, ScriptValue, ExceptionState&);
  Observable* map(ScriptState*, V8Mapper*);
  Observable* filter(ScriptState*, V8Predicate*);
  Observable* take(ScriptState*, uint64_t);
  Observable* drop(ScriptState*, uint64_t);
  // `flatMap()` and `switchMap()` do not actually throw exceptions to script,
  // but we need access to the `exception_state` to determine if future calls to
  // `from()` succeeded or failed. In the failure case, we clear the exception
  // from the stack and report it to the relevant `Subscriber`.
  Observable* flatMap(ScriptState*, V8Mapper*, ExceptionState& exception_state);
  Observable* switchMap(ScriptState*,
                        V8Mapper*,
                        ExceptionState& exception_state);
  Observable* inspect(ScriptState*,
                      V8UnionObservableInspectorOrObserverCallback*);
  // See documentation above `flatMap()` and `switchMap()` for why this method
  // accepts an `exception_state`, even though it does not throw an exception
  // itself.
  //
  // This is the implementation for the `Observable#catch()` method, although
  // internally in C++ it has to be named something other than `catch()` due to
  // `catch` being a language keyword.
  Observable* catchImpl(ScriptState*, V8CatchCallback*, ExceptionState&);
  Observable* finally(ScriptState*, V8VoidFunction*);

  SharedObservable* share(ScriptState*, const ShareConfig*, ExceptionState&);
  const FrozenArray<SharedObservable>* sharedSources() const;
  void InitializeSharedSources(const HeapVector<Member<SharedObservable>>&);
  Observable* PropagateSharedSources(Observable* result,
                                     Observable* other = nullptr);
  virtual SharedObservable* AsSharedObservable() { return nullptr; }
  static bool IsContextRunnable(ExecutionContext*);

  void Trace(Visitor*) const override;

  // The `subscribe()` API is used when web content subscribes to an Observable
  // with a `V8UnionObserverOrObserverCallback`, whereas this API is used when
  // native code subscribes to an `Observable` with a native internal observer.
  // For consistency with the web-exposed `subscribe()` method, the
  // `ScriptState` does not have to be associated with a valid context.
  void SubscribeWithNativeObserver(ScriptState*,
                                   ObservableInternalObserver*,
                                   SubscribeOptions*);

 protected:
  Subscriber* CreateSubscriber(ScriptState*,
                               ObservableSubscriptionObserver*,
                               SubscribeOptions*,
                               SharedObservable* shared_owner = nullptr);
  void StartProducer(Subscriber*, ScriptState*);

 private:
  // The `ScriptState` argument does not need to be associated with a valid
  // context (this method early-returns in that case).
  void SubscribeInternal(ScriptState*,
                         V8UnionObserverOrObserverCallback*,
                         ObservableInternalObserver*,
                         SubscribeOptions*);

  // Exactly one of `subscribe_callback_` and `subscribe_delegate_` must be
  // non-null. `subscribe_callback_` is non-null when `this` is created from
  // script, and the subscribe callback is a JS-provided callback function,
  // whereas `subscribe_delegate_` is used for `Observable`s created internally
  // in C++, where the subscription steps are native steps.
  //
  // `subscribe_callback_` gets called when the `subscribe` method is invoked.
  // When run, is run, errors are caught and "reported":
  // https://html.spec.whatwg.org/C#report-the-exception.
  const Member<V8SubscribeCallback> subscribe_callback_;
  const Member<SubscribeDelegate> subscribe_delegate_;

  // Initialized once by the construction/composition path, before exposure.
  Member<FrozenArray<SharedObservable>> shared_sources_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_OBSERVABLE_H_
