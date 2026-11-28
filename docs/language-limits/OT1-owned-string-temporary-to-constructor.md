# OT1 - Owned string temporaries as constructor arguments

## Problem

`new T(f())`, with `f` returning `owned string` and the constructor parameter an unqualified `string`, was
rejected: "unqualified parameter 'p' of type 'builtin:string' cannot receive an ownership transfer or a borrowed
value". The same argument to a method or a namespace function with an unqualified `string` parameter was accepted:
the fresh owned string is lent for the full expression and released after it (`fullExpressionBorrowArgumentOrdinals`).
Constructors only applied that rule to `borrowed string` parameters.

## Rule

A fresh owned string (an owned transfer that is also a fresh owner, the result of an owned-returning call) passed to
an unqualified or `borrowed` `string` parameter of a constructor is lent for the full expression and released after
it, exactly as for methods and namespace functions. A constructor that keeps the string must declare the parameter
`owned` (and receives the transfer), as before.

## Change

`src/Frontend/ConstructionModel/ConstructorCandidateEvaluation.cpp`, `isFreshOwnedStringBorrow`: unqualified
parameters are included.

## Status

Built (Release). See the tanda report.
