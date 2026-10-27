# Congelamiento 3.0, elemento 131 — medición de D-08: frames de corrutina y temporales destruidos

Fecha: 2026-10-26. Revisión medida: `36c2a433d` (árbol idéntico a `64d826002`).

## Nota de trabajo

```text
HEAD: 36c2a433d
Elemento: 131 del plan de 146 (D-08 de la auditoría: «cambios de costo y de vida sin medición»)
Naturaleza: medición. Sin cambios de código ni de pruebas
Contrato vigente: C-02 (todo local de una corrutina vive en el frame; cada await tiene slot propio),
  D-01 (slots de repetición), C-11 (dueño oculto de temporales descartados o receptores)
Archivos que se modifican: sólo este documento
Evidencia: A:\temp\nsc-dec\d36 (sondas, scripts, volcados y tablas)
```

## Qué mide

D-08 dejó dos afirmaciones sin número:

- **C-02.** Los frames crecen para todo `async` y todo generador porque guardan todos los locales.
- **C-11.** Los destructores de temporales corren donde antes no corrían.

No hay compilador previo a la campaña con el que comparar: `build-chk2` queda fuera por instrucción. Por eso
cada número se compara con una **cota por vivacidad**, el frame que bastaría guardando sólo lo que sobrevive a
una suspensión. La cota no es un diseño propuesto; sólo dice cuánto del frame paga la regla de C-02.

## Método

### Frames

`MirFunctionLowerer.cpp` (`buildPollFrame`, `buildMoveNextFrame`) arma el frame en este orden:

1. un slot por `await` (el future hijo: 40 bytes, 48 con tipo de error);
2. slots de repetición de D-01;
3. todos los locales y los parámetros usados;
4. un byte de bandera de destrucción por local, alineado a 8.

El tamaño sale de la constante de `__nativesharp_alloc` en el poll o el `MoveNext`. Es la misma en `-O0` y en
`-O2`: M1 reserva 72 bytes en ambos.

`d36\frames.py` lee el LLVM sin optimizar y separa los slots por tipo. Después calcula qué slots están vivos al
reanudar: una lista de trabajo sobre el CFG, con una arista de cada suspensión a su caso del `switch` de
reanudación y a su caso de cancelación. En el tramo que se repite al reanudar, sólo sigue la rama que el
estado guardado hace posible: el `await` ya creado se reutiliza, no se crea otra vez.

- **Cota.** Slots de `await`, slots de repetición, locales vivos y sus banderas, todo alineado a 8.
- **Una lectura cuenta como uso.** También cuenta como uso cualquier otro empleo de la dirección del slot.
- **Un store al slot completo lo define.**

### Temporales

`d36\hirtemps.py` cuenta en el HIR los nodos `owned-temporary-borrow` que crea C-11
(`HirExceptionAndCleanup.cpp`, `ownDroppableTemporaries`): un productor (llamada, delegate, concatenación) cuyo
padre es una `full-expression-cleanup` (resultado descartado) o el receptor de un consumidor (campo, llamada de
instancia, `Length`, índice de string). Los préstamos de argumentos a parámetros `owned`, que existían antes,
quedan fuera.

## Resultados: frames

### Sondas (`d36\src\M*.ns`)

Todas corren con el valor esperado.

| Sonda | Corrutina | Forma | Frame | Cota | Diferencia |
|---|---|---|---|---|---|
| M1AsyncLocals | `Work` | 5 locales `int` antes del `await`; sólo 1 se lee después | 72 | 56 | 16 |
| M2AsyncNoLocals | `Work` | `return await Slow() + 7;` | 40 | 40 | 0 |
| M3AsyncStructDead | `Work` | struct de 32 bytes muerto antes del `await` | 96 | 56 | 40 |
| M4AsyncThreeAwaits | `Work` | tres `await` en sentencias separadas | 144 | 136 | 8 |
| M5Generator | `Count` | generador con 2 locales muertos antes del ciclo | 24 | 16 | 8 |
| M6AsyncSpill | `Work` | `Count() + await Slow()` (slot de D-01) | 64 | 48 | 16 |
| M7AsyncLoop | `Work` | `await` dentro de un `while` | 72 | 64 | 8 |
| M8AsyncRaii | `Work` | `raii Box` vivo a través del `await` | 64 | 64 | 0 |
| todas | `Main` | `int r = await Work(); return r;` | 56 | 40 | 16 |

