# Explicit Observable sharing prototype

This experimental fork implements the direction in
[WICG/observable#221](https://github.com/WICG/observable/issues/221), using
[spec PR #222](https://github.com/WICG/observable/pull/222) at
`fdd017d413c87888824e8de7e1b67d7fce150c85` as the behavior contract.
The Chromium base is `deef82faf08b374d0525fbc1ddabc821cc1346d3`.

## Implementation

- Ordinary Observable subscriptions create independent producer executions.
  SharedObservable owns one execution and its observation count, replay ring,
  and permanent terminal state. A reachable shared instance retains a kept-alive
  execution when its last observation leaves.
- Constructor dictionaries, `share()` setting inheritance and identity,
  `sharedSources`, replay snapshots, and producer-local `{close: true}` follow
  the draft. Metadata contains the nearest declared shared sources.
- Reset or permanent closure is committed before abort algorithms and producer
  teardowns run. Each subscription has a distinct observer and cancellation
  guard. Replay registers first and iterates a fixed history snapshot, allowing
  synchronous nested live delivery.
- Operators return ordinary Observables. `take` cancels upstream before its
  final delivery; `switchMap` publishes the replacement before old-inner cleanup
  and checks its identity before subscribing. `takeUntil` converts its notifier.
- Promise-consuming methods are removed. Promise, iterable, and async iterable
  inputs remain supported. The optional `scan` and `takeLast` additions are
  deferred, as in the spec draft.
- SharedObservable instances inherit from Observable, but the constructor
  inherits directly from Function.prototype, so it does not inherit `from`.
  The bindings change also preserves the existing WindowProperties skip.

## Tests and current evidence

The ten proposal test files in
`third_party/blink/web_tests/external/wpt/dom/observable/tentative/sharing/`
are a snapshot of the parallel WPT work. They contain 134 tests covering
construction, conversion, lifecycle, replay, reentrancy, metadata, operators,
error reporting, and EventTarget integration. `bindings.any.js` adds five tests
for native branding, constructor inheritance, subclasses, replay-ring wraparound,
and metadata conversion. `bindings.window.js` adds four browser-only tests for
cross-realm objects and cancellation, detached Documents, and Window inheritance.

The 139 `.any.js` tests pass against the JavaScript reference implementation
from the pinned spec PR, using WPT testharness in a Node adapter. This validates
the test snapshot against that model; it does **not** execute or validate the C++
implementation. The four `.window.js` tests have not been executed.

JavaScript syntax checks, Web IDL syntax parsing for the three changed IDLs,
Python syntax compilation for the generator change, and `git diff --check`
pass. Changed C++ hunks were formatted with clang-format.

**Native compilation, Blink bindings generation, WPT lint, Chromium presubmit,
and browser test execution are pending.** The authoring environment contains a
source snapshot, without a full Chromium checkout, depot_tools, build output,
or the complete WPT Python dependencies.

## Completing native validation

Apply the branch to a full Chromium checkout, install its dependencies, and
configure `out/Default`. Then run:

```sh
autoninja -C out/Default blink_tests
third_party/blink/tools/run_wpt_tests.py -t Default external/wpt/dom/observable/tentative/sharing/
third_party/blink/tools/run_wpt_tests.py -t Default external/wpt/dom/observable/
```

Review test expectations before interpreting those results. Existing tests in
the parent Observable directory still encode implicit sharing and the removed
Promise consumers; their migration remains part of the parallel WPT work. This
patch does not mask those conflicts with expected failures or skipped tests.
WPT development and native implementation can proceed in parallel, but the
updated tests must be reconciled and pass natively before upstream submission.

Before moving this fork draft to a Chromium Gerrit WIP CL, resolve compilation
and generated-binding issues, review Oilpan retention and cross-realm behavior,
complete the legacy test migration, and agree on runtime feature gating and
compatibility. This prototype changes behavior directly and has no rollout
flag; it is not ready to land. Chromium's upstream review path is Gerrit rather
than a GitHub pull request to the mirror.
