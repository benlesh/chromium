test(() => {
  let starts = 0;
  let completions = 0;
  new Observable(() => { ++starts; })
    .takeUntil(new Observable(() => {})
      .inspect({subscribe: () => { throw "KABOOM"; }}))
    .subscribe({
      next: assert_unreached,
      error: assert_unreached,
      complete: () => { ++completions; },
    });
  assert_equals(completions, 1);
  assert_equals(starts, 0, "A synchronous notifier failure prevents source subscription");
}, "takeUntil completes on an inspect subscribe error without starting the source");

