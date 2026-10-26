# Congelamiento 3.0 P4 — un `Span` sobre un arreglo conserva su mapa de vida

Fecha: 2026-10-26. Revisión de partida: `e26ddb4bd`.

## Decisión aplicada

P4, opción 1, en palabras de Alberto: todo `Span` derivado de un arreglo con mapa de vida conserva el mapa y el
desplazamiento correspondiente. Escribir revive; leer un slot muerto atrapa. Las subdivisiones y conversiones de
`Span` no pueden perder esa información.

## Problema

DEC-11 (doc 15) dejó como límite que un `Span` lee y escribe por un `T*` sin pasar por el bit de vida. Medido
con el compilador de `e26ddb4bd` (sondas en `A:\temp\nsc-dec\d16`, destructor que cuenta):

| Sonda | Forma | Antes |
|---|---|---|
| Q1 | `RefAt` de un slot muerto | 1001: lee 0 sin atrapar |
| Q3 | `Spans.OfRange` cuyo primer slot está muerto | 146 al **crear** el span (`&items[0]` lee el slot) |
| Q4 | `AsReadOnly().RefAt` de un slot muerto | 3001: lee 0 |
| Q5 | `foreach (ref …)` sobre un span con un slot muerto | 3101: lee 0 |
| Q6 | vista implícita `Span<Guard> s = arr;` y `ReadOnlySpan<Guard> r = s;` | 77001: lee 0 |

Además, con P3 todo tipo con mapa de vida es move-only (un `copy struct` no puede tener destructor ni campos
`owned`), y el setter del `Span` exige `copy`: no había forma de escribir un elemento así por un `Span`.

## Diseño

- `Span<T>` y `ReadOnlySpan<T>` guardan, además de `start` y `count`, el arreglo de origen (`source`) y el índice
  de su primer elemento (`offset`). Sus accesos (`this[]`, `RefAt`, `UncheckedAt`, `UncheckedSet`, y por ellos
  `Fill`, `CopyTo`, `CopyToOverlapping`) van por `source[offset + i]` cuando hay arreglo. Así el compilador
  aplica DEC-11 tal cual: una lectura de un slot muerto atrapa (`DeadElementRead`, 146), un store destruye el
  valor anterior solo si estaba vivo y revive el slot.
- `Put(int, owned T)` nuevo en `Span<T>`: mueve un valor a un elemento, para todo tipo valor. Es la escritura
  para un `T` move-only. Fuera de rango hace `panic`.
- `Spans.Of`, `OfRange`, `ReadOnlyOf` y `ReadOnlyOfRange` ya no toman `&items[i]` (eso lee el slot y atrapaba
  con el primero muerto); guardan el arreglo y el índice. `Data()` toma la dirección cuando se pide.
- `Slice` y `AsReadOnly` conservan arreglo e índice (`offset + begin`).
- Conversiones del compilador:
  - `OopLlvmExpressionMachine.cpp`: la vista implícita de un arreglo y la captura de resto (`SpanFromArray`)
    guardan arreglo e índice (`storeArraySpanSource`). Los spans que arma el compilador sin arreglo (arreglo
    fijo, literal `u8`) ponen en cero los campos nuevos; el paquete de parámetros ya lo hacía con `memset`.
  - `OopLlvmFieldsAndConversions.cpp`: `Span` → `ReadOnlySpan` copia todos los campos, no solo dos.
- `StatementParser.cpp`: `foreach` sobre un span lee cada elemento con `RefAt(i)` en lugar de `Data()[i]`, así
  que un slot muerto atrapa también ahí.
- Un span crudo (`OfRaw`, un puntero, `stackalloc`) no tiene arreglo y se comporta como antes.

## Validación

nsc Release recompilado. Sondas `d16`, `-O0` y `-O2`:

| Sonda | Ahora | Control solo con slots vivos |
|---|---|---|
| Q1 `RefAt` de un slot muerto | 146 | Q1b: 1001 |
| Q2 `Put` en un arreglo nuevo, `Put` otra vez, `delete[]` | 6102: valor 6, una destrucción al reemplazar, una al final | — |
| Q3 `Slice(1)` y lectura del slot muerto siguiente | 146 | Q3b: 401 |
| Q4 `AsReadOnly` | 146 | Q4b: 301 |
| Q5 `foreach (ref …)` | 146 | Q5b: 3502 |
| Q6 vista implícita y conversión a `ReadOnlySpan` | 146 | Q6b: 7701 |
| Q7 `Span<int>`: indexador, `Fill`, `Slice`, `AsReadOnly`, `foreach`, `ref mut`, vista implícita, `CopyTo` | 67868100, igual que antes |
| Q8 span crudo de `int` | 59900, igual que antes |
| Q9 `params ReadOnlySpan<int>` y `u8"abc"` | 630800, igual que antes |

Regresión: sondas H1–H8 y G4 de P3 (`d15`), A1–A11, L1–L6, cc1 `c27`/`c35`, `d11` y K1 con los mismos valores del doc
28; casos `nsc_component_tests` Debug y Release 96 PASS de referencias y 30 PASS de copia; corpus de `await` igual
que `d04`.

Pruebas nuevas: `Freeze30ContractTests::testSpanLivenessMap`.

## Límites

- `Data()` es un puntero crudo: lo que se haga por él no pasa por el mapa. `Algorithm` ordena y busca por
  `Data()`; sobre un span con slots muertos no atrapa. Sobre los spans que arma la biblioteca (`List.Sort`) todo
  el rango está vivo.
- `Put` está pensado para elementos valor. Con un `T` clase transfiere la propiedad al arreglo; para un arreglo
  `owned` de clases se usa el setter de la colección dueña.
- El layout de `Span`/`ReadOnlySpan` creció de 16 a 32 bytes. Como plantilla genérica, el consumidor lo
  materializa desde el fuente; una biblioteca precompilada con firmas no genéricas que pasen spans queda
  cubierta por el incremento de época de NSMETA de P8.
