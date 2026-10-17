# NSC 3.0.5 — R04A: continuaciones iterativas de `foreach` en el parser

## SCOPE LOCK

- **Repositorio:** `A:\nsc-devel`.
- **Rama autorizada:** `development`.
- **Componente:** parser de statements de NSC.
- **Defecto reproducible:** 256 cuerpos `foreach` anidados terminan con
  `0xC00000FD` después de `syntax-lex` y antes de completar `syntax-parse`.
- **Causa estructural que se corrige:** la arista indirecta
  `parseForeachStatement -> parseBlock -> parseStatement -> parseForeachStatement`
  conserva un frame grande por nivel mientras el cuerpo todavía no ha sido analizado.
- **Símbolos autorizados:** `Parser::parseBlock`, `Parser::parseForeachStatement` y helpers/estado
  privados nuevos que separen lectura de encabezado, continuación de cuerpo y materialización del
  AST existente.
- **Archivos de producción autorizados:**
  `include/nativesharp/Parser/Parser.h` y `src/Parser/Parser.cpp`.
- **Evidencia focal autorizada:**
  `tests/unit/ProjectMethodBodyModelTests.cpp`, este expediente y el tracking externo
  `A:\handover-deepseek\restantes-optimizacion-deepseek.md`.
- **Consumidor de prueba:** `ProjectCompilation::compileProjectMethodBodies`; debe atravesar lexer,
  parser y method bodies reales.
- **Contratos congelados:** sintaxis, árbol AST y sus ranges/identidades sintéticas, orden y texto de
  diagnostics, semántica de `foreach`, ownership, API, ABI, NSMETA, CLI y artefactos 3.0.4.
- **Prohibido:** aumentar el stack, reducir profundidad, aceptar un guard de profundidad como
  solución, reparsear con otra gramática, introducir una ruta AST alternativa, ejecutar baseline,
  batería completa, deploy o instalación.
- **Paralelismo:** fuera de alcance; el parser conserva su orden determinista.
- **Terminación de R04A:** el caso focal de 256 niveles alcanza method bodies sin stack overflow;
  el target focal completo conserva sus comprobaciones; `nsc` construye; la arista indirecta de
  `foreach` queda sustituida por frames explícitos; expediente, tracking y commit local quedan
  cerrados juntos.
- **No terminación de R04:** R04 permanece activo hasta convertir o clasificar con evidencia las
  demás familias estructuradas. R04A no autoriza declarar iterativo todo el parser de statements.

## Evidencia de entrada

- 192 niveles ya alcanzan y pasan method bodies.
- 256 niveles fallan dentro de `syntax-parse` con stack overflow.
- Regresiones profundas de 256 `if`, 256 `while`, 128 `for`, 256 `using`, 256 `switch` y 256
  `try/finally` ya pasan, pero aún conservan aristas recursivas de parser que se tratarán como
  subítems separados de R04.

## Diseño causal

1. Leer el encabezado de `foreach` en un estado movible que preserve exactamente tokens, tipos,
   pattern, expresiones y nombres sintéticos actuales.
2. Abrir cada cuerpo con un frame explícito que mantenga el bloque y su destino efectivo para
   `defer`.
3. Cuando el siguiente statement sea otro `foreach`, apilar su encabezado y cuerpo sin entrar otra
   vez al stack C++.
4. Al cerrar `}`, materializar el mismo AST de range/array/span/enumerator y anexarlo al frame padre.
5. Compartir el anexo de statements de bloque para conservar el tratamiento exacto de
   `defer-registration` y `conditional-pattern-inline`.

## Gates focales

- El test profundo se eleva de 192 a 256; no se reduce ni se omite si falla.
- `nsc_project_method_body_model_tests` debe terminar con salida PASS y código cero.
- El target `nsc` debe construir con el toolchain oficial disponible por `PATH`.
- Una inspección estática debe confirmar que un cuerpo `foreach` anidado se apila y no llama a
  `parseForeachStatement` desde su propia continuación.
- No se ejecuta la matriz completa; la certificación GUI continúa reservada para el usuario.

## Estado

R04A terminado el 2026-10-17.

## Implementación cerrada

- `ForeachParseState` conserva fuera del stack C++ toda la información que antes quedaba viva al
  entrar al cuerpo.
- `parseForeachHeader` consume exclusivamente la cláusula y preserva el orden de asignación de
  identidades sintéticas, parseo duplicado de la colección y diagnostics existentes.
- `parseForeachStatement` usa `ForeachFrame` en un `std::vector` para abrir, cerrar y anexar cuerpos
  `foreach` anidados sin reentrar a esa función por `parseBlock`.
- `lowerForeachStatement` contiene sin cambios contractuales la materialización range/array/span/
  enumerator que antes ocurría después del retorno recursivo.
- `appendParsedStatement` centraliza la lógica preexistente de `defer-registration` y
  `conditional-pattern-inline`; tanto `parseBlock` como la continuación iterativa consumen la
  misma regla.
- La regresión profunda queda permanentemente en 256 niveles.

## Resultados

- Construcción de `nsc_project_method_body_model_tests`: PASS, código 0.
- Ejecución focal: `PASS: 87 checks`, código 0.
- El caso de 256 `foreach` alcanza y termina method bodies; ya no produce `0xC00000FD`.
- Construcción de `build-clang` target `nsc`: PASS, código 0.
- Contrato de host runtime estático de `nsc.exe`: PASS.
- `git diff --check`: PASS.
- Inspección estática: la continuación de `parseForeachStatement` no llama `parseBlock` ni vuelve a
  llamar `parseForeachStatement`; los hijos directos se apilan como `ForeachFrame`.

No se ejecutaron baseline, matriz completa, deploy ni instalación. R04 global permanece activo.
