# R03C2C2A - coordinador compartido expression/block/statement

## SCOPE LOCK

- **Objetivo autorizado:** establecer la infraestructura compartida de continuaciones para la
  frontera expression/block/statement.
- **Naturaleza:** refactor interno preparatorio; esta hoja no migra todavía ninguna familia de
  statements y no cierra la SCC lambda-block.
- **Revisión base:** `899487d3dec5`, cierre GATE-F02D.
- **Símbolos productivos modificables:** las fachadas `parseExpression`, `parseBlock` y
  `parseStatement`, el coordinador privado y sus frames/slots.
- **Infraestructura modificable:** registro de la nueva fuente productiva y su shim privado en
  `nsc_component_tests`.
- **Protegido:** gramática, AST público, cursor, recovery, diagnósticos, ABI, NSMETA, `loop`,
  `comprehend` y los niveles Pratt binary/unary/postfix-primary.
- **Prohibido:** afirmar O(1) de stack para la SCC, migrar familias R03C2C2B-F, baseline o suite
  completa.

## Causa y frontera

La lambda con cuerpo bloque entra desde `ExpressionIterationMachine` a `parseBlock`. El bloque
despacha statements y 24 rutas de statements pueden volver a expresión. La infraestructura
anterior sólo tenía continuaciones dentro de assignment/conditional y dentro del control
estructurado, sin una sesión común capaz de transportar resultados entre esas máquinas.

Esta hoja introduce una sola sesión por entrada pública. Las llamadas anidadas reutilizan el
mismo coordinador. El comportamiento todavía es deliberadamente sincrónico: cada request
despacha al `*Impl` canónico, por lo que la migración algorítmica y la eliminación de frames
nativos permanecen visibles en R03C2C2B-F.

## Diseño

`ParseContinuationCoordinator` posee:

1. una worklist LIFO de requests tipados `Expression`, `Block` y `Statement`;
2. slots de resultado en `deque`, estables aunque se agreguen requests anidados;
3. frames de bloque en `deque`, identificados por índice estable y con `beginIndex`,
   `beforeIndex`, `block` y `destination` reservados para la migración incremental;
4. activación RAII que restaura el coordinador anterior incluso si una operación abandona la
   sesión de forma excepcional.

Las fachadas conservan sus firmas. Una entrada externa crea la sesión; una entrada anidada
encola un request en la sesión activa. Los `*Impl` siguen siendo la única autoridad de sintaxis,
cursor, recovery y diagnósticos.

## Estado de validación

- Análisis estático de ownership: los slots y frames no exponen referencias invalidables;
  todos los enlaces se conservan mediante índices.
- Reconfiguración de producto y compiler-suite: PASS. Logs:
  `A:\temp\r03c2c2a-configure.log`, `A:\temp\r03c2c2a-configure-suite.log` y
  `A:\temp\r03c2c2a-configure-suite-name.log`.
- Build `nsc`: PASS. Log: `A:\temp\r03c2c2a-build-nsc.log`.
- Targets `nsc_component_tests`, `nsc_component_tests_destructive` y
  `nsc_component_tests_source_extended`: PASS. El target principal recompiló y enlazó 531
  pasos; los otros dos quedaron vigentes en el mismo comando. Log:
  `A:\temp\r03c2c2a-build-suite.log`.
- Host.Parser completo: PASS 554/554. Log: `A:\temp\r03c2c2a-host-parser.log`.
- Regresión dirigida de fachadas expression/block/statement: PASS 22/22, incluidos recovery,
  bloques, dispatch de statements, return/throw, assignment, lambdas y TP-PAR-551…554. Log:
  `A:\temp\r03c2c2a-facade-regressions.log`.
- `nslink.exe` se resolvió por nombre desde el PATH de proceso a
  `A:\nslink-devel\bin\release\nslink.exe`; el cache de suite conserva literalmente
  `NSC_NSLINK_EXECUTABLE:STRING=nslink.exe`.
- Registro productivo en `nsc_frontend` y shim privado en `nsc_component_tests`: PASS;
  `validate_source_registration.py` también PASS.
- Paridad CMake: una entrada productiva, una entrada de shim y un único include de la fuente.
- Umbral: todas las funciones nuevas miden entre 4 y 15 líneas físicas, por debajo de 60.
- `git diff --check`: PASS; sólo aparece el aviso de normalización LF/CRLF de `Parser.cpp`.
- No se creó TP-PAR-555 en esta hoja: el contrafactual profundo debe entrar con R03C2C2B,
  cuando exista la primera ruta realmente no recursiva, y ampliarse hasta el cierre R03C2C2F.
- La SCC profunda permanece explícitamente pendiente en R03C2C2B-F; esta evidencia sólo
  certifica la infraestructura y la paridad de las fachadas.
