# NSC 3.0.5 — R03C2C2D3: `try`/`catch`/`finally` en el coordinador compartido

## SCOPE LOCK

Esta hoja migra exclusivamente `try`, sus cláusulas `catch`, filtros `when`, `finally` y la
fachada interna de `catch` standalone a `Parser::ParseContinuationCoordinator`.

Quedan fuera `using` (D4), `defer` (D5), switch/match/conditional-pattern (E1–E3), el cierre
especial F, las capas Pratt, HIR, MIR, LLVM, sintaxis, ABI, NSMETA y cualquier cambio de
diagnóstico o recovery. `tests/unit/ProjectMethodBodyModelTests.cpp` permanece protegido.

## Contrato preservado

- `TryStatement.first` conserva el bloque protegido y `TryStatement.second` conserva `finally`.
- Las cláusulas `CatchClause` permanecen en `children()` y en orden de fuente.
- `CatchClause.first` conserva el body; `second`, el filtro; `flag`, el catch-all.
- Tipo, nombre, rangos, cursor, múltiples catches y recuperación por bloque ausente no cambian.
- Un `try` sin `catch` ni `finally` conserva exactamente el diagnóstico histórico.
- La profundidad estructural se representa con frames, slots y una worklist heap-backed; no con
  recursión del stack nativo ni fachadas/drains anidados de esta familia.

## Diseño

`StatementKind::Try` desvía `KwTry` antes del fallback `Direct`. `TryFrame` conserva el nodo,
la cláusula pendiente, sus fronteras y el estado de `finally`; `CatchFrame` cubre la fachada
standalone. Las continuaciones separan bloque protegido, filtro, body de catch, descubrimiento
de la siguiente cláusula y finally. Los bloques y filtros se solicitan mediante los slots
estables del coordinador compartido.

La implementación antigua de try/catch se retiró de `StructuredControlParser`; las familias
D4–F siguen delegadas sin cambios. Todas las funciones nuevas o modificadas permanecen por
debajo de 60 líneas ejecutables.

## Prueba puntual e infraestructura

`TP-PAR-558` (`Host.Parser`, `CP-PAR-035`, `NIGHTLY`) contiene cuatro probes aislados dentro de
un solo caso focal:

1. 32,768 niveles de `try` con catch filtrado y finally;
2. orden de múltiples catches, filtro, catch-all y finally;
3. catch standalone seguido por otro statement;
4. recovery por cláusula o body ausente, preservando progreso y diagnóstico.

La cadena está registrada en el módulo, CMake de compiler-suite, `TestRegistry.h`, registro
default, design CSV, allocation CSV y manifest JSONL.

## Evidencia

- Configuración aislada: PASS con `clang-cl` y herramientas LLVM/NSLink resueltas por nombre
  desde el `PATH` nativo del proceso.
- Target exclusivo `nsc_component_tests`: PASS, 535/535 pasos, exit 0.
- Prueba exclusiva `nsc_component_tests.exe --case TP-PAR-558`: PASS, exit 0,
  527,866 microsegundos.
- El primer intento focal descubrió dos defectos del propio fixture, no del producto: los tipos
  nominales `First`/`Failure` no estaban registrados en `ParserFixture` y `protected` se usaba
  como identificador pese a ser keyword. Se corrigió el oracle sin relajar ninguna aserción;
  después pasaron los cuatro probes, incluida la profundidad 32,768.
- Registro estático, inventarios CSV/JSONL, símbolos, límite de funciones y `git diff --check`:
  PASS.

Logs persistentes:

- `A:\NSC-devel-paralelo\logs\agente-2\r03c2c2d3-configure.log`
- `A:\NSC-devel-paralelo\logs\agente-2\r03c2c2d3-build-nsc-component-tests.log`
- `A:\NSC-devel-paralelo\logs\agente-2\r03c2c2d3-build-final.log`
- `A:\NSC-devel-paralelo\logs\agente-2\r03c2c2d3-tp-par-558-final.log`

No se ejecutaron Host.Parser completo, CTest, TL-LANG, regresiones adicionales, baseline,
deploy ni instalación.
