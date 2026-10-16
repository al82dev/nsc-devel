# Tanda de validación 257 — barrido de la corrida 20261015-195637-2ec1de48

## SCOPE LOCK

- Objetivo autorizado: analizar y cerrar las `18` terminales no PASS de la corrida completa O0 ejecutada por
  Alberto desde NSC Builder 3, mediante correcciones causales aisladas y pruebas focales.
- Naturaleza: corrección funcional, actualización de oráculos obsoletos demostrados e infraestructura de timeout.
- Repositorio: exclusivamente `A:\nsc-devel`, rama `development`.
- Revisión fallida congelada: `7be1cfd69814e1f9f0ed89aed048a40e5e5f54fc`.
- Corrida congelada: `A:\nsc-devel\_nscbuilder3\history\20261015-195637-2ec1de48`.
- Resultado original: `5,973 PASS`, `13 FAIL`, `1 INFRASTRUCTURE_ERROR`, `4 TIMEOUT`.
- Rango histórico causal: desde el último commit anterior a cada feature que alteró el contrato hasta
  `7be1cfd69814e1f9f0ed89aed048a40e5e5f54fc`; no se reescribe historia.
- Configuración de validación focal: `Debug`, pruebas host con optimización `NA`, lenguaje en `O0` cuando aplique.
- Toolchain: se conserva la configuración oficial del proyecto; no se modifica ni autodetecta otra raíz.
- Comportamiento que debe permanecer idéntico: los `5,973` resultados PASS, los ordinales históricos de enums y
  tokens, semántica del lenguaje ajena a los fallos y configuración funcional del Builder.
- Acciones prohibidas: batería completa, minibaseline, baseline, deploy, instalación, push, pull, fetch,
  versionamiento NSMETA, cambio de rama, descarte de trabajo ajeno y limpieza global.
- Criterio de terminación: todas las causas raíz conocidas tienen cambio mínimo, documento, commit local aislado y
  prueba focal aprobada; la batería completa posterior queda reservada a Alberto.

## Presupuesto de cambio

| Productor | Archivos y símbolos autorizados | Casos |
|---|---|---|
| Target | `BuiltinLayoutKind`, catálogo host, manifiesto y contratos documentales directos | `TF-TGT-002`, `005`, `006`, `008`, `036`, `038`, `040` |
| AST | catálogo host y contratos documentales directos; el enum de producto queda protegido | fallos `TA-AST-021`, `023`, `025`; vecindad `019..026` |
| Lexer | `TokenKind`, catálogo host y contratos documentales directos | fallos `TL-LEX-001`, `002`; vecindad `001..008` |
| Parser foundation | seed esperado del host y contratos documentales directos; constructor de producto protegido | `TP-PAR-007` |
| Parser nested declarations | reconocimiento de `partial` antes de un tipo anidado y expediente directo | fallo `TP-PAR-199`; vecindad `197..201` |
| Parser properties | oráculo host y contratos documentales directos; implementación `ref` protegida | `TP-PAR-239` |
| Parser unary robustness | prefijos unarios de `Parser::parseUnaryExpression`; oráculo sin cambios | `TP-PAR-532` |
| Builder timeout | `timeout_limit_for_average`, pruebas unitarias y documentación directa | `T12B-00012`, `00025`, `01069`, `01073` |

Todo archivo o símbolo no enumerado permanece protegido hasta que una evidencia de esta corrida demuestre que
participa en la ruta causal y se registre aquí antes de modificarlo.

## Inventario congelado y grafo causal

| Grupo | Terminales | Clasificación inicial | Productor | Estado |
|---|---:|---|---|---|
| Target | 5 | regresión de identidad ordinal al insertar `Decimal` | `BuiltinLayoutKind` | corregida; `7/7 PASS` con vecindad |
| AST | 3 | oráculo anterior a tres nodos append-only | catálogo CA-AST-003 | corregida; `8/8 PASS` con vecindad |
| Lexer | 2 | regresión ordinal al insertar `Utf8StringLiteral` en posición 7 | `TokenKind` | corregida; `8/8 PASS` con vecindad |
| Parser seed | 1 | oráculo anterior a `Future`, `Iterator` y `AsyncIterator` | CP-PAR-001 | corregida; `1/1 PASS` |
| Parser partial | 1 | `partial struct` cae en la ruta de miembro parcial | `parseClassMember` | corregida; `5/5 PASS` con vecindad |
| Parser ref property | 1 | oráculo anterior a la feature `e1629a749` | CP-PAR-023 | causa demostrada |
| Parser nesting | 1 | recursión por cada prefijo unario; stack overflow `0xC00000FD` | `parseUnaryExpression` | reproducido |
| Builder timeout | 4 | margen histórico insuficiente para variación de I/O corta | `timeout_limit_for_average` | causa demostrada |

Los fallos de cada grupo son consumidores del productor indicado. No se parchean de manera independiente.

## Evidencia causal previa al cambio

### Target

