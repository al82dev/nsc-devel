# Congelamiento 3.0 P3 — un retorno `ref` de struct como receptor llega al almacenamiento

Fecha: 2026-10-26. Revisión de partida: `b0a03bd98`.

## Problema

Con `public ref S GetMut()`, `h.GetMut().A = 5` y `h.GetMut().Bump()` no cambiaban nada: R16a y R16c daban 0 y
R4 daba 0. El HIR generaba `field-address(temporary-address(pointer-load(instance-call reference-result=true)))`:
cargaba el struct a un temporal y escribía en el temporal. Con un local `ref` sí funcionaba (R16b).

`isNaturallyAddressable` ya trataba como almacenamiento un accesor (propiedad o indexador) que devuelve `ref`.
No hacía lo mismo con una llamada a método. P3 lo necesita para `lista[i].Campo = v` y `lista[i].Metodo()` con un
indexador `ref`.

## SCOPE LOCK

- **Cambia.** `HirExpressionMachine.cpp`, `isNaturallyAddressable`: un `CallExpression` resuelto a un método
  `isReferenceReturn` es almacenamiento. El campo o el receptor se toman de la dirección devuelta.
- **Idéntico.**
  - Las llamadas por valor siguen materializando un temporal.
  - Los accesores no cambian.
  - Los demás nodos no cambian.

## Validación

| Sonda | Antes | Ahora |
|---|---|---|
| R16a `h.GetMut().Bump()` | 0 | 1 |
| R16c `h.GetMut().A = 5` | 0 | 5 |
| R4StructReceiver (dos `Bump` y lectura por `ref readonly`) | 0 | 22 |
| R16b (local `ref`) | 1 | 1 |
| R16d (`int`) | 10 | 10 |

Corpus de `await` y casos TL-LANG: ver doc 23.
