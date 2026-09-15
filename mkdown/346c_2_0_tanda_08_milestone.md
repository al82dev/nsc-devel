# Milestone — NativeSharp 2.0 tanda 8 / Batch 8 milestone

## Alcance / Scope

La tanda 8 completa los ítems 334–346 del roadmap y entrega una frontera nativa verificable:
manifiestos SDK declarativos con mapeo, selección y diagnósticos; COM crudo y productivo; punteros
de función y thunks que contienen excepciones; matrices ABI de agregados, callbacks y `vectorcall`;
y un segundo target ARM64 guiado por el consumidor de objetos. / Batch 8 completes roadmap items
334–346 and delivers a verifiable native boundary: declarative SDK manifests with mapping,
selection, and diagnostics; raw and production COM; function pointers and exception-containing
thunks; aggregate, callback, and `vectorcall` ABI matrices; and a consumer-driven second ARM64
object target.

El cierre incorpora además la corrección de nombres temporales anidados de NSLink encontrada por el
baseline. La publicación transaccional conserva ahora rutas cortas y la prueba integral de
bibliotecas nativas vuelve a pasar bajo concurrencia. / The closure also includes the nested NSLink
temporary-name fix found by the baseline. Transactional publication now keeps paths short, and the
native-library integration test passes under concurrency again.

## Entregas / Deliveries

- 334–337: formato declarativo de SDK, mapeos, selección por target y diagnósticos accionables. /
  Declarative SDK format, mappings, target selection, and actionable diagnostics.
- 338–341: vtables COM, punteros de función y thunks de frontera seguros ante excepciones. / COM
  vtables, function pointers, and exception-safe boundary thunks.
- 342–345: ABI de agregados y callbacks, `vectorcall`, HVA y retornos vectoriales. / Aggregate and
  callback ABI, `vectorcall`, HVA, and vector returns.
- 346: emisión real de COFF Windows ARM64 como segundo target de objetos, sin prometer todavía un
  runtime ejecutable ARM64 completo. / Real Windows ARM64 COFF emission as a second object target,
  without yet claiming a complete ARM64 executable runtime.

## Baseline certificado / Certified baseline

```text
Run: 20260915-112900-262a5512
Compiler build: 1.0.1-dev+build.20260915T172901Z.8d50087111afdb0f
Compiler SHA-256: 722f405e5f17cf17795896848ea80992100c17f758d2bc1d7ff54cc4ef6a24b2
NSLink SHA-256: 3804675135132ef82280ba53435ca80ff44a282779eb224866d05a98d8eea39a
lld-link CTest: 381/381 passed
NSLink CTest: 381/381 passed
lld-link fixtures: 148/148 passed (O0/O2)
NSLink fixtures: 148/148 passed (O0/O2)
lld-link bug triage: 18/18 passed (O0/O2)
NSLink bug triage: 18/18 passed (O0/O2)
Gate 0: LANDED
Elapsed: 500.894 s
```

La evidencia canónica queda en `_abi/gates/G00/gate.json`; el historial detallado es regenerable y
no forma parte del snapshot. / Canonical evidence lives in `_abi/gates/G00/gate.json`; the detailed
run history is regenerable and is not part of the snapshot.

## Mensaje de commit de referencia / Reference commit message

**Título / Title**

`Cierra la tanda 8 / Close batch 8`

**Descripción / Description**

`Registra el Gate 0 verde del SDK, COM y ABI con 381 pruebas, 148 fixtures O0/O2 y 18 casos de
triage por cada linker, usando la publicación transaccional corregida de NSLink. / Record the green
Gate 0 for SDK, COM, and ABI with 381 tests, 148 O0/O2 fixtures, and 18 triage cases per linker,
using NSLink's corrected transactional publication.`
