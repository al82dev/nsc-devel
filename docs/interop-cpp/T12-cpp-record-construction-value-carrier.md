# T12 - The value of a C++ record construction uses the type's value carrier

## Defect

Passing a freshly constructed C++ record directly to a call whose parameter has that type failed the LLVM verifier:

    Call parameter type does not match function signature!
      %struct.new.value82 = load [8 x i8], ptr %struct.new79
      ... call ... List<glm.vec2>.Add(..., i64 ...)

Seen in a port as `uvs.Add(new glm.vec2(UVW.x, UVW.y))`. The same type compiled when the construction went
through a local first (`glm.vec2 uv = new glm.vec2(u, v); list.Add(uv);`).

## Trace of the type

1. Source: `new glm.vec2(x, y)` is a `ValueConstruct` MIR node with `typeKey = type:<glm::vec2 identity>`,
   `targetTypeOrdinal` set to the imported record and `immediate = 8` (Clang's layout size).
2. Imported representation: `glm::vec2` is a `CppRecord` whose copy, assignment and destruction are trivial, so
   `ProjectNameLookup::cppTrivialValueEligible` holds and `ProjectOopMirModel` imports a MIR struct layout for it
   (`MirLayoutImporter.cpp`): size 8, alignment 4, `abiPassingForSize(8) = Direct`.
3. Generic specialization: `List<glm.vec2>.Add(T item)` substitutes `T` with the same type key; its parameter is
   that key.
4. Parameters: `createDeclaredFunctionType` builds every parameter with `abiParameterType`. The layout is
   `Direct`, so the parameter is `llvmType(typeKey)`, which `mapNonBuiltinLlvmType` maps from the struct layout's
   size: 8 bytes -> `i64`.
5. Other value uses: a local load (`LocalValue`), a parameter and a call result of the same type all read the value
   as `llvmType(typeKey)`, so they carry `i64`.
6. Lowering of the construction: `lowerValueConstruction` (`OopLlvmConstruction.cpp`) reserves the slot with the
   byte carrier `[immediate x i8]` for every imported record and then loaded the result with that same byte
   carrier. Only a C struct or union without a constructor (T19-002) was read with `llvmType(typeKey)`.

Divergence: step 6. The construction handed over `[8 x i8]` while every other value of the type, and the declared
parameter, is `i64`. Through a local it did not show, because the byte value was stored into the local and read
back as `i64`. For records whose size is not 1/2/4/8 the two carriers are the same `[N x i8]` and travel indirectly
through `wrapIndirectArgument`, so only register-sized records were affected.

## ABI

`i64` is not forced; it is the carrier the contract already gives the type. A record with a MIR struct layout of
size 1/2/4/8 is `Direct` (`abiPassingForSize`) and its carrier is `iN` (`mapNonBuiltinLlvmType`). That is the Win64
convention for an aggregate of that size whose copy and destruction are trivial: it travels by value in an integer
register, and Clang lowers it the same way for `x86_64-pc-windows-msvc`. Records that are not trivially copyable
have no MIR struct layout and are not affected.

## Change

`OopLlvmImplementation::lowerValueConstruction` (`src/LLVM/Lowering/OopLlvmConstruction.cpp`): the value read
from the construction slot uses `llvmType(node->typeKey)` for every imported record that has a MIR struct layout
(`findStructLayout`), not only for a C struct or union without a constructor. The slot keeps its byte carrier.

## Preserved behavior

- The construction slot, the constructor call (`this` is the slot), field initializers, the destination path
  (`cppDestination`) and the discarded temporary path (destructor call) are unchanged; they return the slot.
- Records without a MIR struct layout keep the byte carrier.
- Records of other sizes: `llvmType` already is `[N x i8]` for them, the same type as before.
- C structs and unions: T19-002 behavior is a subset of the new condition.

## Validation

NSC built in Release with `/clang:-O3` (`ninja -C build-clang nsc`). No tests, probes or tutorials were run; the
consumer has not been recompiled with this compiler.

## Pending

- Recompile the consumer with the installed compiler to confirm the verifier no longer rejects the call.
- Not reviewed: whether any other producer of an imported-record value (outside `lowerValueConstruction`) still
  hands over a byte carrier for a register-sized record.
