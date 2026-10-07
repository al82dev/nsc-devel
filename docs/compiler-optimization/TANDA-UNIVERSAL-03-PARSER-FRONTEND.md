# Tanda universal 03: parser y frontend sin edición

## SCOPE LOCK

- Objetivo autorizado: eliminar del parser y de sus fachadas de frontend toda selección de sintaxis basada en
  `LanguageEdition`.
- Naturaleza: corrección funcional de compatibilidad acumulativa y eliminación de plumbing obsoleto.
- Invariantes: la gramática, el AST, los diagnósticos de features realmente no implementados y las etapas
  posteriores deben conservar su comportamiento.
- Revisión de entrada: `9e8fb195b`.
- Archivos permitidos: parser, fachadas directas de parseo, consumidores de sus constructores y este documento.
- Archivos protegidos en esta tanda: ownership, method bodies, MIR, NSMETA, runtime, bibliotecas y herramientas.
- Pruebas históricas: conservan sus nombres; sólo se elimina de ellas la construcción explícita de un frontend
  por edición.
- Prohibido: escribir o instalar en `C:`, certificar un baseline, ejecutar O1/O2/O3, renombrar pruebas históricas
  o implementar features aplazados.
- Terminación de la tanda: commit local seguido de reconstrucción limpia y batería O0 completa con NSLink.

## Causa

`Parser::requireEdition3` ya devolvía siempre `true`, pero 68 referencias en `Parser.cpp` mantenían una frontera
ficticia. `Parser`, `FrontendCompilation` y `ProjectSyntaxUnit` seguían recibiendo y almacenando una edición que
no podía cambiar la sintaxis aceptada. Ese estado permitía que consumidores futuros volvieran a introducir un
rechazo por manifiesto y obligaba a propagar un dato sin significado por cada unidad de sintaxis.

## Cambios

- `Parser` deja de incluir, recibir y almacenar `LanguageEdition`.
- Se eliminan `requireEdition3` y todas sus llamadas; los dos sitios donde la llamada también consumía el token
  conservan explícitamente ese `consume()`.
- El parser de fragmentos interpolados usa exactamente el mismo contrato acumulativo que el parser principal.
- `FrontendCompilation` deja de exponer un constructor y un accessor de edición.
- `ProjectSyntaxUnit` deja de transportar la edición del proyecto hasta el parser.
- `ProjectCompilation` construye unidades fuente, precompiladas y de plantillas sin pasar una edición.
- Las pruebas históricas T00 y T49 construyen el frontend acumulativo sin seleccionar V2 o V3.
- Se actualizaron comentarios del parser que todavía describían reglas por edición, sin renombrar archivos,
  targets o pruebas.

## Archivos

- `include/nativesharp/Parser/Parser.h`
- `src/Parser/Parser.cpp`
- `include/nativesharp/Frontend/FrontendCompilation.h`
- `src/Frontend/FrontendCompilation.cpp`
- `include/nativesharp/Frontend/ProjectSyntaxUnit.h`
- `src/Frontend/ProjectSyntaxUnit.cpp`
- `src/Frontend/ProjectCompilation.cpp`
- `tests/unit/NativeSharp3T00ConstitutionTests.cpp`
- `tests/unit/NativeSharp3T49LanguageSurfaceTests.cpp`

## Verificación previa al commit

- `git diff --check`: sin errores.
- `requireEdition3` en parser/frontend: cero referencias restantes.
- Constructores explícitos de `FrontendCompilation` con `LanguageEdition`: cero referencias restantes.
- Pruebas todavía no ejecutadas: por metodología, el commit precede a la reconstrucción y a la batería completa.

## Verificación posterior al commit

### Intento 1: `e63eaa90c`

- Corrida: `universal-o0-t03-20261007`.
- La configuración limpia pasó.
- La construcción no produjo `nsc.exe`; por tanto CTest, fixtures y triage no se ejecutaron.
- Clang detectó cinco variables que sólo alimentaban al gate retirado.
- Dos consumidores multilínea de `ProjectSyntaxUnit` en `Driver.cpp` todavía pasaban la edición histórica de
  NSMETA al parser.
- Se retiraron exclusivamente esos siete residuos; no se cambió semántica ni se amplió el alcance.

### Intento 2: `b7e88a389`

- Corrida: `universal-o0-t03-r2-20261007`.
- La reconstrucción limpia del compilador pasó: 290/290 pasos.
- La configuración limpia de CTest encontró los 429 ejecutables previstos.
- La construcción de CTest llegó a 1148/1151 pasos y falló antes de ejecutar CTest porque
  `NativeSharp3T01ExpressionTests.cpp` todavía pasaba una edición al constructor de `Parser`.
- La matriz de fixtures sí concluyó: 89/90; sólo `cc1_c23_views_and_lifetimes` conservó el fallo funcional
  preexistente (exit 8, esperado 42).
- Bug triage concluyó 9/9.
- Una auditoría global de consumidores directos de `Parser` encontró el mismo contrato residual en T01 y T02.
  Ambos helpers se convirtieron a una sola ejecución acumulativa, sin duplicar el mismo parseo bajo etiquetas
  V2/V3 que ya no alteran la gramática.

### Intento 3: `48931781d`

- Corrida: `universal-o0-t03-r3-20261007`.
- Tiempo total: 1304.987 s.
- Reconstrucción limpia del compilador: aprobada, 290/290 pasos.
- Construcción limpia de CTest: aprobada, 1151/1151 pasos para 429 ejecutables.
- CTest completo: 639/684; 45 fallos, exactamente el mismo total que al cierre de la tanda 2.
- Fixtures O0: 89/90; sólo `cc1_c23_views_and_lifetimes` conservó el fallo funcional preexistente
  (exit 8, esperado 42).
- Bug triage O0: 9/9.
- `nsc.ns3.t01.expressions`: aprobado.
- `nsc.ns3.t02.aggregates`: aprobado.
- `nsc.ns3.t12.semantic-modules`: aprobado.
- `nsc.project-graph`: aprobado.
- Las ocho pruebas de Language Service continuaron aprobadas.
- No se escribió en `C:` y no se ejecutó certificación de baseline.

La igualdad de 45 fallos respecto del corte anterior, junto con la aprobación de los consumidores modificados,
demuestra que esta tanda cerró el plumbing de edición del parser/frontend sin introducir fallos adicionales. Los
45 fallos restantes pertenecen a las siguientes fronteras funcionales todavía no migradas: proyecto/NSMETA,
ownership, contratos de errores y self-host derivado.