En M4, 120 de los 144 bytes son los tres slots de `await` (40 cada uno). Con un slot compartido entre `await`
secuenciales serían 40. La regla de C-02 de un slot por `await` cuesta más que guardar los locales.

### Corpus de corrutinas (`A:\temp\nsc-dec\aw\src`, 55 fixtures)

40 fixtures declaran 68 corrutinas.

| Medida | Bytes |
|---|---|
| Frames, suma | 3,448 |
| Cota por vivacidad, suma | 2,872 (−17 %) |
| Slots de `await` | 1,784 (52 % de todo) |
| Locales y parámetros | 1,018, de los que 709 están vivos al reanudar |
| Slots de repetición (D-01) | 24 |

- De las 59 corrutinas que suspenden, 28 no tienen diferencia con la cota. La diferencia mediana es de 8 bytes
  y la máxima de 40.
- Ocho corrutinas tienen más de un slot de `await`: 688 bytes en total. Es el mayor costo de forma.
- **Nueve corrutinas nunca suspenden** y aun así reservan frame: 112 bytes en total y una asignación de heap por
  llamada. Con frames por vivacidad no reservarían nada.
- Los 15 generadores (sync y async) suman 336 bytes, con una cota de 304.

Los 27 proyectos de `tests/cases` no declaran corrutinas.

## Resultados: temporales (C-11)

### Sondas (`d36\src\T*.ns`)

Resultados idénticos en `-O0` y `-O2`. `Box` es una clase con destructor y `Make` devuelve `owned Box`;
`Guard` es un struct con destructor.

| Sonda | Forma | Destrucciones |
|---|---|---|
| T1Discard | `Make(1); Make(2);` | 2 |
| T2Field | `int v = Make(7).V;` | 1 |
| T3Method | `int v = Make(7).Get();` | 1 |
| T4StructDiscard | `MakeGuard(1); MakeGuard(2);` | 2 |
| T5StructField | `int id = MakeGuard(4).Id;` | 1 |
| T6StringReceiver | `(a + "cd").Length` | (string; resultado 4) |
| T9Loop | `Make(1).V` 1,000 veces | 1,000 |
| T7ReferenceResult | `string s = Make(3).Name;` | 0 |
| T8Chain | `Make(3).Next().V` | 1 de 2 |
| T10FieldWrite | `Make(9).V = 12;` | 0 |
| T11NewReceiver | `(new Box(5)).Get()` | 0 |

- **T1–T5 y T9 son la conducta nueva de C-11.** Hay un destructor y una liberación por temporal, en el punto
  donde termina la expresión completa.
- **T7, T8, T10 y T11 siguen sin destruir el temporal.** C-11 sólo posee un receptor cuando el consumidor
  produce un escalar o datos simples:
  - T7: el resultado es un string tomado del temporal;
  - T8: el consumidor intermedio `Next()` devuelve otro objeto;
  - T10: el temporal es el destino de una asignación;
  - T11: el productor es `new`, no una llamada.

### Alcance en el código existente

| Corpus | Descartes | Receptores |
|---|---|---|
| 27 proyectos de `tests/cases` | 0 | 1 |
| 55 fixtures de corrutinas | 0 | 0 |
| `NativeSharp.Core` | 0 | 0 |

El único sitio es `ReceiverHarness.ReturnedReceiver` en `nsc.stage31.receivers` (`Make(7).Add(8)`). Ahora
libera el `Counter` temporal, que no tiene destructor, así que la salida no cambia.

## Conclusión

- **C-02.** El costo de guardar todos los locales es acotado en este corpus: 17 % sobre la cota. Lo dominan los
  slots de `await` propios (52 % de los bytes) y las corrutinas que nunca suspenden, que pagan una asignación
  por llamada.
- **C-11.** En el código existente es casi invisible (1 sitio, sin destructor). En código nuevo agrega
  exactamente un destructor por temporal descartado o receptor de escalar.

Nada de esto es un defecto de corrección. Las cuatro formas que siguen sin destruir el temporal (T7, T8, T10,
T11) y la reserva de frame en corrutinas sin suspensión son **hallazgos no autorizados: NO IMPLEMENTAR** sin
decisión de Alberto.

## No verificado

- Tiempo de ejecución: no se midió. Las cifras son de bytes y de conteo de destrucciones.
- Comparación con el compilador previo a la campaña (sin `build-chk2`, por instrucción).
- La batería histórica completa: el corpus es el focal de arriba.
