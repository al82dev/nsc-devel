# Rutas de copia, indexadores y campos owned declarados (pendientes reales tras doc 14)

Rama `development-main-fixes`. Evidencia:

- `A:\nsc-main-fixes-evidencia\claude\rutas`: R03b, R03c-a, V03, R15, R16, V16 y R17;
- `m1-run` (sección `Declared`);
- `logs\p1-*` a `logs\p4-*`.

## Mejora 2: rutas que seguían omitidas

| Sonda | Ruta | Antes | Ahora |
| --- | --- | --- | --- |
| R03b | `List<T>` de la biblioteca: `AddRange(refs)`, destrucción, `list[0].Read()` | aceptada | E2 |
| R03c-a | igual, leyendo con `list.RefAt(0)` | aceptada | E2 |
| V03 | igual, leyendo antes de la destrucción | aceptada | aceptada |
| R15 | el callee copia `a[0]` de un arreglo rastreado a un campo | aceptada | E2 |
| R16 | captura `[move o]` en una lambda; el cierre muere y luego se lee el arreglo | — | E1 |
| V16 | igual, leyendo mientras el cierre vive | — | aceptada |
| R17 | indexador de fuente `this[int]` que devuelve un elemento guardado | aceptada | E2 |

## Causas y contrato

1. **Arreglos rastreados pasados a una llamada.** Un arreglo local rastreado pasado como argumento expone
   en `routeExposed` todo objeto que guarda: el callee puede copiar sus elementos (R03b, R15).
2. **Guardados no seguidos en el resumen.** Un guardado en un campo rastreado cuyo valor no es un parámetro
   ni `null` (un elemento o un campo de un parámetro, un resultado) vuelve el campo desconocido (R15).
3. **Lectura por indexador.** Una lectura de indexador de usuario (llamada sintética `get_Item`, sin enlace
   de llamada en el replay) es un uso de todo lo que el receptor puede guardar (R03b, R17).
   - Si el receptor es local rastreado, se usan sus pseudoarreglos de campo y su centinela.
   - En otro caso, se usa el receptor desconocido.
   - Nunca se reporta E1, porque el camino del getter no se modela.
4. **Campos rastreados.**
   - Los de tipo parámetro genérico (`T`, `T[]`) se rastrean.
   - Los arreglos `owned` en campo también: `List<T>` guarda en `owned T[] items` lo que recibe por
     `move(values[i])`, aunque el arreglo de origen solo tenía referencias prestadas.
   - Los campos `owned` simples siguen excluidos (adopción, documento 04).

## Mejora 1: campo owned inicializado en su declaración

`owned Child child = new Child();` daba «use of unassigned local variable», porque el tipo no se
consideraba apto para objeto local.

Ahora se acepta `isClassLocalNewOwnedClassField` (`new T(literales)`) en dos lugares:

- **El analizador de sentencias** lo admite al decidir si el tipo es apto para objeto local.
- **El modelo de construcción** exige una construcción cerrada (documento 14), del tipo del campo o de una
  clase derivada. Un `new` con argumento de llamada sigue sin demostrarse
  (`LocalOwnedFieldInitializerCallArgument`).

`m1-run`, sección `Declared` (O0 y O2), crea `Child`, `SubChild` y `Valued(5)` en la declaración y exige los
conteos exactos de constructores y destructores.

## Verificación

- `t06`, `class-value-origins`, `stage50_7` y `stage56 c7` pasan.
- `m1-run`: PASS O0 y O2.
- Bibliotecas: 32 de 32 (`libs-out-p4.log`).
- Sondas P01–P25 sin cambios.
