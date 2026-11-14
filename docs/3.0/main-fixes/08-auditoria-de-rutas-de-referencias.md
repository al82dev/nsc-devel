# Auditoría de rutas de referencias (M2-07)

Rama `development-main-fixes`. Responde al matiz del usuario sobre la regla conservadora: comprobar que no
existan rutas de referencia que el análisis omita por completo. Evidencia:

- `A:\nsc-main-fixes-evidencia\claude\rutas` (sondas R01–R14, V01, V02 y `RESUMEN-m2-07.txt`);
- `logs\m2-07-*`;
- `logs\libs-out-m2-07d.log`.

## Método

Se escribió una sonda por cada forma en que una referencia de clase a un objeto con dueño local puede quedar
fuera de la vista de ese dueño y usarse después de un `move` o de una destrucción:

- campo static;
- arreglo recibido como parámetro;
- colección genérica;
- struct local;
- receptor anidado;
- elemento pasado por `ref`;
- local prestado;
- lambda;
- arreglo de arreglos;
- objeto de clase local;
- campo alcanzado dentro del callee;
- receptor `this`.

Cada sonda se analizó con `--check-project-constructions`; no se ejecutó nada. El resultado se clasificó en
tres grupos:

- el lenguaje ya la rechaza;
- la regla la sigue;
- era una ruta omitida, que esta etapa cierra.

## Resultado

| Sonda | Ruta | Antes | Ahora |
| --- | --- | --- | --- |
| R02, R06-ref-local, R07, R09-directo | copiar un dueño `raii` a una ranura, un local o un arreglo | rechazada por el lenguaje (`RAII owner ... cannot be copied`) | igual |
| R14 | capturar un dueño `raii` en una lambda | rechazada por el lenguaje (captura sin `move`) | igual |
| R10 | método sobre un objeto de clase local implícito | rechazada por la mejora 1 (`unproven use or receiver effect`) | igual (M1) |
| R01 | guardado en campo static por un helper | **omitida** | E2 |
| R02b | guardado en un arreglo recibido como parámetro | **omitida** | E2 |
| R04 | guardado en un struct local por `ref` | **omitida** | E2 |
| R05 | receptor anidado `outer.box.Put(x)` | **omitida** | E2 |
| R06 | guardado por `ref refs[0]` | **omitida** | E2 |
| R09 | arreglo de arreglos `Store(x, grid[0])` | **omitida** | E2 |
| R11 | el callee guarda en un campo de otro objeto (`inner.item = x`) | **omitida** | E2 |
| R12 | `this.Put(x)` dentro del callable reproducido | **omitida** | E2 |
| R13 | contenedor genérico `Bag<T>` | **omitida** | E2 |
| V01, V02 | lectura antes del evento / arreglo de parámetro sin relación | aceptadas | aceptadas |

Conclusión del lenguaje: una referencia prestada a un dueño local solo puede guardarse fuera de su vista
mediante una llamada que la recibe como parámetro no `owned`. Por eso la auditoría se concentra en la frontera
de la llamada.

## Contrato agregado

1. **Frontera de la llamada.** Un objeto rastreado pasado sin `move` queda en `routeExposed` si la llamada
   recibe, además, algo que puede guardar referencias:
   - un receptor de instancia;
   - un argumento de tipo clase, struct o arreglo que no sea un arreglo local rastreado;
   - un argumento `ref`, `out` o `in`;
   - `this`;
   - un argumento de tipo desconocido;
   - otro objeto rastreado.

   Un `move` transfiere ownership y no expone. `routeExposed` solo lo leen los candidatos de la regla de vida.
   Los pseudoarreglos de campo leen solo `routeExposed`; los arreglos locales leen `exposedObjects` y
   `routeExposed`. Los estados precisos y la publicación no cambian.
2. **Argumentos `ref`/`out`/`in`** que nombran un arreglo rastreado o uno de sus elementos vuelven el arreglo
   escapado y desconocido.
