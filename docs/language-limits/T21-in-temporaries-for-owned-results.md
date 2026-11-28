# T21 - `in` temporaries for callees with an owned result

## Problem

L1 lends a temporary to an `in` parameter when the callee's result cannot refer to its arguments. The check
treated every lifetime-carrying result type (class reference, string, array, pointer) as possibly referring to
them. A function such as `owned List<V> corners(in M m)` called with `corners(a * b)` or `corners(list[i])` was
rejected with "argument 1 of type 'M' does not convert to 'M*'".

## Rule

An `owned` result is a new owner transferred to the caller, not a borrow of the arguments: the borrow model never
ties its lifetime to argument storage. A class, string or array result declared `owned` therefore does not keep
a temporary from being lent to an `in` parameter. A reference return and a pointer result (owned or not) still
refuse it.

## Change

`src/Frontend/Bodies/ConversionAndPassKindChecks.cpp`, `inTemporaryArgumentAllowed`: the lifetime-carrying result
check is skipped when `callee.declaredOwnership == Owned` and the result is not a pointer.

## Status

Built (Release O3). Checked on a small project: a NativeSharp function returning `owned List<glm.vec4>` called with
a C++ operator result and with a list element compiled, and the executable returned the expected value.
