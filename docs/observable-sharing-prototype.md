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
track the parallel [WPT PR #65060](https://github.com/web-platform-tests/wpt/pull/65060)
at `71be05d543b554bfb543a1701ff8dffd8f8a034e`, plus local regressions.
The WPT snapshot contains 158 tests covering
construction, conversion, lifecycle, replay, reentrancy, metadata, operators,
error reporting, and EventTarget integration. `bindings.any.js` adds five tests
for native branding, constructor inheritance, subclasses, replay-ring wraparound,
and metadata conversion. `bindings.window.js` adds four browser-only tests for
cross-realm objects and cancellation, detached Documents, and Window inheritance.
The local regressions add canceled `catch()` recovery and zero-based `flatMap`
and `switchMap` mapper indices. The existing takeUntil crash regression now
uses `subscribe()` and checks completion when the notifier's inspect callback
throws, without starting the source.

The 166 sharing `.any.js` tests and the rewritten crash regression (167 total)
pass against the JavaScript reference implementation
from the pinned spec PR, using WPT testharness in a Node adapter. This validates
the test snapshot against that model; it does **not** execute or validate the C++
implementation. The four `.window.js` tests, the rewritten native GC tests,
and the new C++ BFCache regression have not been executed.

JavaScript syntax checks, Web IDL syntax parsing, Python syntax checks for the
generator and attribute validator, and use-counter XML parsing pass. Changed
C++ hunks were formatted with clang-format. The whitespace check now compares
the candidate files to the actual upstream contents. The earlier claim that
`git diff --check` passed was based on a local snapshot that already contained
extra EOF blank lines; those blank lines have been removed.

**Native compilation, Blink bindings generation, WPT lint, Chromium presubmit,
and browser test execution are pending.** The authoring environment contains a
source snapshot, without a full Chromium checkout, depot_tools, build output,
or the complete WPT Python dependencies. The attempted native build reports
`autoninja: command not found`, fetching a checkout fails DNS resolution, and
the WPT lint entry point cannot import `tools.localpaths`.

## Review follow-up

Review items 1-18 are addressed in this branch: generated output declarations,
cancellation after catch callbacks, Document/ancestor/BFCache activity checks,
spec anchors, Window and worker exposure listings, exported draft IDL and
idlharness objects, crash/GC/index tests, constant-time cancellation checks,
an explicit internal IDL attribute for constructor inheritance, the parent
constructor invariant, FrozenArray return objects, whitespace and ordering,
observer naming, and use counters with matching histogram metadata.

The exported IDL intentionally leaves the constructor-prototype mismatch with
standard Web IDL visible in idlharness. No expected failure or skip was added.
The two draft questions about shared-execution realms and constructor inheritance
are [prepared separately](observable-sharing-spec-questions.md); neither has
been resolved by changing the normative contract in this implementation.

## Completing native validation

Apply the branch to a full Chromium checkout, install its dependencies, and
configure `out/Default`. Then run:

```sh
autoninja -C out/Default blink_tests blink_unittests
out/Default/blink_unittests --gtest_filter='ObservableTest.*'
third_party/blink/tools/run_wpt_tests.py -t Default external/wpt/dom/observable/tentative/sharing/
third_party/blink/tools/run_wpt_tests.py -t Default external/wpt/dom/observable/
third_party/blink/tools/run_web_tests.py -t Default wpt_internal/observable/subscriber-weak-ref.html
third_party/blink/tools/run_web_tests.py -t Default webexposed/global-interface-listing.html webexposed/global-interface-listing-dedicated-worker.html webexposed/global-interface-listing-shared-worker.html
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
compatibility. Review item 21 remains open: this prototype changes behavior
directly and has no rollout flag. A correct gate must preserve the shipped
Promise consumers, constructor conversion, implicit sharing, and EventTarget
listener behavior when disabled; hiding SharedObservable alone would not do so.
Review item 22 also remains open until a full native build and test run pass.
The branch is not ready to land. Chromium's upstream review path is Gerrit rather
than a GitHub pull request to the mirror.
