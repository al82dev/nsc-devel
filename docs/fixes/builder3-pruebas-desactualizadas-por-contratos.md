> **Aviso vigente sobre NSMeta:** El versionado de NSMeta ya no aplica. Las menciones de versiones, compatibilidad por versión y requisitos de versión de este documento son históricas y no constituyen un contrato vigente. No debe reintroducirse el versionado de NSMeta ni rechazarse su lectura o compilación por una versión; la aceptación se determina por el contenido y los contratos aplicables.

# Pruebas de NSC Builder que congelaban contratos ya cambiados

Corrida de NSC Builder `20261115-210204-df82b231`. Cada sección es una hipótesis causal distinta y
tiene su propio commit. En todas, el producto cambió deliberadamente su contrato en un commit
identificado y la prueba conservó el contrato anterior; la prueba se actualiza al contrato vigente y
sigue comprobando valores exactos, de modo que una regresión del producto la vuelve a hacer fallar.
No se cambia código de producto.

## TL-LANG-282 (CL-LANG-021): tabla de operadores

- Observado: "the operator registry must contain exactly 20 rows"; el registro tiene 22.
- Causa: `d00b3a869` (NSC-NCSS-003, `++`/`--` definidos por usuario) añadió
  `{"++", 1, "lang:Increment", "op_Increment"}` y `{"--", 1, "lang:Decrement", "op_Decrement"}`
  después del menos unario en `kOperatorTraits`; `BuildAndDeclaration.cpp` y `CallAnalyzer.cpp` los
  consumen.
- Cambio: `kExpectedOperators` pasa a 22 filas con esas dos en la misma posición; el caso sigue
  comparando número de filas, orden y los cuatro campos de cada fila.

## TA-AST-023 (CA-AST-003): límite superior de `AstNodeKind`

- Observado: el ordinal 108 ya no es `UnknownAstNode`.
- Causa: `94d35b132` (iteración tipada de campos estáticos) añadió `StaticFieldIterationStatement`
  en el ordinal 108.
- Cambio: el oráculo independiente `ExpectedAstKinds` incluye ese tipo (109 entradas) y TA-AST-025
  fija el final del rango añadido en `StaticFieldIterationStatement`. TA-AST-023 vuelve a comprobar
  el primer ordinal fuera del dominio (109) y el máximo del tipo subyacente.

## TF-DRIVER-075 y TF-DRIVER-083 (CF-DRIVER-010/011): traza de tiempos JSON

- Observado: la variante 3 de ambos contratos esperaba `"total_ms": 0`.
- Causa: `d0d9c3494` (LKF-243) escribe `null` para una métrica que la salida no trajo o que se
  rechazó (negativa), para distinguir "no medido" de un cero medido.
- Cambio: esas dos variantes esperan `"total_ms": null`; las demás siguen exigiendo los valores
  medidos (`"total_ms": 9`, contadores de proceso, 1000 fases).

## TF-INTEROP-341 (CF-INTEROP-043): puertas del compilador actual

- Observado: la variante 5 (`formatMinor` = actual + 1) esperaba rechazo.
- Causa: NSC-META-001 (`2320596aa` y siguientes) verifica NSMETA por contenido y ABI, no por su
  etiqueta de versión; `LibraryMetadataVersionToleranceTests` exige ya que una etiqueta menor futura
  no rechace. La regla vigente del proyecto prohíbe rechazar por versión de NSMeta.
- Alcance: `LibraryMetadataCodec` es la metadata de bibliotecas NativeSharp, no la
  interoperabilidad C/C++; no se toca código de interoperabilidad ni el codec.
- Cambio: la variante 5 espera aceptación. Las variantes 3, 4 y 6 (módulo vacío, triple y target
  distintos) siguen exigiendo rechazo con diagnóstico.

## TSH-01001, 01002, 01006, 01026 y 01008 (SelfHost.Types)

- Observado: códigos 21, 22, 26 y 46; TSH-01008 (capa `component`, fuera de la corrida `unit`)
  devolvía 28 por la misma causa.
- Causa: `05a105dfc` añadió `sbyte` como builtin 16 de `TypeTable` (tamaño 1, alineación 1, entero
  con signo). Las pruebas congelaban 16 builtins y el primer ID nominal en 16.
- Cambio: 17 builtins, la fila 16 `sbyte` en `BuiltinInventory`, el índice 16 como entero y
  numérico en `NumericTruthTable`, y el primer ID nominal en 17 con 18 tipos (TSH-01008).

## Validación

- Host: `nsc_component_tests` Debug y Release, 58 casos de los seis contratos, todos PASS
  (`A:/nsc-fixes-evidencia/b3fix/host.txt`). Sin niveles O0–O3 (casos `NA`).
- Self-host: `SelfHostTypes.nsproj` materializado con `selfhost_inventory.py` y compilado con
  `build-clang/bin/nsc.exe` en debug y release, O0, O1, O2 y O3; los casos del manifiesto para ese
  proyecto devolvieron el código esperado en las ocho variantes (`A:/nsc-fixes-evidencia/b3fix/tsh2.txt`).
- Pendiente: las descripciones de `manifest/cases.jsonl` (por ejemplo `twenty_rows_order_and_fields_exact`
  o "valor 108") siguen con el texto anterior; no se modificaron.
