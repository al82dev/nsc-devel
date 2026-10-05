# NS3-DBG-11 — Async, generadores y módulos / Async, generators, and modules

## Resultado / Result

ES: NSDBG presenta estados reales de Future, generador y generador async desde sus campos CodeView,
y reconoce frames de reanudación mediante marcadores reservados del PDB. El frame lógico siempre
conserva su índice físico AMD64; una operación suspendida que no está ejecutándose sigue siendo un
valor expandible y nunca un caller inventado. La salida DAP enlaza además EXE/DLL, carga tardía e
identidad GUID/age del PDB con el `ModuleId` monotónico de cada carga concreta.

EN: NSDBG presents real Future, generator, and async-generator states from their CodeView fields and
recognizes resume frames through reserved PDB markers. A logical frame always retains its backing
AMD64 physical index; a suspended operation that is not executing remains an expandable value and
never becomes a fabricated caller. DAP also binds EXE/DLL kind, late loading, and PDB GUID/age to the
monotonic `ModuleId` of each concrete load.

## Cambios productivos / Productive changes

- `VariableValue` clasifica `future<T>#id`, frames async, generadores y generadores async. Los campos
  `state`, `completedState`, `status` y `suspensionKind` sólo se consumen si CodeView los materializó;
  estados optimizados o ausentes se declaran no disponibles.
- Los símbolos terminados en `.async#N`, `.generator#N` o `.async-generator#N`, con `.resume`
  opcional, se presentan con nombre fuente, clase y estado; sufijos no numéricos se rechazan.
- `stackTrace` descarta símbolos, líneas o entradas inline cuyo `ModuleId` ya no coincide con la
  carga viva. Esto impide mezclar una DLL recargada con resultados de su PDB anterior.
- `ModuleSymbolStatus` conserva por separado la identidad RSDS esperada y la identidad efectivamente
  cargada. La segunda sólo se publica después de validar GUID y age contra DbgHelp.
- DAP añade `nsdbgTypeKind`, `nsdbgCoroutineKind`, `nsdbgSuspensionState`, `nsdbgLogicalFrame`,
  `nsdbgImageKind`, `nsdbgLoadId`, `nsdbgExpectedPdbIdentity` y `nsdbgLoadedPdbIdentity`.

- `VariableValue` classifies `future<T>#id`, async frames, generators, and async generators. State
  fields are consumed only when CodeView materialized them; absent or optimized state stays explicit.
- Reserved numeric PDB suffixes provide logical source names while retaining physical-frame identity.
- Stack resolution rejects symbols, lines, and inline entries belonging to a stale module load.
- Symbol status records expected RSDS and validated loaded PDB identities separately.
- DAP publishes additive coroutine, state-machine, image-kind, load-ID, and PDB-identity fields.

## Contrato de honestidad / Honesty contract

ES: PDB validado no significa que pueda reconstruirse una cadena async inexistente. NSDBG sólo
presenta el frame de reanudación que está ejecutándose y los objetos suspendidos que CodeView permite
leer. No recorre heap, scheduler ni punteros arbitrarios para fabricar una pila lógica.

EN: A validated PDB does not make a nonexistent async chain reconstructible. NSDBG presents only the
currently executing resume frame and suspended objects readable through CodeView. It does not crawl
the heap, scheduler, or arbitrary pointers to fabricate a logical stack.

## Prueba puntual / Focused test

- `build/nsdbg_ns3_coroutine_tests.exe`: estados created/suspended/completed, await/yield, nombres
  lógicos respaldados por PDB y rechazo de marcadores ambiguos.

No se ejecutan baseline, CTest general, instalación ni deploy. / No baseline, broad CTest,
installation, or deployment is run.
