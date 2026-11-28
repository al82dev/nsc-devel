# T14 - Diagnostic for `&`, `ref`, `in` or `out` applied to a value without address

## Defect

`analyzeAddressOf` (`IdentifierAnalyzer.cpp`) reported every non-addressable operand, other than an indexed read,
with one text: "bit fields have no address; operand of & must name addressable storage, not a bit field or
temporary value". For `in f(x)` or `ref new T()` the message named bit fields and `&`, neither of which the source
contains.

## Change

`nonAddressableOperandMessage` (file-local in `IdentifierAnalyzer.cpp`) chooses the text from the operand syntax
and names the form the source wrote (`&`, `ref`, `in`, `out`, read from the `pass:` marker of the argument):

- member access (`a.b`, `p->b`): it may be a bit field or a property value;
- identifier, `this`, `base` or a dereference: the previous general text;
- any other expression (call, construction, operator, literal, cast, conditional): a temporary value, which has no
  address; store it in a local first.

The indexed-read texts (string character, indexer by value) are unchanged.

## Preserved behavior

Only the text changes. Which operands are addressable, and therefore which programs are accepted, is unchanged.

## Validation

NSC built in Release with `/clang:-O3`. No tests were run.

## Pending

Not verified by compiling a program that reaches each text.