El commit `59f0ca311` insertó `Decimal` antes de `Enum` y `Pointer`. El estado anterior fijaba `Enum=16` y
`Pointer=17`; la inserción desplazó ambos. `Decimal` puede añadirse al final sin cambiar su layout ni consumidores
simbólicos. Los cinco fallos son manifestaciones del mismo desplazamiento. La auditoría de vecindad añadió
`TF-TGT-038` y `TF-TGT-040`: ambos seguían pasando porque su tabla de `18` entradas también omitía `Decimal`, por
lo que deben extenderse a `19` para evitar cobertura falsamente verde.

### AST

El enum actual conserva el orden append-only y termina en `SuspendStatement`, `NameOfExpression` y
`OverflowPolicyExpression`. El oráculo sólo contiene `NameOfExpression` y declara `106` valores, por lo que difiere
desde la posición `105`. No se modifica el producto. Como el catálogo es compartido por los ocho casos de
`CA-AST-003`, la validación focal cubre `TA-AST-019..026`, aunque sólo `021`, `023` y `025` fallaron en la corrida.

### Lexer

El commit `82d85d29` insertó `Utf8StringLiteral` después de `StringLiteral`, pese al contrato documentado que exige
append-only para identidades serializadas. El enum previo terminaba en `DotDotEqual`; el token UTF-8 puede añadirse
después sin cambiar el lexer que usa el nombre simbólico. Los ocho casos `CL-LEX-001` recorren el mismo catálogo y
se validan juntos para cubrir tanto la identidad como los clasificadores que dependen de rangos y operadores.

### Parser

- El constructor actual siembra `19` nombres: los `16` previos más `Future`, `Iterator` y `AsyncIterator`.
- `partial struct` se consume como modificador de miembro porque `partialNestedModifier` sólo reconoce `class` y
  modificadores intermedios; así omite el diagnóstico que `parseNestedTypeDeclaration` ya implementa. La ruta de
  miembro además consulta el token actual `partial` con `at(Identifier)` en vez del token posterior. Se corrige el
  lookahead para los inicios de tipo anidado ya soportados y se valida la vecindad completa `TP-PAR-197..201`.
- `e1629a749` hizo legal la propiedad `ref` y conserva el flag en el getter; TP-PAR-239 todavía exige rechazo.
- TP-PAR-532 reproduce salida `-1073741571` (`0xC00000FD`) con el host Debug de la revisión fallida. El helper
  `flatten` ya es iterativo; la recursión demostrada está en la cadena de 256 llamadas de prefijos unarios.

### Builder timeout

Las cuatro pruebas habían pasado históricamente `18/20/20/20` veces. Sus máximos PASS eran `4.866`, `8.703`,
`3.100` y `2.249` segundos. En la corrida congelada agotaron límites `10/11/5/5` a `10.104/11.090/9.382/9.255`
segundos. Los logs muestran trabajo útil completo o en la segunda repetición; los casos cortos con staging de
filesystem quedan expuestos a una variación superior al margen fijo de cinco segundos.

## Plan de commits y validación

Cada fila se cierra en un commit local independiente después de construir sólo el target afectado y ejecutar
exclusivamente sus casos focales:

1. restaurar ordinales históricos de Target y extender el catálogo con `Decimal` al final;
2. actualizar el catálogo AST append-only;
3. restaurar ordinales históricos del Lexer y añadir UTF-8 al final;
4. actualizar el seed esperado de Parser;
5. corregir el reconocimiento de `partial` para tipos anidados no admitidos;
6. actualizar TP-PAR-239 al contrato vigente de getter `ref`;
7. hacer iterativa la cadena demostrada de prefijos unarios;
8. ampliar el piso de timeout corto y validar su función sin ejecutar la suite.

La corrida integral no se ejecutará en esta tanda. Tras cerrar todas las causas, Alberto decidirá y ejecutará la
siguiente batería desde la GUI.

## Estado

- Inventario original: congelado.
- Causas raíz conocidas: `8`.
- Causas corregidas: `5`.
- Pruebas focales aprobadas: `29`.
- Target: `nsc_component_tests` Debug compiló; `TF-TGT-002`, `005`, `006`, `008`, `036`, `038` y `040`
  pasaron individualmente. Se restauraron `Enum=16` y `Pointer=17`; `Decimal=18` quedó añadido al final.
- AST: `nsc_component_tests` Debug compiló; `TA-AST-019..026` pasaron individualmente. El catálogo host ahora
  reproduce las `108` entradas reales, incluyendo `SuspendStatement` y `OverflowPolicyExpression`; el producto no
  se modificó.
- Lexer: `nsc_component_tests` Debug compiló; `TL-LEX-001..008` pasaron individualmente. Se restauraron los
  ordinales previos y `Utf8StringLiteral` quedó añadido al final como ordinal `151` de `152` entradas.
- Parser seed: `nsc_component_tests` Debug compiló y `TP-PAR-007` pasó. El oráculo cubre los `19` nombres
  predeterminados vigentes y la seed externa; el constructor de producto no se modificó.
- Parser partial: `nsc_component_tests` Debug compiló; `TP-PAR-197..201` pasaron individualmente. El lookahead
  distingue tipos anidados de miembros parciales y deja que la regla existente diagnostique `partial struct`.
- Siguiente acción causal: actualizar TP-PAR-239 al contrato vigente de getter `ref`.
