# Stage 50 Ownership example

This example is the vertical Stage 50 ownership demonstration.

The positive path is:

```text
Create() -> owned return -> raii owner
                          -> borrowed view -> Inspect(view)
                          -> owner remains alive
                          -> Consume(move(owner))
                          -> owned parameter cleanup
                          -> exactly one destructor call
```

`Main.ns` uses `Resource.LiveCount()` and `Resource.DestroyCount()` so the executable can validate the ownership lifetime without requiring Console I/O. Exit code `0` means all checks passed; codes `10` through `15` identify the failed invariant.

On Windows CMD:

```cmd
build_and_run_windows.cmd C:\path\to\nsc.exe
```

The `negative` directory contains intentionally invalid projects. Each one freezes a Stage 50 diagnostic contract and is not part of `Stage50Ownership.nsproj`.
