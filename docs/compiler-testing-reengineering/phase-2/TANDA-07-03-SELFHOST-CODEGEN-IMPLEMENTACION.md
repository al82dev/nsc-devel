# Tanda 07.03 — materialización aislada de SelfHost.CodeGen

## 1. Alcance canónico

`SelfHost.CodeGen` es el componente inmediatamente posterior a `SelfHost.Bootstrap` en
`inventory/implementation-waves.csv`. Esta tanda materializa e integra su superficie de pruebas
sin agregar casos no ejecutables al manifiesto ordinario.

| Magnitud | Total |
|---|---:|
| Archivos de producto inventariados | 1 |
| Contratos | 19 |
| Casos | 136 |
| IDs | `TSH-00052..TSH-00187` |
| Capa `unit` | 37 |
| Capa `component` | 75 |
| Capa `integration` | 24 |
| Perfil `PR` | 120 |
| Perfil `NIGHTLY` | 16 |

La matriz asigna los 136 casos a `nsc_suite_controller+nsc` y a la familia de fixtures
`ns/selfhost/selfhost-codegen`.

## 2. Módulos consolidados

- `SelfHostCodeGenFoundationModule.cpp` materializa `TSH-00052..TSH-00120`, contratos
  `CSH-CG-001..010`.
- `SelfHostCodeGenLoweringEmissionModule.cpp` materializa `TSH-00121..TSH-00187`, contratos
  `CSH-CG-011..019`.
- `SelfHostCodeGenTestSupport.h` genera cada ID individual y conserva componente, contrato, capa,
  perfil, oracle y disposición.

Los descriptores compactos no reducen el denominador: sus rangos se expanden a un `TestCase` por
ID. El gate reconstruye esa expansión y la compara con las 136 filas canónicas.

## 3. Trazabilidad contrato-oracle

| Contrato | Casos | Capa | Oracle |
|---|---:|---|---|
| `CSH-CG-001` | 6 | unit | precedencia lookup, push/pop y estado break |
| `CSH-CG-002` | 9 | component | símbolos alcanzables y entry seleccionada |
| `CSH-CG-003` | 6 | unit | kind LLVM, ancho, elemento y agregado |
| `CSH-CG-004` | 6 | unit | parámetros, receiver, retorno y convención |
| `CSH-CG-005` | 8 | component | nombres, linkage, inicializadores y declaraciones |
| `CSH-CG-006` | 8 | component | símbolo/firma seleccionados y diagnóstico no resuelto |
| `CSH-CG-007` | 6 | unit | opcode, anchos, signedness y tipo resultado |
| `CSH-CG-008` | 6 | unit | instrucción, ordering, alignment, mask y bytes |
| `CSH-CG-009` | 7 | component | longitud, bytes, allocations y comparación |
| `CSH-CG-010` | 7 | integration | hook, firma, opcode, ordering y resultado |
| `CSH-CG-011` | 7 | component | layout de celda, llamada, ramas, argumentos y phi |
| `CSH-CG-012` | 7 | component | procedencia de dirección, GEP, offsets y tipo |
| `CSH-CG-013` | 7 | component | opcodes, constantes, ramas, calls, memoria y tipo |
| `CSH-CG-014` | 7 | component | aristas CFG, phi y alcanzabilidad de efectos |
| `CSH-CG-015` | 8 | component | template, constraints, convención, efectos y salidas |
| `CSH-CG-016` | 7 | component | bloques/aristas, terminadores, retornos y cleanup |
| `CSH-CG-017` | 8 | integration | cuerpos, verifier y diagnóstico |
| `CSH-CG-018` | 7 | unit | nivel exacto y texto del pipeline |
| `CSH-CG-019` | 9 | integration | exit, artefacto, verificación y estructura emitida |

## 4. Clasificación

- `InfrastructurePending`: 136.
- `KnownProductDefect`: 0.
- Ejecutables: 0.
- Filas de manifiesto: 0.
- PASS afirmados: 0.

La implementación depende de `SelfHost.IR`, `SelfHost.Sema`, `NativeSharp.LLVM.C` y del ABI
Windows x64. El controlador actual no compila ni invoca `LlvmBackend`, no inyecta productores
self-host, no controla la frontera LLVM C y no observa tipos, valores, CFG, verificador,
diagnósticos o artefactos COFF/assembly. Además, las bibliotecas externas están excluidas de esta
tanda. Implementar dobles que repliquen ese lowering cambiaría el objeto probado.

No se asigna `KnownProductDefect`: sin el ejecutor y los productores necesarios no existe todavía
un fallo reproducible por ID contra su oracle. Esta clasificación no afirma corrección del
producto; conserva el trabajo como infraestructura pendiente.

## 5. Fragmento y gate

No se crea fragmento JSONL: los fragmentos son exclusivamente para filas ejecutables y el conteo
ejecutable es cero. `run_selfhost_codegen_gate.py` contrasta los 19 contratos, 136 IDs, rangos,
capas, perfiles, oracles, disposición y ausencia de filas ejecutables. Está registrado como
`nsc.compiler-suite.selfhost-codegen-structure`; no exige PASS y no lanza el producto.

## 6. Exclusiones y validación

No se modificaron fuentes de producto, batería histórica, coverage, bibliotecas externas, C:,
remotos ni archivos protegidos. Los dos registros se conectaron en `TestRegistry.h` y `main.cpp`;
las dos fuentes y el gate estructural se conectaron en `CMakeLists.txt`. No se tocaron
`TRACKING.md` ni `CURSOR.json`. No se ejecutaron build, pruebas, gates ni validación; el resultado
es materialización e integración estáticas pendientes de validación offline.
