// META: title=Observable sharing: draft spec prototype and brand rules
// META: global=window,dedicatedworker
// META: spec=https://github.com/WICG/observable/pull/222

test(() => {
  assert_equals(Object.getPrototypeOf(SharedObservable), Function.prototype);
  assert_equals(Object.getPrototypeOf(SharedObservable.prototype), Observable.prototype);
  assert_false("from" in SharedObservable);
  const shared = new SharedObservable(() => {});
  assert_true(shared instanceof Observable);
  assert_true(shared instanceof SharedObservable);
  assert_equals(Observable.from(shared), shared);
}, "Shared instances inherit Observable methods without inheriting static from");

test(() => {
  const ordinary = new Observable(() => {});
  assert_false(ordinary instanceof SharedObservable);
  const count = Object.getOwnPropertyDescriptor(
    SharedObservable.prototype, "activeSubscribers").get;
  const replay = Object.getOwnPropertyDescriptor(
    SharedObservable.prototype, "replayValues").get;
  assert_throws_js(TypeError, () => count.call(ordinary));
  assert_throws_js(TypeError, () => replay.call(ordinary));
  assert_throws_js(TypeError, () => Observable.prototype.share.call({}));
}, "Sharing inspection requires the shared brand, and share requires an Observable");

test(() => {
  class Derived extends SharedObservable {}
  const source = new Derived(() => {});
  assert_equals(source.share(), source);
  assert_equals(Observable.from(source), source);
  const result = source.map(value => value);
  assert_equals(Object.getPrototypeOf(result), Observable.prototype);
  assert_false(result instanceof Derived);
  assert_false(result instanceof SharedObservable);
}, "Subclassed shared sources preserve identity while composition produces ordinary instances");

test(() => {
  let sink;
  const shared = new SharedObservable({maxReplayValues: 2, producer(s) { sink = s; }});
  shared.subscribe();
  for (let i = 0; i < 20; ++i) sink.next(i);
  const snapshot = shared.replayValues;
  assert_true(Object.isFrozen(snapshot));
  assert_array_equals(snapshot, [18, 19]);
  sink.next(20);
  assert_array_equals(snapshot, [18, 19]);
  assert_array_equals(shared.replayValues, [19, 20]);
  sink.complete();
  assert_array_equals(shared.replayValues, []);
  shared.subscribe();
  sink.next(21);
  assert_array_equals(shared.replayValues, [21]);
  sink.complete();
}, "Bounded replay preserves order through repeated wraparound and reset");

test(() => {
  const source = new SharedObservable(() => {});
  const declaration = [source, source];
  const result = new Observable({producer() {}, sharedSources: declaration});
  declaration.length = 0;
  const snapshot = result.sharedSources;
  assert_true(Object.isFrozen(snapshot));
  assert_array_equals(snapshot, [source]);
  assert_equals(result.sharedSources[0], source);
  assert_throws_js(TypeError, () => new Observable({
    producer() {}, sharedSources: [new Observable(() => {})],
  }));
}, "Source metadata copies and validates shared native identities");
