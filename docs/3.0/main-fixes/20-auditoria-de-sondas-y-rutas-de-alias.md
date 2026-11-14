# Probe audit, library evidence, and the alias and element routes of the array lifetime rule

Branch `development-main-fixes`. This closes items 4 and 5 of the review of docs 16 to 18.

Evidence lives in `A:\nsc-main-fixes-evidencia\correcciones-revision`:

- `rutas-corregidas\` holds the corrected and new probes. Each has an `ESPERADO.txt` and its `check-r*.log`;
- `rcheck.cmd` runs the analysis only and executes nothing;
- `libs2.cmd`, `libs-revision-O0.log` and `libs-revision-O2.log` are the library lane.

The original probes in `claude\rutas` and `claude\probes` were not modified.

## Library evidence

Earlier documents quote `logs\libs-out-p*.log` (32/32), most recently `libs-out-p21.log`. Those runs:

- used `check_new_nsc.py` with its default level, -O0;
- only build each library package (`--emit-project-oop-lib`); nothing is executed;
- link against the runtime library installed in `C:\native-sharp`.

They are a build check of the library lane at -O0. They are not an independent verification, and they say
nothing about -O1 to -O3.

In this revision the same script ran with the compiler that includes docs 19 and 20, at -O0 and at -O2
(`libs-revision-O0.log`, `libs-revision-O2.log`). Both levels build 32/32. It is still a build check only.

## Audit of the negative probes

Several route probes from docs 08 and 15 stopped at a syntax, type or ownership error before the lifetime rule
ran. For those probes, the route they name was not covered. The table gives, for each one, the cause, the
corrected probe and its control. "E1" is "reference to a destroyed object cannot be used". "E2" is "cannot prove
that the referenced object is still alive".

| Original probe | Where it stopped | Corrected probe | Result now | Valid control |
| --- | --- | --- | --- | --- |
| R02-param-array | ownership: an owner copied into an element | R02d (borrowed alias in a block, parameter array) | E2 | V02d accepted |
| R03-list | `NativeSharpCore` library references missing (not the `using` order) | R03e (`List.Add` through a callee) | closed by ownership (`Add` takes `owned T`) | V03e: same ownership error (not a control) |
| R03c-b | type: method group `list.Count` | R03c-b3 (`list.Count()`) | E2 | V03 (doc 15) accepted |
| R06-ref-local | ownership | R06b: borrowed value into an unqualified `ref` local | closed by the borrow check | V06b: same error (not a control) |
| | | R06c: the callee writes through a `ref` local | E2 | V06c accepted |
| R07-borrow-local | ownership: `R b = o;` | R07b: `borrowed R b = o;`, move while the borrow is alive | closed by the borrow check ("cannot move owner while a borrowed local is alive") | V07c accepted (borrow ends before the move) |
| R08-lambda | syntax: `System.Func` | R08c: owner captured by reference | closed by ownership | R16/V16 (move capture, doc 15): E1 / accepted |
| R14-lambda-capture | type and ownership: `var` with an unconverted lambda | R08b: borrowed alias captured by copy | closed by the borrow check | V08b: same error (not a control) |
| R09-local-array-of-array | ownership | R09c (borrowed alias in a block, nested array) | E2 | V09c accepted |
| P03-store-directo-en-main | ownership | covered by P02 and P14 | | |
| P08-valido-sobrescritura-viva, P09-valido-otra-ranura | labelled valid, but rejected by an ownership error | P08b and P09b are the controls that apply | accepted | |
| P12-ciclo-con-condicion-runtime | ownership: owner may stay moved across the loop | covered by P11 and P23 | E2 | P24 accepted |

New probes for routes that no earlier probe named:

| Probe | Route | Result now |
| --- | --- | --- |
| R02e / V02e | borrowed alias stored directly into a tracked local array | E1 / accepted |
| R26 / V26 | element copied from one tracked array into another | E2 / accepted |
| R24, R25, R27 | borrowed alias passed to a callee that stores it | closed by the borrow check ("borrowed argument requires a borrowed parameter") |
| R28, R29 | borrowed alias stored into a static or instance field | closed by the borrow check ("the field may outlive its source owner") |
| R30 | alias stored, owner destroyed, element never read | accepted (storing without using is not an error) |

## Defect found: references that reach an element without naming their owner (item 5)

Before this change, the compiler at commit `d90a75430` accepted R02d, R02e, R09c and R26 (`check-r3.log`).
Each of them uses a destroyed object.

The replay of the array rule (docs 04 to 08) only followed a reference value when it was the owner local itself
or a parameter going through a call summary. A direct element store `a[i] = value` did this:

- into a tracked local array, it erased the slot and marked it maybe absent. A later read had no candidate and no
  life event;
- into an untracked array (a parameter array, a nested element), it marked the flow unknown but exposed no
  object, so the unknown receiver had no candidate either.

The ownership rules forbid storing the owner itself. Two values could still carry it:

- a borrowed alias (`borrowed R b = o;`). The borrow check rejects storing it into a field, a `ref` local or an
  unqualified parameter, but not into an array element;
- an element copied from another tracked array.

The analysis treated those stores as safe only because it could not follow them.

### Fix

`src/Frontend/Bodies/ArrayReferenceFieldEffects.cpp` adds two functions:

- `arrayReferenceValueObjects` says which tracked objects a reference value may name, and whether that answer
  is exact:
  - literals, `null`, `new`, `this`, parameters and `move` name none;
  - an owner names its objects (exact);
  - a recorded non-owning local names what was recorded for it;
  - an element of a tracked array names that element's candidates (not exact: it may also hold an untracked
    object);
  - anything else may name every tracked object (not exact): a call result, a member, an element of an
    untracked array, a conditional, a local never recorded.
- `recordArrayReferenceValueLocal` records that answer for each non-owning reference local. It runs at the
  declaration (`StatementAnalyzer`) and widens on each later assignment (`AssignmentAnalyzer`).

Uses of the answer:

- **Direct element store into a tracked local array** (`AssignmentAnalyzer`):
  - an exact answer becomes a precise write of the slot, so E1 is possible;
  - an inexact answer becomes a possible write, and the array becomes unknown, so the result is at most E2;
  - an empty answer keeps the previous behavior.
- **Store into an untracked array:** the objects are added to the route-exposed and exposed sets that the
  unknown receiver reads. The result is at most E2.
- **Call summary that stores a non-owning local** (`MethodBodyBindingRecorder`): the summary writes the local's
  objects as a possible store.
- **Field-route call boundary** (`recordArrayReferenceFieldCall`): a recorded local passes its objects like a
  borrowed owner.

### The E1/E2 rule is unchanged

- E1 needs an exact slot whose single candidate was destroyed on every path. The new exact answers only come
  from owners and recorded aliases. A value the replay cannot follow only reaches E2.
- A move is still not a destruction. Moves into elements and owned parameters keep their existing handling
  (`releaseUnproven`, owned effects).
- Storing without using is not an error (R30, `AliasStoredNeverUsed`).
- A transfer proven to stay alive is still allowed. Probes P06 and P07 do not change.

### Conservative rejections this adds

A program that stores an unmodelled reference value into an element, destroys a tracked object, and then reads
that element is now rejected with E2. The value can be a call result, a member or a conditional, even when it
cannot name that object. `CallResultIntoElement` is the documented case. Its control `CallResultIntoElementLive`
(read before the destruction) is accepted.

## Regression

`tests/unit/ArrayReferenceAliasRouteTests.cpp` (`nsc.main-fixes.array-reference-alias-routes`) is a new file;
no existing test changed. It has 12 cases:

| Case | Expected |
| --- | --- |
| `AliasIntoLocalArray` | E1 |
| `AliasIntoParameterArray` | E2 |
| `AliasIntoNestedArray` | E2 |
| `AliasOfAliasIntoLocalArray` | E1 or E2 |
| `ElementCopy` | E2 or E1 |
| `CallResultIntoElement` | E2 |
| `AliasStoredNeverUsed` | accepted |
| five `...Live` controls | accepted |

## Verification

- **Probes:** `rutas-corregidas` (`check-r5.log`, `check-r6.log`) give the results above. `claude\rutas` and
  `claude\probes` give results identical to doc 18 (`RESUMEN-rev5.txt` equals `RESUMEN-p21.txt` and
  `RESUMEN-p17.txt`).
- **Tests:** `nsc.main-fixes.array-reference-alias-routes`, `nsc.ns3.t06.escape-summaries`,
  `nsc.class-value-origins`, `nsc.stage50_7.borrow-escape-analysis`, `nsc.stage56.c7-definite-assignment` and
  `nsc.main-fixes.wineh-catch-exit-cleanup` pass.
- **Libraries:** 32/32 at -O0 and at -O2. This is a build check only.

## Limits

- The borrow check still accepts storing a borrowed alias into an array element. Rejecting it there would be a
  language change, so it is not made here. The lifetime rule now covers the route instead.
- Fields of tracked objects written directly with a borrowed alias are closed by the borrow check (R28, R29),
  not by this rule.
