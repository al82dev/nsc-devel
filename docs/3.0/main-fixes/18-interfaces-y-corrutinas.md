# Local converted to an interface, and frame objects in coroutines

Branch `development-main-fixes`. This closes two real pending items of improvement 1.

Evidence lives in `A:\nsc-main-fixes-evidencia\claude`:

- `m1-run` (check 27);
- `m1-co\a-frame`, with its heap baseline `m1-co\b-heap`;
- `logs\p18-*` to `logs\p21-*`;
- `logs\libs-out-p18.log` and `logs\libs-out-p21.log`.

## A. Passing the local to an interface parameter

`Reader.Count(square)`, with the parameter `Count(ISided s)`, accepts the `ReferenceInterface` conversion.

Inside the callee, `classParameterMayEscape` resolves each interface call on the parameter with
`classInterfaceTargetForExactType`:

1. It walks from the exact type up the base chain until it finds the table plan for the interface.
2. It takes the member that implements the interface method.
3. A virtual implementation then resolves through the dispatch slot of the exact type (doc 16).

The local is still rejected in these cases:

- the implementation reached through the table publishes `this`;
- the callee stores the interface parameter;
- the implementation is missing or abstract (the analysis fails closed).

## B. Frame objects in async functions, iterators and async generators

Before this change, a `T value;` local in a coroutine was always rejected:

- in the front end, an async or generator caller made the proof return false;
- HIR reported "not yet supported in a coroutine".

The fix has three parts:

1. **MIR.** `appendCoroutineFrameLocalNodes` reserves room for the object in the coroutine frame. The
   size and alignment come from the `AllocateAndConstruct` node. The reservation is recorded in
   `ProjectOopMirFunction::asyncFrameObjects` as (local, offset, type). The MIR cache serializes this
   field, and the drop and generator constructor functions inherit it.
2. **LLVM.** In a frame-resident coroutine, `aliasCoroutineFrameLocals` points the object storage at
   `frame + offset`. The address is therefore stable across every `await`, `yield` and `yield return`.
   The local's slot holds that address. Cleanup works as follows:
   - on normal scope exit, the object-only destroy entry runs;
   - when the generator is abandoned, the cancel path does the same;
   - a function that would construct the object without owning its frame fails explicitly.
3. **Front end.** Async and generator callers are no longer rejected. A call that can suspend still
   cannot use the local, whether as the receiver or as an argument (`classLocalCallableMaySuspend`).

The runtime probe `m1-co\a-frame` passes at O0 and O2, matching the heap baseline. It covers:

- an async function with the local across two `await`s;
- an iterator abandoned with `break`, where the destructor runs once;
- an iterator run to completion.

The IR has no heap allocation for `Item`. `__nsc_classdesc_Item` is stored in `%coro.object`, inside the
frame.

## Tests

- `nsc.class-value-origins`:
  - positives: `LocalArgumentInterface`, `LocalInAsyncAcrossAwait`, `LocalInIteratorAcrossYield`;
  - negatives: `LocalArgumentInterfaceImplementationEscapes`, `LocalArgumentInterfaceStored`,
    `LocalPassedToAsyncCallee`.
- These also pass: t06, stage50_7, stage56 c7, m1-run, m1-07 (except k6, the earlier rethrow defect from
  doc 17), and m1-core.
- Routes and probes are unchanged.
- Libraries: 32/32.
