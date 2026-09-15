# Milestone — NativeSharp 2.0 tanda 7 / Batch 7 milestone

## Alcance / Scope

La tanda 7 completa los ítems 324–333 del roadmap y entrega layout compatible con C y MSVC:
structs y unions anónimos, promoción de sus miembros, sintaxis, layout, acceso y volatilidad de
bitfields, además de declaraciones C variádicas, promociones por defecto y ABI Win64 de varargs. /
Batch 7 completes roadmap items 324–333 and delivers C- and MSVC-compatible layout: anonymous
structs and unions, member promotion, bit-field syntax, layout, access, and volatility, plus C
variadic declarations, default promotions, and the Win64 vararg ABI.

El cierre incorpora también la reparación de direccionabilidad de elementos de arreglos detectada
por el baseline y valida la tanda con el NSLink actualizado e instalado. / The closure also includes
the array-element addressability repair found by the baseline and validates the batch with the
updated installed NSLink.

## Entregas / Deliveries

- 324–326: agregados anónimos y resolución de miembros promovidos. / Anonymous aggregates and
  promoted-member resolution.
- 327–330: bitfields con layout MSVC, acceso seguro y semántica volatile. / Bit fields with MSVC
  layout, safe access, and volatile semantics.
- 331–333: declaraciones variádicas, promociones C y ABI Win64 certificado. / Variadic
  declarations, C promotions, and certified Win64 ABI.

## Baseline certificado / Certified baseline

```text
Run: 20260915-044738-041e83e6
Compiler build: 1.0.1-dev+build.20260915T104821Z.69841ff835d6327a
Compiler SHA-256: 0f1fd4fa2035c5ca361f4a92839212f1fa6bcd6969f0e1c515400ba2ba6da8cb
NSLink SHA-256: 03a5e07d577042d5db9f52d326fd42f0dadf51d000749b72cc0d370b2c5f6caa
lld-link CTest: 368/368 passed
NSLink CTest: 368/368 passed
lld-link fixtures: 148/148 passed (O0/O2)
NSLink fixtures: 148/148 passed (O0/O2)
lld-link bug triage: 18/18 passed (O0/O2)
NSLink bug triage: 18/18 passed (O0/O2)
Gate 0: LANDED
Elapsed: 875.159 s
```

La evidencia canónica queda en `_abi/gates/G00/gate.json`; el historial detallado es regenerable y
no forma parte del snapshot. / Canonical evidence lives in `_abi/gates/G00/gate.json`; the detailed
run history is regenerable and is not part of the snapshot.

## Mensaje de commit de referencia / Reference commit message

**Título / Title**

`Cierra la tanda 7 / Close batch 7`

**Descripción / Description**

`Registra el Gate 0 verde para layout C/MSVC y varargs Win64 con 368 pruebas, 148 fixtures O0/O2 y
18 casos de triage por cada linker, usando el NSLink actualizado. / Record the green Gate 0 for
C/MSVC layout and Win64 varargs with 368 tests, 148 O0/O2 fixtures, and 18 triage cases per linker,
using the updated NSLink.`
