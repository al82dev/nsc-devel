# Tanda 46 — SH44: determinismo entre máquinas

| Campo | Valor |
|---|---|
| Estado | `LANDED` — matriz reproducible local aprobada; atestación externa opcional |
| Dependencia | SH43 `c99094a45` |
| Gate | SH44 |
| Instalación de trabajo | `A:\native-sharp` |
| LLVM | `C:\clang-llvm` |

## Objetivo

Probar que la autocompilación produce bytes idénticos y que el resultado no depende de la ruta,
espacios, texto no ASCII, locale, zona horaria, concurrencia del harness ni orden de creación de los
archivos. La matriz local completa es el requisito de cierre; una segunda máquina puede aportar una
atestación adicional, pero no bloquea el desarrollo ni la entrega.

## Diseño e implementación

`verify_cross_machine_determinism.py` copia las 103 entradas del compilador a cuatro raíces nuevas.
Cada raíz usa un orden físico distinto y combina ruta, locale, zona horaria y 1–4 workers reales del
harness. En cada variante ejecuta O0–O3; por perfil genera un objeto, enlaza con LLD y NSLink y
verifica versión, self-test y el proyecto completo. Después compara bytes crudos contra la variante
canónica. No existe fase de normalización.

`machine_identity()` genera una huella con host, plataforma, procesador y arquitectura.
`compare_peer()` exige otra huella y compara contrato, manifiesto fuente, herramientas, bibliotecas y
hashes de artefactos. Pasar dos veces el JSON local se rechaza explícitamente.

`run_sh44_machine.cmd` es la entrada configurable para CMD. Recibe raíz fuente, productos SH41,
salida, instalación y LLVM; no contiene una ruta de escritura fija. Sin sexto argumento recopila la
atestación local. Con una atestación externa activa cierre estricto.

## Archivos y funciones

| Archivo | Responsabilidad |
|---|---|
| `selfhost/bootstrap/verify_cross_machine_determinism.py` | staging, matriz, comparación y atestaciones |
| `selfhost/bootstrap/run_sh44_machine.cmd` | ejecución portable desde CMD |
| `selfhost/contracts/cross-machine-determinism.contract.json` | ejes, perfiles, linkers y política de identidad |
| `cmake/VerifyNativeSharpCrossMachineDeterminism.cmake` | candidatos SH41, imports y cierre opcional con peer |
| `CMakeLists.txt` | gate acumulado SH43 → SH44 → promotion-readiness |

Funciones principales: `ordered_files`, `reorder`, `source_manifest`, `stage_sources`, `link`,
`machine_identity`, `verify_image`, `execute_profile`, `compare_peer` y `main`.

## Resultado local

| Medición | Resultado |
|---|---:|
| máquinas verificadas / requeridas | 1 / 1 |
| variantes | 4 |
| fuentes por variante | 103 |
| perfiles / linkers | 4 / 2 |
| comparaciones de objeto | 12 |
| comparaciones de imagen | 24 |
| normalizaciones / omitidos | 0 / 0 |
| estado | `passed` |

SHA-256 del reporte dirigido final, ya sin materialización de globs:
`5d550f20828242396b8d8338695cf3eb5393b0e10cfb794b9fa7e505697b31f6`.
SHA-256 de la atestación local:
`45119e4211a5bcb683903b6c1c1c46267eef82218d56b47f44c2a18b9984553d`.

La prueba negativa con la propia atestación terminó con `peer attestation belongs to the same
machine`, como exige el contrato.

## Corrección UTF-8 descubierta por el gate

La primera matriz encontró una brecha real: `GetCommandLineW` reducía los argumentos con `CP_ACP` y
`File.Exists`/`File.ReadAllText` enviaban bytes UTF-8 a `GetFileAttributesA`/`CreateFileA`.
`buildEnvironmentGetCommandLineArgs()` usa ahora `CP_UTF8`; el nuevo
`buildUtf16NullTerminatedCopy()` convierte rutas mediante `MultiByteToWideChar(CP_UTF8)` y las dos
operaciones de lectura usan `GetFileAttributesW`/`CreateFileW`. Las tablas canónicas y los imports
de los gates incluyen los tres símbolos nuevos.

La sonda `ruta-ñ-日本` aprobó `--read-source`, `--check-project` y la emisión O0. Su objeto conservó
el SHA-256 canónico `8c4d695e00ff8edfcaad07752a238bbd729987ed317ff57a6f0d65467fe4b27f`.
La matriz completa repitió después O0–O3 y ambos linkers sin divergencias.