3. **Resúmenes.**
   - Se calculan también para métodos static.
   - Los campos static se rastrean siempre a través del receptor desconocido.
   - UnknownAll se produce cuando un parámetro de referencia:
     - se guarda en un destino no modelado (distinto de un campo, de una ranura de campo o de un arreglo
       parámetro);
     - inicializa un local;
     - se pasa a una llamada que no es sobre `this` o a un `new`.
   - `expr.F` sobre otro objeto es un uso del campo `F` del receptor desconocido.
4. **Receptores.**
   - `this` y el receptor implícito del callable reproducido forman un objeto receptor propio. Escapa
     cuando `this` se pasa como argumento o una llamada sobre él no tiene resumen.
   - Los demás receptores que no son locales rastreados (parámetros, cadenas de miembros, resultados) usan el
     receptor desconocido, que siempre está escapado.
   - Una llamada sin resumen sobre un receptor desconocido no es un uso. Solo expone lo que recibió.
5. **Lecturas.**
   - Los campos propios, de `this` y static leídos en el callable reproducido son usos del objeto receptor
     correspondiente.
   - Un elemento de un arreglo que no es local rastreado (parámetro, índice anidado, campo o resultado) se
     evalúa contra el pseudoarreglo centinela del receptor desconocido. Sus candidatos son `routeExposed`.
   - Un acceso directo `local.F` desde el llamador ya no expone objetos. El lenguaje impide guardar así una
     referencia prestada: copiar un dueño `raii` se rechaza, y `local.F = move(x)` es una transferencia.
6. **Alias.** `alias = otro` con `otro` arreglo local rastreado reasocia el alias a ese arreglo, sin marcarlo
   como escapado.
7. **Candidatos del replay.** Se reproduce todo callable analizado que contiene un `move` o un dueño de
   ámbito anidado, porque solo así puede haber un evento de vida.

## Pruebas

- `nsc.ns3.t06.escape-summaries`: 456 comprobaciones, pasa.
  - Rechazos E2:
    - `route_static_field`, `route_parameter_array`, `route_ref_element`;
    - `route_struct_local`, `route_nested_receiver`, `route_deep_callee`.
  - Rechazos E1: `route_this_receiver` y `route_generic_container`, con destrucción cierta.
  - Aceptados: `route_static_field_live` y `route_parameter_array_unrelated`.
- `nsc.stage50_7.borrow-escape-analysis`, `nsc.class-value-origins` y `nsc.stage56.c7-definite-assignment`
  pasan.
- Sondas P01–P25 sin cambios respecto de M2-06.
- Bibliotecas: 32 de 32 compilan (`libs-out-m2-07d.log`).
  - Durante el desarrollo, dos versiones intermedias demasiado amplias rompieron `NativeSharp.Core` y
    `NativeSharp.ConVision`, con E2 en lecturas de campos propios tras un `move` a un campo `owned`.
  - Las reglas 1 y 5 se acotaron hasta que las 32 bibliotecas volvieron a compilar.

## Límites (no verificados o no cubiertos)

- **`List<T>` de la biblioteca (R03).** La sonda no compila sin la referencia a la biblioteca, así que no se
  midió. El contenedor genérico de fuente (R13) sí se cubre. Las llamadas a código precompilado son
  UnknownAll y exponen lo recibido, pero un uso posterior solo se detecta si el receptor es un local rastreado
  o la lectura llega a un pseudoarreglo.
- **Lambdas con captura por `move`.** No se auditaron. La captura de un dueño `raii` sin `move` la rechaza el
  lenguaje.
- **Corrutinas, `async` e iteradores.** No se auditaron.
- **Fase `--check-project-method-bodies`.** La regla no corre en esa fase sola (M2-08).
- **Conservadurismo.** El receptor desconocido reúne todo valor expuesto. Un programa seguro que guarda una
  referencia en un contenedor no local, mueve el dueño y luego lee otro contenedor no local recibe E2.
