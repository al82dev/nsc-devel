# Expected runtime failure: async-frame cleanup

This version compiles with the installed compiler. Its synchronous Iterator cleanup checks pass,
but cancelling AsyncIterator after its first yield leaves the recorded cleanup trace at 0,
where the program expects 7, and returns 5. This is an execution failure, not a compile error.

Observed in `_validation/runs/20261106-091519-chapter-20`. The example's normal project shows
an explicit caller-owned resource lifetime as the working counterpart. Do not change the expected
trace to zero to hide the missing cleanup. No compiler or installed-library change is part of this work.