La primera corrección todavía dejaba la enumeración completa en `FindFirstFileA`/`FindNextFileA` y
materializaba los globs únicamente en la variante Unicode. Ese atajo se retiró antes del cierre:
`buildDirectorySearchPattern()` convierte UTF-8 a UTF-16, la enumeración usa
`FindFirstFileW`/`FindNextFileW`, reserva los 592 bytes reales de `WIN32_FIND_DATAW`, filtra `.` y
`..` como `wchar_t` y `buildWideCStringToNativeSharpString()` convierte los nombres encontrados de
vuelta a UTF-8. Las cuatro variantes usan ahora sus `Compile Include="**/*.ns"` originales; el
reporte final no contiene un campo de materialización.

Pruebas dirigidas adicionales: SH41 regenerado y aprobado en 371.50 s; seis pruebas afectadas
(`stage53-3`, `stage53-3b`, `stage53-5`, `stage53-6`, `windows-runtime`, `windows-linker`) aprobadas
6/6 en 12.79 s. La matriz final aprobó en 393.72 s.

## Aislamiento de los gates históricos

El primer baseline de cierre, corrida `20260921-034142-601eff1a`, descubrió una contaminación real
del harness. SH05 enumeraba recursivamente toda la raíz del checkout y trató como fuente normativa
una copia generada por SH44 bajo `build-test-*/selfhost-sh44/staged/ruta-ñ-日本`. Por esa causa
fallaron únicamente `nsc.selfhost.frontend` y su consumidor
`nsc.selfhost.functional-equivalence`; las otras 477 pruebas CTest, las 148 fixtures y los 18 casos
de triage aprobaron con cada linker. La corrida se conserva como evidencia negativa y no se acepta
como baseline de cierre.

Los contratos SH05, SH06 y SH07 declaran ahora las mismas seis raíces canónicas:
`book_examples`, `examples`, `lib`, `selfhost/nsc`, `selfhost/tests` y `tests`. Sus enumeradores
rechazan implícitamente cualquier árbol de build, sin depender de su nombre, ubicación, limpieza ni
estado Git. `test_selfhost_analysis_corpora_ignore_generated_build_trees` construye deliberadamente
un árbol `build-any-name/selfhost-sh44/staged/ruta-ñ-日本` y demuestra que sus fuentes no entran en
ninguno de los tres corpus.

La regresión de infraestructura aprobó 45/45. SH05 aprobó con LLD y NSLink en 42.76 s, y SH42
completo aprobó con ambos linkers en 152.63 s. Esta corrección elimina 1,276 copias generadas de la
medición observada y reduce el gate sin disminuir el corpus fuente contractual, que contiene 523
unidades en este corte.

La segunda corrida completa, `20260921-051611-ab93c0cc`, aprobó la matriz técnica con ambos
linkers: 479/479 CTest, 148/148 fixtures y 18/18 casos de triage por ruta. Sin embargo, Gate 0 la
rechazó por `S0 != S1`: durante la medición se actualizaron este expediente y el README de
contratos, por lo que el hash del árbol cambió de `0609c4223d52a5aa` a `ae527cec31a5e9fb`. La regla
es correcta y la corrida no se usa como baseline de cierre. El intento final se ejecuta con el árbol
congelado y sin escrituras de desarrollo concurrentes.

La corrida congelada `20260921-062601-17e90a1d` cerró correctamente Gate 0 en estado `LANDED`, sin
problemas: 479/479 CTest, 148/148 fixtures y 18/18 casos de triage con LLD, y los mismos conteos con
NSLink. `S0` y el estado posterior conservaron exactamente
`7eb1d76575e4e8986ee0e02a4586a1a32eb44bff1a83c101d74adb61d428ab53` sobre 1,576 archivos. La
corrida duró 4,140.02 s. El reporte tiene SHA-256
`ba9a33dc768ca32e549bec91ab7ce45cc6044b3f48dba93264a03da1f220a700`; el registro Gate 0,
`309de8a743ed0d11bbacaf37f62f9c74b95735b24a90623f95aa94a83813a739`.

## Cierre

SH44 queda cerrado por decisión de proyecto con la matriz local completa: cuatro variantes reales,
O0–O3, LLD y NSLink, sin normalización ni divergencias. La atestación de otra PC permanece como
comprobación opcional y no condiciona el avance. El paquete portable validado localmente es
`selfhost/out/sh44-peer-package-wide-20260921.zip`, SHA-256
`68a81b461a8f680df9d22016a5a8c847b4899bf1e7343925bc5b76204ee48d38`; permanece ignorado y no
forma parte del commit.

El paquete se ejecutó desde su propio árbol aislado y reprodujo exactamente `source_sha256`,
`contract_sha256`, `input_sha256` y `artifact_sha256`. Su reporte local tiene SHA-256
`5a293a30c6e2bb17e004f94efd991c1b190d3f03431c4cae83019140a84cce3d`.
