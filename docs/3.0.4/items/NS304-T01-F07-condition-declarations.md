# NS304-T01-F07 — condition declarations

## Contract

An `if`, `while`, or `switch` condition may declare exactly one initialized local with the
form `T name = expression`. The local is visible only in the controlled statement:

- `if`: the true and false branches;
- `while`: the current iteration, including its body;
- `switch`: every section of that switch.

`if` and `while` retain the existing `bool` condition rule. `switch` retains the existing
integral-or-enum governing rule. This item does not introduce truthiness or a new implicit
conversion.

The declaration owns one lexical lifetime. Normal completion and every structured exit
(`false`, `break`, `continue`, `return`, or unwind) use the existing block/cleanup machinery.

## Root cause

The parser already recognized `T name = expression`. A previous repair lowered only the `if`
form to a declaration followed by an `IfStatement`. `while` and `switch` still emitted
`NS3049` and then attempted to parse the declaration tokens as an expression. Moving the
`while` declaration before the loop would also have initialized it only once and extended its
lifetime across iterations.

## Implementation

- A shared internal binding helper rejects multiple declarators and synthesizes the exact
  declared-name expression.
- `if` and `switch` lower to a block containing the declaration followed by the control node.
- `while (T name = expression)` lowers to `while (true)` with an iteration block containing
  the declaration, `if (!name) break`, and the original body. This is the same consumed shape
  used by `while let`; it recreates the local per iteration and routes false/break/continue
  through the established cleanup paths.
- No new semantic, HIR, MIR, or LLVM state is introduced.

## Focused evidence

- Build: three demonstrated objects plus one relink of `nsc_component_tests`; LLVM/Clang 23;
  no CMake regeneration.
- `TL-LANG-300`: `PASS` (93,767 microseconds), including MIR scope/order and negative
  non-bool, multiple-declarator, and scope-leak fixtures.
- Guards: `TP-PAR-027`, `TP-PAR-334`, `TP-PAR-336`, `TP-PAR-340`, `TP-PAR-341`, and
  `TL-LANG-160`: `PASS`.
- No full battery was run.

Surface promotion was deliberately deferred to `NS304-T01-F08`, which subsequently closed
metadata, debug, and productive execution integration for the whole T01 family.
