# NativeSharp.Core — Stage 41 minimum native core library

This directory is the canonical Stage 41 source declaration surface for the
minimal native core library. It is not a CLR assembly and it does not imply a
managed runtime, GC, reflection, or dynamic metadata system.

Stage 41 exposes:

```text
NativeSharp.Core.Console.WriteLine(string)
```

The compiler binds that exact ordinary static method call to the Stage 41 Core
operation and the native Windows runtime provides the terminal bridge.
