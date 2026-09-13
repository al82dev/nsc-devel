# 286 — PDB y stepping de funciones inline / Inline PDB and stepping

## Resultado / Result

Los productos O0 con `forceinline` cumplen ahora la expansión obligatoria antes de emitir CodeView.
El PDB conserva un `S_INLINESITE` real bajo el caller, las líneas y locales del cuerpo expandido y
la ubicación de llamada. Los métodos `inline` no expandidos y `noinline` permanecen como
procedimientos ordinarios. / O0 products containing `forceinline` now honor mandatory expansion
before emitting CodeView. The PDB retains a real `S_INLINESITE` under the caller, the expanded
body's lines and locals, and the call location. Unexpanded `inline` and `noinline` methods remain
ordinary procedures.

## Archivos y funciones / Files and functions

- `src/LLVM/ProjectOopObjectGenerator.cpp`
  - `ProjectOopObjectGenerator::generate()` detecta funciones MIR con política `Required`.
  - Conserva caller y callee en un módulo cuando existe esa obligación; O0 sin `forceinline`
    mantiene la partición paralela existente.
- `tests/integration/inline_debug/InlineDebug.nsproj` y `Main.ns`
  - Producto ejecutable con `forceinline`, `inline` y `noinline` y resultado observable 42.
- `cmake/VerifyNativeSharpInlineDebugO0.cmake`
  - Compila únicamente el fixture con `-g -O0`, lo ejecuta e inspecciona símbolos, líneas,
    checksum SHA-256 y remapeo de ruta mediante `llvm-pdbutil`.
- `CMakeLists.txt`
  - Registra `nsc.debug.inline-stepping-o0` como evidencia enfocada.
- `docs/1.0.1`
  - Actualiza roadmap, estado, changelog, especificación, contrato del compilador, contrato de
    depuración, libro y mapas de conformidad.

## Implementación y decisión / Implementation and design decision

LLVM sólo puede expandir una llamada y construir su scope inline cuando caller y callee coexisten
en el mismo módulo. La partición anterior podía separar ambos en O0: `alwaysinline` quedaba sobre
una declaración opaca, la llamada sobrevivía y el PDB sólo mostraba dos procedimientos. Hasta que
la partición importe summaries y cuerpos entre unidades, un producto con cualquier obligación
`forceinline` usa un módulo. La restricción no se aplica globalmente, por lo que los proyectos sin
esa política conservan el throughput paralelo. / LLVM can only expand a call and construct its
inline scope while caller and callee coexist in one module. The prior O0 partitioning could split
them, leaving `alwaysinline` on an opaque declaration, a residual call, and two ordinary PDB
procedures. Until partitioning imports cross-unit summaries and bodies, products with any
`forceinline` obligation use one module. The restriction is not global, so projects without that
policy retain parallel throughput.

El test no acepta un nombre o flag superficial: exige ejecución correcta, un `S_INLINESITE` para
`RequiredStep`, ausencia de inline sites falsos para `PreferredStep` y `BoundaryStep`, locals de las
tres funciones y líneas remapeadas del cuerpo y los call sites. / The test does not accept a
superficial name or flag: it requires correct execution, an `S_INLINESITE` for `RequiredStep`, no
false inline sites for `PreferredStep` or `BoundaryStep`, locals from all three functions, and
remapped body and call-site lines.

## Evidencia enfocada / Focused evidence

```text
ctest -R ^nsc.debug.inline-stepping-o0$
1/1 passed
O0 executable exit 42 + CodeView/PDB symbol and line inspection
```

No se ejecutó baseline en este commit; corresponde al cierre de la tanda 3. / No baseline was run
for this commit; it belongs to the end-of-batch 3 closure.

## Mensaje de commit de referencia / Reference commit message

**Título / Title**

`Preserva stepping PDB de funciones inline / Preserve PDB stepping for inline functions`

**Descripción / Description**

`Mantiene monolíticos los productos con obligaciones forceinline hasta disponer de summaries entre unidades y certifica frames S_INLINESITE, líneas y límites noinline en O0. / Keep products with forceinline obligations monolithic until cross-unit summaries are available and certify S_INLINESITE frames, lines, and noinline boundaries at O0.`
