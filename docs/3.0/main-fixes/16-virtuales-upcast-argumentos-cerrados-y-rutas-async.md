# Virtuales sobre `this`, upcast, argumentos escalares cerrados y rutas async/iteradores

Rama `development-main-fixes`. Esta etapa cierra pendientes reales que quedaron tras el doc 15.

Evidencia en `A:\nsc-main-fixes-evidencia\claude`:

- `m1-run`, con las comprobaciones 23 a 26;
- `rutas`: R18 a R22, V18, V20 y V21;
- `m1-06\g-static-arg`;
- `logs\p6-*` a `logs\p9-*`;
- `logs\libs-out-p9.log`.

## Mejora 1

### A. Llamada virtual sobre `this` dentro del llamado

Caso: `square.Twice()` cuando `Shape.Twice` llama a la virtual `Sides()` sobre `this`. Antes, el resumen
general de escape del receptor (`classReceiverMayEscape`, doc 10) trataba como escape toda llamada virtual
sobre `this`.

Ahora el resumen recibe el tipo exacto del objeto de marco. La llamada se resuelve a la ranura de ese tipo
mediante `classVirtualTargetForExactType`:

- **Con modelo de despacho:** se toma la ranura de la tabla del tipo exacto. Una ranura abstracta o no
  resuelta es escape.
- **Fase provisional, sin modelo:** se toma el miembro elegido. La etapa de construcciones lo vuelve a
  comprobar con el modelo de despacho.

El override alcanzado se analiza con el mismo tipo exacto, de forma recursiva. La memoria del resumen se
indexa por (callable, tipo exacto, fase).

### B. Upcast del local a un parámetro base (M1-04)

`Reader.Measure(square)`, con `Measure(Shape s)`, acepta la conversión `ReferenceUpcast` además de la
identidad.

`classParameterMayEscape` recibe también el tipo exacto. Dentro del llamado:

- las llamadas virtuales sobre el parámetro se resuelven por ese tipo;
- las llamadas sobre `this` dentro de ellas también;
- el reenvío a otro parámetro conserva el tipo.

Siguen rechazados:

- un override que publica `this`;
- un parámetro guardado en un estático.

### C. Argumentos escalares cerrados en el `new` del hijo owned (M1-06)

`classLocalClosedScalarArgument` acepta como argumento:

- un literal o una expresión acotada de literales (`2 + 3`);
- un literal con signo (`-4`);
- un parámetro escalar por valor;
- un campo estático escalar cuyo tipo tiene inicialización estática trivial.

Se usa en dos sitios:

- `classLocalChildConstructionIsClosed`, para la cadena y el inicializador del campo;
- `classLocalOwnedChildAssignmentField`, para el cuerpo del constructor.

El filtro sintáctico `isClassLocalNewOwnedClassField` deja pasar esas formas. La prueba semántica la hace
el modelo de construcciones.

Siguen sin demostrarse:

- una llamada como argumento;
- un estático cuyo tipo ejecuta un inicializador estático.

### Ejecución

`m1-run` pasa en O0 y O2, con estas comprobaciones nuevas:

- 23: `square.Twice() == 8`;
- 24: `Reader.Measure(square) == 12`;
- 25 y 26: `Seeded`, con `new Valued(Config.seed)` en la declaración y `new Valued(3 + 4)` en el
  constructor.

En el IR, los nueve locales de clase son `%localN.object = alloca`, sin `__nativesharp_alloc`.

## Mejora 2: auditoría de rutas async, corrutinas e iteradores

| Sonda | Ruta | Resultado |
| --- | --- | --- |
| R18 | async: guardar, `await`, destruir, leer | E1 |
| V18 | async: leer tras `await`, antes de destruir | aceptada |
| R19 | callee async que guarda en el arreglo; destruir; leer | E2 |
| R20 | iterador: guardar, `yield return`, destruir, leer | E1 |
| V20 | iterador: leer tras `yield return`, antes de destruir | aceptada |
| R21 | iterador con guardado diferido (corre en `MoveNext`, después de destruir) | E2 |
| V21 | el mismo iterador, leyendo mientras el objeto vive | aceptada |
| R22 | async con guardado diferido tras `yield;`, esperado después de destruir | E2 |

Conclusiones:

- **Puntos de suspensión.** `await`, `yield;` y `yield return` son flujo ordinario para la regla de vida.
- **Llamados corrutina.** Un llamado async o iterador puede guardar después de que la llamada vuelve
  (guardado diferido). Su guardado nunca se trata como demostrado: el uso posterior a una destrucción es E2
  aunque el arreglo haya sido sobrescrito con un objeto vivo (R21, R22).

Los ocho casos están en `nsc.ns3.t06.escape-summaries`:

- `async_*`;
- `iterator_*`.

## Pruebas

- `nsc.class-value-origins`: nuevos positivos y negativos.
  - Positivos: `LocalVirtualCallOnThisInCallee`, `LocalArgumentUpcast`,
    `LocalOwnedFieldInitializerStaticArgument` y `LocalOwnedChildStaticAndExpressionArguments`.
  - Negativos: `LocalVirtualCallOnThisOverrideEscapes`, `LocalArgumentUpcastOverrideEscapes`,
    `LocalArgumentUpcastStored` y `LocalOwnedFieldInitializerLazyStaticArgument`.
- También pasan:
  - `nsc.ns3.t06.escape-summaries`;
  - `nsc.stage50_7.borrow-escape-analysis`;
  - `nsc.stage56.c7-definite-assignment`.
- Las sondas P01 a P25 no cambian.
- Bibliotecas: 32/32.
