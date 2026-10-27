# Congelamiento 3.0, elemento 113 — campo `Future<T>` de clase

Fecha: 2026-10-26. Revisión de partida: `c7908a7a0`.

## Nota de trabajo

```text
HEAD: c7908a7a0
Elemento: 113 del plan de 142 (defecto: campo Future<int> de clase rechazado)
Contrato vigente: C-14 (futures move-only; await consume su future; un future que no completa libera su frame)
Documento histórico que podría contradecirlo: ninguno
Archivos exactos que se modificarán:
  include/nativesharp/Frontend/ProjectObjectLayoutModel.h
  src/Frontend/ProjectObjectLayoutModel.cpp
  src/LLVM/Internal/OopLlvmImplementation.h
  src/LLVM/Lowering/OopLlvmExpressionLeaves.cpp
  src/LLVM/Lowering/OopLlvmCoroutines.cpp
  src/LLVM/Lowering/OopLlvmExpressionMachine.cpp
  tests/unit/Freeze30ContractTests.cpp
```

## Problema

Sonda `A:\temp\nsc-dec\d21\A4FutureField`:

```
class Holder { public Future<int> Pending; }
...
raii Holder h = new Holder();
h.Pending = Make();
return await h.Pending;      // se espera 7
```

Resultado: `error: field type has no resolved named-type layout`.

`ProjectObjectLayoutModel::layoutBaseType` buscaba el ordinal de un tipo con nombre. `Future<T>` no es un tipo con
nombre: su clave canónica es `future:<T>`, y su layout físico sólo existía como struct MIR
(`ProjectOopMirModel::appendFutureLayout`). Un local o un parámetro future ya usaban ese layout; un campo no.

## Causas encontradas al corregirlo

Con el layout del campo resuelto aparecieron dos defectos del mismo elemento:

1. **Doble liberación.** `await h.Pending` copiaba el future y no vaciaba el campo. El await libera el frame al
   completar, y al destruirse `h` el drop del campo lo liberaba otra vez (`0xC0000374`, A6 y A7). C-14 ya dice que
   `await` consume su future, pero `consumeAwaitedFuture` sólo vaciaba locales y parámetros.
2. **Llamada a la dirección cero.** Un campo nunca asignado, o ya consumido, guarda un future en cero. Esperarlo
   llamaba a su función poll nula (`0xC0000005`, A9).

## SCOPE LOCK

- **Cambia.**
  - Un campo de clase cuyo tipo es exactamente `Future<T>` (sin modificadores) tiene layout. El tamaño y la
    alineación salen de los mismos miembros, en el mismo orden, que el struct MIR de `appendFutureLayout`:
    estado, frame, resultado, función poll, error (sólo future que lanza) y función Drop.
  - `await` de un campo future lee y vacía el campo por una sola dirección. El receptor se evalúa una vez, en las
    tres rutas del await (síncrona, corrutina y entrada async).
  - Esperar un campo future vacío (sin función poll y no listo) atrapa como referencia nula, con el mismo código de
    salida que un receptor nulo (135).
- **Idéntico.**
  - Un campo de struct `Future<T>` se sigue rechazando con el mismo diagnóstico, en el mismo rango: la destrucción
    de un struct no libera un future (sonda S1: el future se fugaba).
  - Un campo con modificadores (`Future<T>[]`, `Future<T>[4]`) se sigue rechazando como antes.
  - El layout de locales, parámetros, tuplas y frames; el await de locales y parámetros.
- **Sin cambio de ABI.** El layout del campo es el del future que ya existía en MIR; antes el programa no compilaba.

## Validación

nsc Release y `nsc_component_tests` Debug y Release de este árbol. Sondas en `A:\temp\nsc-dec\d21`, `-O0` y `-O2`
con el mismo resultado:

| Sonda | Forma | Antes | Ahora |
|---|---|---|---|
| A4FutureField | campo asignado y esperado | rechazo | 7 |
| A5FieldNeverAwaited | campo nunca esperado; se destruye el dueño | rechazo | 1 (el parámetro owned se destruye) |
| A6FieldOverwrite | dos asignaciones y await | rechazo | 25 |
| A7FieldAwaitedThenDropped | await y destrucción del dueño | rechazo | 15 |
| A8FieldDefault | campo sin usar | rechazo | 9 |
| A9FieldUnassigned | await de un campo vacío | rechazo | 135 (referencia nula) |
| A10AsyncAwaitField | await de un campo dentro de un cuerpo async | rechazo | 16 |
| A12FieldVoid | `Future<void>` | rechazo | 4 |
| S1StructField | campo de struct | rechazo | el mismo rechazo |
| S2StructCopy | copia de un struct con campo future | NS3020 | NS3020 |

Antes de las dos correcciones del await, A6 y A7 terminaban con `0xC0000374` y A9 con `0xC0000005`.

Regresión focal (`all.cmd f113`, `focal.cmd f113b`): resultados de siempre; `awcheck` sólo cambia en `a4`, que es
este caso (antes rechazo, ahora 7). Casos de componente con etiqueta async, future, await o coroutine
(`A:\temp\nsc-dec\asynccases.cmd`, 43 casos en Debug y Release): 86 PASS.

Prueba nueva: `Freeze30ContractTests::testFutureFields` (707, 4, 212, 605, 135 y el rechazo del campo de struct).
Sus fuentes son las sondas `T1`–`T6` de `d21`, que dan esos valores en `-O0` y `-O2`. La suite no se ejecutó: se
ejecuta en el gate final. Compila en `A:\temp\nsc-dec\bt`.

## No verificado

- Batería completa y unitarias: gate final.
