# Congelamiento 3.0, elemento 137 — contrato normativo de las fuentes de vida conocida

Fecha: 2026-10-27. Revisión de partida: `1df8e86e3`.

## Nota de trabajo

```text
HEAD: 1df8e86e3
Elemento: 137 del plan de 151 (contrato normativo con las garantías y los límites exactos)
Contrato vigente: elementos 131a, 131b, 135 y 136 (docs 51, 52, 54, 55); DEC-10 y P10; decisión de Alberto
  (2026-10-27), abajo
Documento histórico que podría contradecirlo: NSC-ESTADO-CONGELAMIENTO-3.0.md §6.6 (formas no reproducidas
  que conservaban la fuga), que 135 dejó obsoleto
Archivos exactos que se modificarán:
  docs/3.0/language/known-life-sources.md   (nuevo)
  docs/3.0/NSC-ESTADO-CONGELAMIENTO-3.0.md
  docs/3.0/decisiones-congelamiento/56-CONTRATO-DE-FUENTES-DE-VIDA-CONOCIDA.md
```

## Decisión de Alberto (2026-10-27)

Texto de la garantía:

> NativeSharp impide fugas, doble destrucción y escapes cuando la fuente y su vida son conocidas estáticamente.
> Las referencias no owned provenientes de parámetros o campos permanecen bajo responsabilidad del programador.

El contrato fija las garantías y los límites exactos.

## Contrato

`docs/3.0/language/known-life-sources.md`, en español e inglés, con tres partes:

- la garantía;
- las seis fuentes (temporal owned, local `raii`, objeto `scope`, excepción atrapada, préstamo de excepción,
  local `pin`) con la regla de cada una;
- seis límites.

Cada regla sale de un elemento con sondas:

| Regla | Elementos | Sondas |
|---|---|---|
| Temporal owned | 131a, 131b, 135 | d37, d38, d40 |
| Local `raii`, objeto `scope`, excepciones, local `pin` | 136 (sin cambio de conducta), DEC-10, P10 | d41 G01–G19 |

La regla del MIR sobre préstamos a través de una suspensión se cita del código y no se midió aquí.

## Medición de los límites

Sondas `A:\temp\nsc-dec\d42` y `d41` (M01–M09), en `-O0` y `-O2` con el mismo resultado:

| Límite | Sonda | Forma | Resultado |
|---|---|---|---|
| 1 | L4ParameterStored | `Hold(b)` guarda el parámetro `raii` en un static | compila; 55: lee `Keep.Id` ya destruido |
| 2 | d41 M01–M06, M09 | `b.Name`, `b.Title`, `Id(b.Name)`, `c ? b.Name : "x"` | compilan; usado tras destruir `Box` |
| 3 | L3CatchDerived | `m = p.Text;` hacia un local exterior | compila; 3 |
| 4 | L1OwnedStringArgument | `string s = Id(MakeText());` | compila; el temporal muere y `s` lo apunta (HIR) |
| 6 | L5ConditionalAwait | `(c ? Make(3).Name : "xy").Length + await Five()` | 83: se destruye una vez |
| — | L2OwnedObjectArgument | `Pick(Make(4))`, parámetro de clase no `owned` | rechazada: el 4 es sólo de strings |

El límite 5 (análisis conservador de llamadas) sale del diseño de `markCallTemporaryBorrow` (doc 54); no tiene
sonda.

## Hallazgo

El límite 4 es una referencia colgante reproducida, de una forma que 135 no cubrió. 135 sigue el préstamo
cuando un argumento ya es un préstamo (`Id(Make().Name)`). No lo sigue cuando el argumento es el propio temporal
string owned, que la llamada toma prestado hasta el final de la expresión
(`fresh-owned-temporary-to-borrowed-parameter`). No se corrige en este elemento: el plan es de 151 y el
elemento 137 es sólo el contrato.

## SCOPE LOCK

- **Cambia.**
  - El contrato nuevo.
  - `NSC-ESTADO-CONGELAMIENTO-3.0.md`:
    - cabecera;
    - contratos de temporales y fuentes de vida conocida;
    - D-08;
    - validación;
    - límites.
- **Idéntico.** El compilador y las pruebas.

## No verificado

- Batería completa y unitarias: gate final.
- La regla del MIR de préstamos a través de una suspensión (citada del código).
