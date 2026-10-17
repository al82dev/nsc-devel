# NSC 3.0.5 — R01A: extracción física de `ParserContext`

## Estado

`IMPLEMENTADO; VALIDACIÓN DE BUILD Y EJECUCIÓN PENDIENTE DEL USUARIO`.

La tanda separa físicamente el estado básico del parser sin cambiar gramática, AST, recovery,
diagnósticos, sintaxis aceptada ni contratos públicos. Por instrucción expresa del usuario no se
compiló ni se ejecutaron pruebas. La evidencia de esta tanda es exclusivamente estática.

## SCOPE LOCK

| Dimensión | Alcance vinculante |
|---|---|
| Objetivo | Extraer cursor, expectativas, rangos y recuperación de `Parser.cpp`. |
| Naturaleza | Refactor no funcional por movimiento puro. |
| Repositorio | Únicamente `A:\nsc-devel`. |
| Rama | `development`. |
| Referencia | Estado de `Parser.cpp` en el padre inmediato de esta tanda. |
| Archivos de producto | `src/Parser/Parser.cpp`, `src/Parser/ParserContext.cpp` y `CMakeLists.txt`. |
| Prueba focal | `tests/compiler-suite/host/modules/ParserCursorRecoveryModule.cpp`. |
| Contratos congelados | Sintaxis, AST, rangos, cursor, recovery, diagnósticos, ABI, NSMETA y CLI. |
| Fuera de alcance | Reescribir parsing, cambiar algoritmos, diagnostics, recovery o API pública. |
| Acciones prohibidas | Build, ejecución de pruebas, baseline, deploy, instalación y red. |
| Criterio estático | Una definición por símbolo, cuerpos idénticos, CMake explícito y funciones <=60 líneas. |

## Causa y frontera

`Parser.cpp` concentraba 17,111 líneas y mezclaba la fachada de compilación con servicios básicos
del cursor. R01A autoriza únicamente extraer la responsabilidad `ParserContext`: estado inicial,
consulta y avance de tokens, expectativas, construcción de rangos y sincronización de errores.

La prueba contrafactual por símbolo confirmó que estos métodos no contienen reglas de declaración,
miembros, tipos, statements, expresiones, patrones, genéricos ni atributos. Esas familias permanecen
en `Parser.cpp` y conservan sus cuerpos.

## Inventario movido

El nuevo `src/Parser/ParserContext.cpp` contiene exactamente estas quince definiciones:

1. `Parser::Parser`;
2. `Parser::current`;
3. `Parser::peek`;
4. `Parser::at`;
5. `Parser::atContextualKeyword`;
6. `Parser::startsParenthesizedBlock`;
7. `Parser::expectContextualKeyword`;
8. `Parser::consumeIf`;
9. `Parser::consume`;
10. `Parser::expect`;
11. `Parser::join`;
12. `Parser::tokenRangeFrom`;
13. `Parser::makeErrorExpression`;
14. `Parser::synchronizeGlobal`;
15. `Parser::synchronizeStatement`.

No se añadió otro tipo, wrapper, interfaz ni estado. `Parser` conserva la misma definición pública
y privada en `include/nativesharp/Parser/Parser.h`.

## Equivalencia

Un comparador de llaves extrajo cada cuerpo desde el `Parser.cpp` de `HEAD` y desde
`ParserContext.cpp`. Los quince pares produjeron igualdad textual exacta, incluido el constructor.
Los comentarios documentales se añadieron fuera de los cuerpos y no modifican la conducta.

La búsqueda sobre todos los `.cpp` de `src/Parser` encontró una sola definición para cada símbolo
movido, todas en `ParserContext.cpp`. `CMakeLists.txt` enumera explícitamente la nueva unidad junto a
`Parser.cpp`.

## Tamaño de funciones

El conteo estático conservador de líneas ejecutables obtuvo:

| Símbolo | Líneas ejecutables |
|---|---:|
| `Parser::Parser` | 12 |
| `current` | 3 |
| `peek` | 6 |
| `at` | 1 |
| `atContextualKeyword` | 1 |
| `startsParenthesizedBlock` | 15 |
| `expectContextualKeyword` | 10 |
| `consumeIf` | 4 |
| `consume` | 7 |
| `expect` | 8 |
| `join` | 1 |
| `tokenRangeFrom` | 2 |
| `makeErrorExpression` | 3 |
| `synchronizeGlobal` | 9 |
| `synchronizeStatement` | 4 |

Ninguna función movida supera 60 líneas ejecutables. No se introdujo recursión ni búsqueda
adicional. Cursor y recovery mantienen complejidad y orden originales.

## Prueba focal registrada

El módulo focal existente `ParserCursorRecoveryModule.cpp` ya cubre constructor, EOF, cursor,
lookahead, consumo, expectativas, contextual keywords, error expression y ambas sincronizaciones.
R01A amplía `TP-PAR-002` para comprobar directamente `tokenRangeFrom(0)`: el rango consumido debe
terminar en el `;` de la declaración, antes del token EOF.

No se creó un caso nuevo ni se alteró el manifiesto. Se reforzó el caso que ya pertenece al contrato
`CP-PAR-001`. La prueba no se ejecutó en esta tanda.

## Auditoría de contratos

- No cambió `Parser.h`, por lo que no existe diferencia de API ni layout.
- No cambió ninguna condición, orden de consumo, texto de diagnóstico ni frontera de recovery.
- No cambió la creación de nodos AST ni sus rangos.
- No cambió sintaxis, semántica, ABI, metadata ni NSMETA.
- No se añadió recursión, concurrencia, estado global, caché ni allocation.
- No se tocó ningún consumidor, fixture de lenguaje ni expectativa preexistente.

## Gates no ejecutados

Quedan pendientes para el barrido integral ordenado por el usuario:

- compilar el target que contiene `nsc` y el host parser;
- ejecutar el caso focal `TP-PAR-002` y el componente `Host.Parser`;
- comparar AST y diagnósticos de la matriz completa contra la referencia 3.0.4;
- ejecutar el baseline final desde la GUI.

La ausencia de ejecución impide declarar R01A compilado, probado o verificado. El cierre de esta
hoja significa únicamente que la implementación y auditoría estática del movimiento puro quedaron
completas.
