# NS2-PERF-P00 corpus

This directory defines the reproducible NativeSharp 2.0 compiler-performance
baseline. The runner exercises every optimization level (`O0` through `O3`),
all supported debug-information modes, object and executable production, and
both Windows linkers where linking applies.

From a Developer Command Prompt:

```bat
cd /d A:\nsc-devel
python tools\nsc_performance_baseline.py --iterations 5 --warmups 1
```

Results are written below `_nscbuilder\performance\p00\<run-id>` and include
the exact commands, raw compiler logs, artifacts, process peak working set,
phase timings, medians, p95 values, output sizes, and executable results.

The measured-job order is shuffled with a fixed seed so that temperature and
background-system effects are distributed across optimization levels. Warmups
are recorded but excluded from summaries. A run fails if compilation fails,
an artifact is missing, or an executable returns a value different from the
corpus contract.

`corpus.json` deliberately uses already certified regression projects. New
entries must remain deterministic, self-contained, and state their expected
process exit code when they produce an executable.
