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

## NativeSharp 3.0 API notes

- `StringBuilder.Length()` is intentionally a method, not a property. Use
  `builder.Length()`.
- `Assertions.Require(condition)` and `Assertions.Require(condition, message)` are always active in
  every optimization level and throw `AssertionException`. NativeSharp currently has no automatic
  caller-file/line intrinsic, so the message overload is the explicit context channel.
- `Lazy<T>` is synchronous and exactly-once after successful evaluation. It detects recursive
  evaluation, resets its guard when the factory throws, and does not claim thread-safe publication.
- Exceptions own copied message/stack text and an optional inner cause. Stack capture is supplied
  explicitly through `SetStackTrace`; Core does not hide a stack walk inside every throw.
