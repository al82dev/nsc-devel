# NativeSharp 2.0 self-hosting — tanda 12: transición autoalojada `nsc2`

## 1. Identidad

| Campo | Valor |
|---|---|
| Estado | `LANDED` |
| Tareas | `NS2-SH-139` a `NS2-SH-149` |
| Gate | `SH11` |
| Entrada | `75dd06ed8` |
| Commit | `selfhost(tanda-12): add audited nsc1 to nsc2 transition` |
| Fecha | 2026-09-19 |

## 2. Objetivo y resultado

La tanda elimina toda ejecución de `nsc0` del paso observado `nsc1 → nsc2`. `nsc1` lee un
manifiesto absoluto, recorre cada fuente del compilador con su lexer, parser, semántica e IR/SSA,
y sólo después materializa `nsc2`. Un verificador externo observa el árbol de procesos cada
milisegundo y rechaza cualquier hijo; registra comando, PID, entorno, hashes, salida y traza.

La generación se repite desde dos árboles limpios, con rutas distintas y combinaciones distintas
de locale/timezone. Después se comparan diagnósticos, `nsmeta` y objetos de `nsc1`/`nsc2`, y se
ejecutan los cuatro perfiles con `lld-link` y NSLink.

## 3. Frontera técnica exacta

El backend NativeSharp de SH08 aún no baja todo el compilador a LLVM: emite el programa canónico
después de validar la entrada. Por eso SH11 implementa una frontera deliberadamente acotada:
`source-attested-image-replay`.

```text
fuentes clean-a ──frontend+SSA nsc1──┐
                                     ├── copia exacta nsc1 ──> nsc2-a
imagen nsc1 certificada ─────────────┘

fuentes clean-b ──frontend+SSA nsc1──┐  LANG/TZ distintos
                                     ├── copia exacta nsc1 ──> nsc2-b
imagen nsc1 certificada ─────────────┘

monitor Toolhelp: descendientes(nsc1) = ∅
SHA-256: nsc1 = nsc2-a = nsc2-b
```

Esto demuestra autonomía de proceso, atestación del conjunto fuente y punto de partida
determinista. **No demuestra todavía codegen derivado de fuentes**: cambiar una fuente hace que el
manifiesto deje de corresponder, pero no regenera instrucciones nuevas. El contrato y cada reporte
guardan `source_derived_codegen=false`; SH14 no podrá promover esta imagen mientras siga falso.

## 4. Diseño y contrato

`selfhost/contracts/self-hosted-generation.contract.json` congela:

- productor `nsc1`, producto `nsc2` y los once IDs SH11;
- método `source-attested-image-replay` y prohibición de afirmar codegen derivado;
- cinco raíces atestadas y la raíz del compilador validada internamente;
- proyectos bootstrap, corpus ejecutable, O0–O3 y ambos linkers;
- identidad CLI y retorno observable 42.

El manifiesto que consume NativeSharp contiene rutas físicas de la corrida y nunca se versiona.
El reporte conserva rutas/comandos efectivos, pero el digest fuente usa rutas relativas, tamaños,
SHA-256 y orden ordinal para que clean-a y clean-b sean comparables.

## 5. Métodos, funciones y responsabilidades

| Elemento | Responsabilidad y fallo |
|---|---|
| `GenerationBuilder.VerifySource` | carga una fuente y exige lexer, parser, semántica y SSA válidos |
| `GenerationBuilder.VerifyManifest` | consume todas las líneas no vacías y exige al menos el conjunto congelado del compilador |
| `GenerationBuilder.Build` | valida generación/entradas, reproduce la imagen y escribe una traza que declara el método real |
| `source_files` | enumera `.ns` por raíces contractuales y orden relativo estable |
| `source_manifest` | produce manifiesto físico y digest lógico independiente de la ruta absoluta |
| `process_snapshot` | captura PID, PPID e imagen mediante Toolhelp32, sin lanzar herramientas auxiliares |
| `descendants` | calcula cierre transitivo de hijos del productor observado |
| `audited_generation` | ejecuta `nsc1`, sondea cada 1 ms y exige cero descendientes |
| `verify_self_hosted_generation.main` | compara raíces, hashes, CLI, diagnósticos, metadata, objetos y ejecuciones |
| `VerifyNativeSharpSelfHostedGeneration.cmake` | crea árboles limpios, prepara `nsc1`, adapter/import-lib y ejecuta el verificador |

## 6. Archivos modificados

| Archivo | Acción | Razón |
|---|---|---|
| `selfhost/nsc/Bootstrap/GenerationBuilder.ns` | nuevo | generación autónoma, validación y traza dentro de NativeSharp |
| `selfhost/nsc/NativeSharp.Compiler.nsproj` | modificar | incluir subsistema Bootstrap |
| `selfhost/nsc/Driver/Program.ns` | modificar | CLI `--bootstrap-generation` e identidad SH11 |
| `selfhost/contracts/self-hosted-generation.contract.json` | nuevo | contrato observable y limitación normativa |
| `selfhost/bootstrap/verify_self_hosted_generation.py` | nuevo | auditoría de procesos y matriz diferencial |
| `cmake/VerifyNativeSharpSelfHostedGeneration.cmake` | nuevo | gate reproducible SH11 |
| `CMakeLists.txt` | modificar | registrar `nsc.selfhost.self-hosted-generation` |
| `tests/selfhost/test_bootstrap_infrastructure.py` | modificar | impedir sobreafirmar codegen o retirar auditoría |
| READMEs y roadmap | modificar | operación y frontera honesta |
| este expediente | nuevo | diseño, funciones, archivos, evidencia y deuda |

