# Congelamiento 3.0, elemento 136 — un clasificador para las fuentes de vida conocida

Fecha: 2026-10-27. Revisión de partida: `cb0a04c8b`.

## Nota de trabajo

```text
HEAD: cb0a04c8b
Elemento: 136 del plan de 151 (unificar las comprobaciones existentes de fuentes de vida conocida)
Contrato vigente: DEC-10 y P10 (excepciones), NS304-T13-F03 (objetos scope), Stage 49 (owners raii),
  NS304-T05-F05 (local pin y suspensión), elemento 135 (doc 54); decisión de Alberto (2026-10-27), abajo
Documento histórico que podría contradecirlo: ninguno
Archivos exactos que se modificarán:
  include/nativesharp/Frontend/ProjectMethodBodyModel.h
  src/Frontend/Bodies/StatementAnalyzer.cpp
  src/Frontend/Bodies/AssignmentAnalyzer.cpp
  src/Frontend/Bodies/LambdaDelegateBinder.cpp
  src/Frontend/Bodies/MethodBodyCallableLifecycle.cpp
  tests/unit/Freeze30ContractTests.cpp
```

## Problema

Cada fuente de vida conocida tenía su propia forma de reconocerse. El mismo patrón (buscar el identificador,
comprobar que es un local o un parámetro, leer un indicador) se repetía en doce funciones:

| Fuente | Dónde se reconocía |
|---|---|
| Préstamo de un temporal owned (135) | `rejectOwnedTemporaryBorrowEscape` |
| Excepción atrapada (DEC-10) | `caughtExceptionScopeOfValue`, captura de lambda |
| Préstamo de parámetro de excepción (P10) | `loanedExceptionParameterOfValue`, captura de local y de parámetro |
| Objeto `scope` (NS304-T13-F03) | escritura de campo, `return`, captura de lambda |
| Owner raii o move-only | `return` sin `move`, copia a local, copia por asignación, captura de lambda |
| Parámetro owned | `return` sin `move`, copia por asignación |
| Local `pin` en función async (NS304-T05-F05) | revisión de locales al cerrar el callable |

## Decisión de Alberto (2026-10-27)

- Unificar las comprobaciones existentes sin introducir rechazos nuevos ni cambiar diagnósticos.
- Los préstamos derivados de locales raii/owned sólo se miden y se reportan; no se rechazan.

## Contrato

1. **Clasificador.** `KnownLifeSources` dice qué fuentes de vida conocida lleva algo:
   - `knownLifeSourcesOfResult`: un resultado lleva el préstamo de un temporal owned.
   - `knownLifeSourcesOfLocal`: un local lleva:
     - la excepción atrapada;
     - el préstamo de excepción;
     - objeto `scope`;
     - owner raii;
     - owner move-only;
     - `pin`.
   - `knownLifeSourcesOfParameter`: un parámetro lleva el préstamo de excepción o es un parámetro owned de
     referencia o string.
   - `knownLifeSourcesOfValue`: un identificador lleva lo de su local, o lo de su parámetro cuando se da el
     método.
2. **Comprobaciones.** Las doce comprobaciones de arriba reconocen su fuente sólo con el clasificador.
   `caughtExceptionScopeOfValue` y `loanedExceptionParameterOfValue` quedan como consultas del clasificador,
   y la herencia de alias (`T alias = e;`) sigue pasando por ellas. Cada comprobación conserva:
   - sus sumideros;
   - su orden;
   - su diagnóstico.
3. **Fuera del clasificador.** La comprobación del MIR que rechaza un préstamo de parámetro o campo a través de
   una suspensión queda en el MIR. Allí se decide qué locales pasan al frame de la corrutina, y el análisis de
   cuerpos no lo sabe.

## Medición: préstamos derivados de locales raii y scope

Sondas `A:\temp\nsc-dec\d41` (M01–M09), en `-O0`. `Box` escribe su `Id` en `Log` al destruirse; un 31 significa
que el string (longitud 3) se usó después de destruir su `Box` (`Log` 1):

| Sonda | Forma | Resultado |
|---|---|---|
| M01RaiiFieldReturn | `raii Box b = ...; return b.Name;` | compila; 31 |
| M02RaiiFieldOuterLocal | `s = b.Name;` en un bloque interior, `s` usado fuera | compila; 31 |
| M03RaiiFieldStore | `h.S = b.Name;` con `h` más longevo | compila; 31 |
| M04RaiiConditional | `s = c ? b.Name : "x";` | compila; 31 |
| M05RaiiCall | `s = Id(b.Name);` | compila; 31 |
| M06RaiiGetter | `s = b.Title;` | compila; 31 |
| M09ScopeFieldReturn | `scope Box b = ...; return b.Name;` | compila; 31 |
| M07RaiiAliasReturn | `Box a = b; return a;` | rechazada (copia del owner) |
| M08RaiiAliasOuter | `keep = b;` | rechazada (copia por asignación) |

Un alias directo del owner se rechaza. Un préstamo derivado (campo, getter, llamada, `?:`) de un local raii o
`scope` compila y puede sobrevivir al owner en las siete formas medidas. En estas sondas `Name` es un literal y
el valor leído sigue siendo correcto. Con un string propio del objeto, la lectura sería de memoria liberada. Es
un hallazgo, no un cambio de este elemento.

## SCOPE LOCK

- **Cambia.** Sólo cómo cada comprobación reconoce su fuente.
- **Idéntico.**
  - Todos los rechazos y diagnósticos.
  - Los sumideros y su orden.
  - El HIR, el MIR y el LLVM.
  - Los préstamos derivados de locales raii/`scope`, que siguen compilando (medidos arriba).

## Validación

nsc Release y `nsc_component_tests` Debug y Release de este árbol.

**Golden de las comprobaciones existentes.** Las sondas `A:\temp\nsc-dec\d41` G01–G19 cubren cada fuente con
sus sumideros. Se corrió `run.cmd` con el nsc de `cb0a04c8b` (`out\base0.log`) y con este árbol
(`out\f136.log`); `fc` no encuentra diferencias. Cubren:

- objeto scope: campo, campo con un valor leído, `return` y captura;
- excepción atrapada: captura, `return`, local exterior y alias en el handler;
- préstamo de excepción: `return`, static, captura del parámetro, captura del alias y alias local;
- owner raii: `return` sin `move` y captura;
- local `pin` en async;
- temporal owned: local y `yield`.

Sondas d40 (135): sin cambio.

Resto: `seq.cmd f136` contra `f135` es idéntico en todo. Incluye:

- `all.cmd`;
- `asynccases`, `gencases` y `loopcases`;
- los 66 programas de Freeze30 en `-O0` y `-O2`;
- los 27 proyectos de `tests/cases`;
- d28;
- las bibliotecas.

Prueba nueva: `Freeze30ContractTests::testKnownLifeSourcesKeepTheirChecks`.

- Rechazadas: F136ScopeFieldStore, F136ScopeReturn, F136OwnerReturnWithoutMove, F136OwnerCopiedToLocal y
  F136PinnedAcrossSuspension.
- Corre: F136ScopeValueReturn, 44.

Las fuentes de excepciones conservan sus pruebas DEC-10 y P10. Compila en `A:\temp\nsc-dec\bt`; se ejecuta en
el gate final.

## No verificado

- Batería completa y unitarias: gate final.
- La lectura de memoria liberada en los préstamos derivados (sólo se midió el orden de destrucción).
