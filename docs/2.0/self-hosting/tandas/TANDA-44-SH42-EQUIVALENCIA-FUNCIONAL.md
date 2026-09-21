# Tanda 44 — SH42: equivalencia funcional nsc0/nsc2

| Campo | Valor |
|---|---|
| Estado | `LANDED` — gate dirigido y baseline acumulado aprobados |
| Dependencia | SH41 `2d9207efa` |
| Gate | SH42 |
| Instalación de trabajo | `A:\native-sharp` |
| LLVM | `C:\clang-llvm` |

## Objetivo y criterio

SH41 demostró que `nsc2` nace de fuentes NativeSharp. SH42 demuestra una propiedad distinta: que
esa imagen conserva el comportamiento observable de la semilla en las capas ya portadas. La
comparación no exige bytes iguales entre implementaciones diferentes; exige identidad donde el
formato es canónico y equivalencia estructural y de ejecución donde las representaciones internas
son deliberadamente distintas.

Las nueve capas obligatorias son diagnósticos, tokens, AST, semántica, IR, objetos, metadata,
productos y proyectos. No se permite omitir una capa ni aprobar por el mero hecho de que un
ejecutable arranque.

## Diseño

### Fuente del candidato

`VerifyNativeSharpFunctionalEquivalence.cmake` consume exclusivamente
`selfhost-sh41/nsc2-o{0,1,2,3}-lld.exe`. No reconstruye candidatos con `nsc0`; por ello el gate mide
el compilador realmente autoalojado. CTest declara dependencia de SH41 y ejecuta SH42 de forma
serial.

### Comparadores por capa

- Diagnósticos: código, stdout y stderr canónicos idénticos para opción desconocida y fuente
  inexistente en las cuatro imágenes.
- Frontend: `verify_frontend.py` compara 1,046 dumps de tokens, 32 AST y 10 casos de recuperación.
- Semántica: `verify_semantics.py` compara símbolos/hechos de 12 fixtures y estabilidad de todo el
  corpus; conserva 28 invariantes explícitos.
- IR: `verify_ir.py` compara conjuntos de funciones con `nsc0`, estabilidad O0–O3, verificadores y
  orden de pases en HIR, MIR, CFG, dominancia y SSA.
- Objetos: `coff_summary()` lee directamente cabecera, secciones y tabla de símbolos COFF; rechaza
  archivos no AMD64 o truncados. Los objetos de ambas implementaciones se enlazan y ejecutan.
- Proyectos: `parse_project_dump()` reduce los dos formatos textuales a nombre, tipo y número de
  fuentes antes de comparar.
- Metadata: `parse_legacy_metadata()` valida el encabezado/payload NSMETA 1.20 binario y
  `parse_modern_metadata()` valida NSMETA/2; se comparan módulo, clase de producto, target y ABI, y
  cada archivo se vuelve a leer con su productor.
- Productos: `nsc0` genera ejecutables O0–O3; `nsc2` genera objetos y ocho planes de producto. Cada
  producto ejecutable se enlaza con LLD y NSLink y debe devolver 42.

La normalización se limita a CRLF/LF en memoria. No se altera ningún archivo, no se ocultan símbolos
y no se normalizan timestamps ni build IDs.

## Archivos y funciones

| Archivo | Responsabilidad |
|---|---|
| `selfhost/bootstrap/verify_functional_equivalence.py` | orquestación, comparadores, COFF, enlace, ejecución y reporte |
| `cmake/VerifyNativeSharpFunctionalEquivalence.cmake` | resolución configurable de herramientas, candidatos SH41 e import library |
| `selfhost/contracts/functional-equivalence.contract.json` | matriz, capas y política de normalización |
| `selfhost/tests/functional-equivalence/Program.ns` | programa común de proyecto con resultado 42 |
| `selfhost/tests/functional-equivalence/*.nsproj` | productos Exe y Library comunes |
| `CMakeLists.txt` | gate serial y dependencia SH41 → SH42 → promotion-readiness |

Funciones centrales nuevas: `call_verifier`, `coff_summary`, `link`, `execute`,
`parse_project_dump`, `parse_legacy_metadata`, `parse_modern_metadata` y `main`.

## Resultado dirigido

| Medición | Resultado |
|---|---:|
| fuentes frontend | 523 |
| comparaciones tokens / AST / recuperación | 1,046 / 32 / 10 |
| semántica diferencial / estabilidad / invariantes | 48 / 1,048 / 28 |
| fuentes IR / oráculos SSA | 524 / 27 |
| estabilidad corpus IR / estructurales / estabilidad dirigida | 1,572 / 20 / 108 |
| verificadores / invariantes IR | 24 / 118 |
| casos de objeto / proyecto / planes | 8 / 4 / 8 |
| ejecuciones | 40 |
| omitidos / problemas | 0 / 0 |

## Baseline acumulado

Corrida: `selfhost/evidence/runs/tanda-44-sh42-equivalencia-20260920/report.md`.

| Componente | Resultado | Tiempo |
|---|---:|---:|
| construcción e instalación | aprobada | 2.273 s |
| CTest LLD | 477/477 | 1288.451 s |
| fixtures LLD | 148/148 | 50.783 s |
| triage LLD | 18/18 | 8.517 s |
| CTest NSLink | 477/477 | 1230.618 s |
| fixtures NSLink | 148/148 | 46.500 s |
| triage NSLink | 18/18 | 7.769 s |
| Gate 0 | `LANDED` | 9.449 s |
| total | 0 fallos | 2671.441 s |

La instalación quedó en `A:\native-sharp` con build reproducible `4400000000000001`. Los productos,
logs y directorios temporales permanecen ignorados; el commit conserva sólo fuentes, contratos,
documentación y los resúmenes versionados.

## Cierre

- [x] los candidatos proceden de la autocompilación SH41;
- [x] las nueve capas están observadas;
- [x] O0, O1, O2 y O3 están incluidos;
- [x] LLD y NSLink enlazan y ejecutan los productos;
- [x] cero pruebas omitidas y cero problemas abiertos;
- [x] baseline acumulado y Gate 0 verdes.

La siguiente tanda es T45/SH43: punto fijo repetido tres veces desde árboles limpios, con política de
identidad explícita y sin normalización oculta.
