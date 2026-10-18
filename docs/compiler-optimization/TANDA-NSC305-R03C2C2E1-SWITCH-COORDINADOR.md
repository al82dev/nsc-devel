# NSC 3.0.5 — R03C2C2E1: `switch` en el coordinador compartido

## SCOPE LOCK

Esta hoja migra exclusivamente `switch` desde la ruta `Direct` al
`Parser::ParseContinuationCoordinator`. Quedan fuera `match`, patrones condicionales y especiales
restantes (E2, E3 y F), Pratt, HIR, MIR, LLVM, sintaxis, ABI y NSMETA.

No se elimina ni modifica el parser histórico, todavía requerido por familias no migradas. El
checkout principal y ambos trackings permanecen fuera de este commit de trabajador.

## Contrato preservado

- El header conserva los diagnósticos exactos para `(`, `)` y `{`.
- El governing ordinario sigue siendo una expresión completa.
- El governing declarativo conserva el wrapper `condition-declaration`, la declaración primero y
  el `SwitchStatement` después; una lista inválida conserva `InvalidCondition` y usa el primer
  binding disponible.
- `SwitchStatement::first()` conserva el governing y `children()` conserva secciones en orden.
- Cada `SwitchSection` conserva labels primero, statements después e `integerValue` igual al número
  exacto de labels agrupados.
- `default` sólo es label cuando le sigue `:`; valores `case` siguen siendo expresiones completas.
- El diagnóstico diferido de list-pattern se emite antes de analizar el valor.
- La recuperación de un token no label, los textos de colon y cierre, rangos y cursor no cambian.
- Sólo `conditional-pattern-inline` se aplana; no se aplica la reescritura de defer del bloque.
- No cambia parser público, AST, Sema, HIR, MIR, ABI ni NSMETA.

## Implementación

`StatementKind::Switch` evita el fallback `Direct`. `SwitchFrame`, almacenado en `std::deque`,
mantiene inicio, slot, declaración, nodo, sección, frontera y cantidad de labels.

Las continuaciones `startSwitch`, `finishSwitchGoverning`, `continueSwitch`,
`continueSwitchLabels`, `finishSwitchLabel`, `continueSwitchSection`,
`appendSwitchStatement` y `completeSwitch` procesan una unidad acotada por paso. Governing y valores
de `case` se solicitan como `WorkKind::Expression`; declaraciones usan el continuador existente;
statements de sección se solicitan como `WorkKind::Statement`. La ruta migrada no abre otra fachada
ni otro `drain`.

Complejidad: O(tokens + labels + statements), pila nativa O(1) en las aristas propias de switch y
memoria heap proporcional a la profundidad activa.

## Prueba puntual e infraestructura

`TP-PAR-561` (`Host.Parser`, `CP-PAR-035`, `NIGHTLY`) contiene cuatro probes aislados bajo un único
selector:

1. 32,768 `switch` anidados, labels default, rangos y cursor EOF;
2. governing expresión, labels agrupados, valores profundos, default, orden y flatten exclusivo;
3. governing declarativo simple y múltiple, wrapper, binding y diagnóstico exacto;
4. recovery de token no label, colon, cierre y diagnóstico diferido de list-pattern.

La cadena queda registrada una sola vez en módulo, CMake de compiler-suite, `TestRegistry.h`,
registro default, design CSV, allocation CSV y manifest JSONL, visible para el catálogo de Builder 3.

## Evidencia

- Reconfigure aislado de compiler-suite: PASS.
- Build exclusivo `cmake --build A:\NSC-devel-paralelo\builds\agente-2 --target
  nsc_component_tests --parallel 16`: PASS, 288/288 pasos, exit 0.
- Única prueba seleccionada, `nsc_component_tests.exe --case TP-PAR-561`: PASS, exit 0,
  321,129 microsegundos; pasaron los cuatro probes, incluido el contrafactual 32,768.
- La primera ejecución del mismo selector localizó un defecto sólo en el oráculo de dos mensajes:
  comparaba la descripción entregada a `Parser::expect`, no el mensaje contractual completo
  `expected ..., found ...`. Se corrigió exclusivamente el fixture y se repitió el mismo target y
  caso; no cambió el producto durante esa corrección.
- Source-registration, CanonicalCatalog 6,016 con TP-PAR-561, símbolos, límite de funciones,
  120 columnas y `git diff --check`: PASS.

Logs persistentes:

- `A:\NSC-devel-paralelo\logs\agente-2\r03c2c2e1-configure.log`
- `A:\NSC-devel-paralelo\logs\agente-2\r03c2c2e1-build-nsc-component-tests.log`
- `A:\NSC-devel-paralelo\logs\agente-2\r03c2c2e1-build-nsc-component-tests-final.log`
- `A:\NSC-devel-paralelo\logs\agente-2\r03c2c2e1-tp-par-561-final.log`

No se ejecutarán Host.Parser completo, CTest, TL-LANG, regresiones, minisuite, baseline, deploy ni
instalación.
