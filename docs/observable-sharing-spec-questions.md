# Questions prepared for WICG/observable #222

These questions are based on tracing the draft and the Chromium prototype.
The native build has not been run. This text is prepared for a public comment;
it has not been posted to #222.

## Which realm owns a shared execution?

The prototype creates the Subscriber in the realm of the first `subscribe()`
call. That makes the execution's lifetime depend on which realm subscribes first:

```js
const frame = document.createElement('iframe');
document.body.append(frame);
let sink;
let starts = 0;
let teardowns = 0;
const shared = new SharedObservable(s => {
  ++starts;
  sink = s;
  s.addTeardown(() => ++teardowns);
});

frame.contentWindow.Observable.prototype.subscribe.call(shared, () => {});
frame.remove();

const values = [];
shared.subscribe(value => values.push(value));
sink.next(1);
sink.complete();
```

If the Subscriber belongs to the removed iframe, its fully-active checks
suppress `next()` and `complete()`, but the shared owner still has that current
execution. A later subscription in the surviving owner realm joins it without
restarting. Normal termination and teardown do not happen through those calls.

Can we explicitly create a shared execution and its control state in the
SharedObservable's relevant realm? We should also define what happens to
observations when their consumer realm is destroyed, and how the shared
execution is disposed when its owner realm goes away. Removing the fully-active
checks would allow script to run in dead Documents, so that is not a suitable fix.

## SharedObservable's constructor prototype needs a Web IDL rule

The draft says SharedObservable.prototype inherits from Observable.prototype,
while `Object.getPrototypeOf(SharedObservable)` is Function.prototype. Normal
[Web IDL interface-object inheritance](https://webidl.spec.whatwg.org/#interface-object)
makes the latter Observable, and idlharness checks that rule from
`interface SharedObservable : Observable`.

The Chromium draft now expresses the exception using an internal binding
attribute instead of a hard-coded interface name. That is implementation
plumbing: the exported draft IDL still cannot express the exception, and the
corresponding idlharness assertion remains incompatible.

Should we propose a Web IDL mechanism and matching idlharness support, or use
ordinary constructor inheritance? With ordinary inheritance, we should specify
whether SharedObservable.from simply inherits Observable.from's identity-
preserving conversion or gets its own explicitly defined behavior. Giving it
a dedicated static method would need that behavior spelled out too.
