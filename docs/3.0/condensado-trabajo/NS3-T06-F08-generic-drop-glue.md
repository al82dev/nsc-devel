# NS3-T06-F08 — Drop glue genérico e indirecto / Generic and indirect drop glue

## Resultado

NativeSharp 3.0 emite un thunk físico por cada tipo valor que necesita destrucción. El thunk nace
del layout MIR del productor, se publica mediante NSMETA 1.36 y se reutiliza desde arrays fijos,
especializaciones genéricas, variants y consumidores precompilados. La bandera de vida existente
se limpia antes de invocarlo, por lo que cada valor inicializado se destruye una sola vez.

## Cambios principales

- MIR 1.22 conserva si el layout es importado, si requiere drop y el nombre físico del thunk.
- LLVM declara el contrato uniforme `void(ptr)` en todos los code units y define el cuerpo sólo en
  el módulo propietario.
- Un consumidor de `LibraryReference` llama el thunk del productor; no reconstruye destructores
  privados ni supone el layout interno.
- Arrays fijos recorren elementos en orden inverso y delegan cada valor nominal a su thunk.
- Variants consultan el tag y destruyen exclusivamente el payload activo. El carrier union interno
  nunca recibe drop independiente.
- Las fábricas generadas transfieren el payload y el resultado mediante `move`, incluida una
  especialización genérica move-only.
- Los thunks de tipos públicos forman parte de la tabla de exports de una DLL.
- NSMETA 1.36 serializa, valida, mezcla, reporta y fingerprinta la identidad física; metadata 1.35
  continúa siendo legible.

## Archivos nuevos

- `tests/unit/NativeSharp3T06DropGlueTests.cpp`: única prueba dirigida de F08.
- `docs/3.0/language/generic-drop-glue.md`: contrato normativo bilingüe.
- Este documento: mapa del commit y evidencia de validación.

## Archivos modificados

- Parser de variants y modelo semántico de unions discriminados.
- Layout MIR, dumper, identidad de caché y schema MIR 1.22.
- Lowerer LLVM y publicación de exports DLL.
- Codec, merger, reporte y fingerprints NSMETA 1.36.
- Expectativas de compatibilidad, CMake, documento de tanda y tracking maestro.

## Mejora observable

Un valor move-only puede vivir dentro de un variant genérico, dentro de un array, cruzar una
`LibraryReference` y seguir destruyéndose con el código del módulo que conoce su representación.
El consumidor no duplica ownership ni ejecuta todos los brazos de un union.

## Validación dirigida

- `nsc` compiló correctamente.
- Objetivo exacto `nsc_ns3_t06_drop_glue_tests` compiló correctamente.
- Un único ejecutable dirigido cubre variant genérico dentro de array fijo, conteo exacto de
  destrucciones, NSMETA 1.36, metadata corrupta, lectura 1.35 y productor/consumidor separado.
- Baseline, suites, matrices y regresiones agrupadas: no ejecutados.
