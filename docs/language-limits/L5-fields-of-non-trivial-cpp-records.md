# L5 - Fields of C++ records with non-trivial copy or destruction

## Limitation

`projectCppSemanticInstanceField` (`CppSemanticRecord.cpp`) projected array, record, enum, fixed-vector and
function-pointer fields of a C++ record only when the owner was a transport value (Item 15.1/T21). A field of such
a type in a record with a user destructor or copy was left unimported ("C/C++ field 'f' of type 'T' was not
imported: NativeSharp has no mapping for that field type"), and a record field was imported only when its own type
was a transport value.

## Rule

A field of a C++ record is reached in its place through the owner, whose complete layout Clang fixed. The owner's
copy and destruction do not affect that access. Whether the field's value may be copied or moved depends on the
field's own type, and the body analysis applies the rules of that type where the value is used (a non-trivial C++
record is not copied implicitly).

- Array, record, enum, fixed-vector and function-pointer fields are projected for every complete C++ owner.
- A record field is projected when its type is an imported record with a verified layout, transport value or not.
- `const` and `volatile` of the field are carried as before; the volatile restrictions for C++ fields are
  unchanged.

## Implementation

`projectCppSemanticInstanceField`: `cppPlaceFields` (every complete C++ owner) replaces the owner test, and the
record-field branch accepts any imported C++ record type with a matching layout.

## Preserved behavior

C records, scalar and pointer fields, bit-fields, unions and flexible arrays are unchanged.

## Validation

NSC built in Release with `/clang:-O3`. No tests were run.

## Pending

- Not verified that every use of a non-trivial record field (member access, address, passing by `in`) lowers
  correctly, nor that an implicit copy of such a field is diagnosed in every context.
- Rejections expected by earlier batch tests for these fields now accept the declarations.