## 7. Matriz SH11

| Dimensión | Cobertura |
|---|---|
| productor | ruta absoluta de `nsc1`; `nsc0` sólo prepara la entrada fuera del intervalo auditado |
| aislamiento | cero procesos descendientes durante ambas generaciones |
| rutas | clean-a y clean-b absolutas y distintas |
| entorno | `C/UTC` y `tr_TR.UTF-8/Pacific-Auckland` |
| fuente | digest de cinco raíces; fuentes del compilador verificadas por frontend/SSA propio |
| diferencial | diagnóstico ausente, `nsmeta` y objetos `nsc1 == nsc2` |
| optimización | O0, O1, O2 y O3 |
| linker | `lld-link` y NSLink; ocho ejecuciones con retorno 42 |
| acumulativo | CTest, fixtures y triage completos por ambos linkers en el baseline |

## 8. Trazabilidad

| ID | Evidencia |
|---|---|
| 139 | comando `--bootstrap-generation nsc2` ejecutado por la imagen `nsc1` |
| 140 | `descendants=[]` y `nsc0_invocations_during_generation=0` |
| 141 | dos auditorías con comando, PID, ambiente, salida, tiempo y hashes |
| 142 | self-test y validación SSA de todas las fuentes del compilador |
| 143 | cuatro proyectos del perfil y corpus ejecutable |
| 144 | baseline completo y matriz local O0–O3/dual-linker |
| 145 | igualdad de diagnósticos, `nsmeta` y objetos |
| 146 | digest relativo; rutas físicas sólo en manifiesto efímero |
| 147 | clean-a/clean-b independientes |
| 148 | dos locales y zonas horarias explícitas |
| 149 | reporte con `problems=[]`; la limitación de codegen es un campo contractual, no una omisión |

## 9. Baseline y evidencia

Corrida canónica `tanda-12-sh11-final-20260919`: `PASSED` en 816.017 s, sin escrituras en `C:`.

| Dato | Resultado |
|---|---|
| CTest `lld-link` | 448/448 PASS, 337.637 s |
| Fixtures `lld-link` | 148/148 PASS, 51.693 s |
| Triage `lld-link` | 18/18 PASS, 8.518 s |
| CTest NSLink | 448/448 PASS, 324.304 s |
| Fixtures NSLink | 148/148 PASS, 48.806 s |
| Triage NSLink | 18/18 PASS, 8.329 s |
| Fuente baseline | 1425 archivos; `956e137723cc39ac79a5fd50b562b69e5ee549bcf5fcf85937f7ead48f215583` |
| Fuente SH11 | 91 archivos; `2a80839b8fae09f2450671c866ea98e0b53cb5d47e2136af39aff09055ed1961` |
| `nsc2` vía lld | `a6fcc5c22006210a4b148794d89d11d7dd62071debde7f2738e66c2559f53b01` |
| `nsc2` vía NSLink | `a9c2bf8cb57b3ac33a965e7866e88795ca57be157888a58e690722cfcc80f667` |
| Reporte baseline JSON | `a546838d1b7f1a489c84abbe819f2e8bb8ad6586aa4b2657167e0822a9300715` |
| Gate G00 | `71e9707468fef9fd5ec96d2bb70bc36b25b8dbdfa7a8f29efdc6eb95a4feca2b` |
| Reporte SH11 lld | `e021951f88c61d4a5d6cbb2f295524888d5142dff7c4c57796c31c6a28843a48` |
| Reporte SH11 NSLink | `024c658cacad3778be0aa0da66fce2f0607424cfb221bf6588f780a9c0a34a9f` |

En cada linker `nsc1`, `nsc2-a` y `nsc2-b` son byte a byte idénticos. Se conservan hashes
separados por linker porque el writer PE no forma parte del contrato de identidad cruzada.

## 10. Condición para la tanda siguiente

Tanda 13 puede demostrar un punto fijo de imágenes y repetirlo, pero debe conservar dos resultados
separados: `image_fixed_point=true` y `source_derived_codegen=false`. Ninguna comparación byte a
byte transforma la segunda condición en verdadera. El reemplazo del lowering canónico sigue siendo
trabajo técnico obligatorio antes del acta de promoción.

## 11. Cierre

- [x] diseño, métodos, funciones, archivos y límite de validez documentados;
- [x] gate aislado verde en ambos árboles CTest;
- [x] baseline completo verde;
- [x] evidencia SH11 promovida con hashes;
- [x] cambios ajenos excluidos;
- [x] commit exacto preparado: `selfhost(tanda-12): add audited nsc1 to nsc2 transition`.
