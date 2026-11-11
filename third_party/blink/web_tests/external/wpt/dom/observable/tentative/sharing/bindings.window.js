// META: title=Observable sharing: cross-realm brands and document liveness
// META: spec=https://github.com/WICG/observable/pull/222

function makeRealm(t) {
  const frame = document.createElement("iframe");
  document.documentElement.append(frame);
  t.add_cleanup(() => frame.remove());
  return frame;
}

test(t => {
  const frame = makeRealm(t);
  const other = frame.contentWindow;
  const shared = new other.SharedObservable(() => {});
  assert_equals(Observable.from(shared), shared);
  assert_equals(other.Observable.from(new Observable(() => {})).constructor, Observable);
  const result = new Observable({producer() {}, sharedSources: [shared]});
  assert_equals(result.sharedSources[0], shared);
  assert_equals(Observable.prototype.share.call(shared), shared);
  assert_equals(Object.getPrototypeOf(other.SharedObservable), other.Function.prototype);
  assert_equals(Object.getPrototypeOf(other.SharedObservable.prototype),
                other.Observable.prototype);
}, "Conversion and metadata accept shared native instances from another realm");

test(t => {
  const frame = makeRealm(t);
  const other = frame.contentWindow;
  let sink;
  const shared = new other.SharedObservable({
    maxReplayValues: 1, producer(s) { sink = s; },
  });
  const stop = new AbortController();
  const values = [];
  other.Observable.prototype.subscribe.call(shared, value => values.push(value),
                                           {signal: stop.signal});
  sink.next(1);
  stop.abort();
  assert_false(sink.active);
  assert_equals(shared.activeSubscribers, 0);
  assert_array_equals(values, [1]);
}, "A consumer AbortSignal from another realm cancels its shared observation");

test(t => {
  const frame = makeRealm(t);
  const other = frame.contentWindow;
  let sink;
  const values = [];
  const shared = new other.SharedObservable({
    maxReplayValues: 1, producer(s) { sink = s; },
  });
  // Use the foreign subscription method to create the execution in that realm.
  other.Observable.prototype.subscribe.call(shared, value => values.push(value));
  sink.next(1);
  frame.remove();
  sink.next(2);
  sink.complete({close: true});
  assert_array_equals(values, [1]);
  assert_array_equals(shared.replayValues, [1]);
  assert_true(sink.active, "An inactive Document suppresses producer terminal steps");
}, "A detached producer Document cannot deliver, retain values, or accept termination");

test(() => {
  assert_equals(Object.getPrototypeOf(Window), EventTarget);
}, "Skipping Observable's constructor preserves Window's existing constructor inheritance");
