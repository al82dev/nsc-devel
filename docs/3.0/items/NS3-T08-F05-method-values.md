# NS3-T08-F05 — Receiver capture and method values

## Status

Implemented on `development`. The directed validation passes 8/8 checks at `-O0` with NSLink. No baseline or broad test suite was executed for this item.

## Language contract

- A method group is a compile-time overload set. It has no runtime representation until a target callable type selects exactly one signature.
- A static method converted to a delegate is represented by its native function target and has no receiver.
- An instance method converted to a delegate produces a native bound-method cell. It is not a managed delegate and does not require a garbage collector.
- `receiver.Method` borrows the receiver. A borrowed receiver cannot escape its local or parameter lifetime, and a fresh temporary cannot be borrowed.
- `move(receiver.Method)` explicitly transfers an owned class receiver into the bound-method cell. The source local or owned parameter is consumed path-sensitively.
- Virtual and interface method values resolve the dynamic target when the binding is created. Invocation then uses the stored native code pointer and receiver.
- `[ref this]` and `[ref mut this]` are valid explicit lambda captures. `copy this` and `move this` are rejected because the implicit receiver is borrowed; `ref mut this` is also rejected in a readonly method.
- Struct receiver method values remain rejected until their lifetime and representation contract is implemented explicitly.

## Windows x64 representation

The native bound-method cell follows runtime ABI 1.1 and occupies 32 bytes with 8-byte alignment:

1. code pointer;
2. receiver/context pointer;
3. context-destruction entry;
4. producer deallocation entry.

An owning method value stores the receiver's dynamic destruction entry in the context-destruction slot. A borrowed method value leaves ownership callbacks empty.

## Implementation

- `include/nativesharp/Frontend/ProjectMethodBodyModel.h`: records dispatch kind, receiver ownership, and source provenance.
- `src/Parser/Parser.cpp`: accepts `this` in explicit lambda capture lists.
- `src/Frontend/ProjectMethodBodyModel.cpp`: selects overloads, validates receiver lifetime and mutability, and implements explicit receiver transfer.
- `src/Frontend/ProjectOopHirModel.cpp`: lowers borrowed and owning receiver bindings, including path-sensitive source consumption.
- `src/Frontend/ProjectOopMirModel.cpp`: resolves direct, virtual, and interface method-value targets.
- `src/LLVM/ProjectOopLlvmLowerer.cpp`: materializes native bound-method cells and resolves dynamic targets at binding time.
- `src/Language/CallableSemantics.cpp`: publishes the ABI 1.1 32-byte layout.
- `tests/unit/NativeSharp3T08MethodValueTests.cpp`: adds the directed feature gate.
- `tests/unit/NativeSharp3T08CallableTests.cpp` and `tests/unit/Stage42DelegatesEventsTests.cpp`: align earlier expectations with the completed contract.

## Directed evidence

Target: `nsc_ns3_t08_method_value_tests`

- native cell layout: 32 bytes, alignment 8;
- overload selection plus virtual and interface dispatch execute with exit code 42;
- `move(receiver.Method)` owns the receiver and destroys it exactly once;
- `[ref this]` executes with exit code 42;
- borrowed local receiver escape is rejected;
- `copy this`/`move this` are rejected;
- a temporary receiver requires explicit move ownership;
- readonly methods reject `[ref mut this]`.

Result: `PASS: 8 checks` using `-O0` and `A:\nslink-devel\build-debug\bin\nslink.exe`.
