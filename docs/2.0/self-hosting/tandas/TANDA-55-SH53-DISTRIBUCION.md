# Tanda 55 — SH53: distribución, rollback y recuperación

## 1. Identidad

| Campo | Valor |
|---|---|
| Estado | `LANDED` |
| Tareas | `NS2-SH-415`–`NS2-SH-422` |
| Gate | `SH53` |
| Rama | `t55-staging` → `development-claude` |
| Commit de entrada | T54/SH52 `f44e6a9b3` |
| Commit de cierre | este commit (`selfhost(tanda-55): certify distribution and recovery`) |
| Fecha de cierre | 2026-09-22 |
| Baseline de cierre | acumulado en T56 por excepción terminal aprobada |

## 2. Objetivo y diseño

Certificar el ciclo de vida completo sin escribir en la instalación activa. CMake instala en
`selfhost-sh53/package`; el propio compilador crea y verifica el inventario. Una segunda instalación
ejecuta la ruta de actualización y sus planes autenticados. El gate conserva una copia conocida,
altera el ejecutable, exige rechazo criptográfico y restaura el árbol anterior. En un directorio
separado mantiene `nsc0.exe` y `nsc2.exe`, consulta ambos por ruta absoluta y exige identidades
distintas.

La recuperación ejecuta `verify_environment.py` sobre `bootstrap.lock` con las rutas explícitas del
build. El hash de `nsc0` se congela en
`b3050c698519e9d7098a4fc74cbde5c8f22546d45eb66267beea08a1f833b472`; clang-cl, LLD, LLVM-C,
NSLink, Python, CMake y Ninja conservan sus hashes bloqueados. El runtime Win32 se resuelve primero
relativo al `nsc.exe` instalado, por lo que copiar el prefijo a otra máquina no conserva una ruta de
build. No se usa PATH para seleccionar el compilador; la prueba aislada no toca `C:\native-sharp`.

## 3. Contratos e invariantes

- Instalación limpia, actualización sin cambios y rollback terminan verificables.
- El manifiesto canónico no cambia durante una actualización sin cambios.
- Los temporales de update se eliminan al finalizar.
- Un candidato alterado no sustituye silenciosamente el árbol conocido.
- `nsc0` y `nsc2` tienen nombres, rutas y hashes explícitos.
- La receta exige directorio nuevo y prohíbe reutilizar objetos fallidos.
- Migración, breaking-change ledger y RC freeze son artefactos ejecutables, no sólo documentación.

## 4. Archivos, tipos y funciones

| Archivo | Acción | Responsabilidad |
|---|---|---|
| `selfhost/bootstrap/bootstrap.lock` | modificar | congelar el seed nsc0 certificado |
| `selfhost/contracts/distribution-final.contract.json` | crear | matriz SH53 y requisitos de recuperación |
| `selfhost/bootstrap/verify_distribution_final.py` | crear | ciclo install/update/rollback/coexistencia |
| `cmake/VerifyNativeSharpDistributionFinal.cmake` | crear | suministrar toolchain bloqueado y aislar salida |
| `CMakeLists.txt` | modificar | registrar gate acumulativo después de SH52 |
| `include/nativesharp/Linker/WindowsRuntimeLibrary.h` | modificar | contrato de resolución relocatable |
| `src/Linker/WindowsRuntimeLibrary.cpp` | modificar | preferir runtime junto al ejecutable y diagnosticar ruta |
| `tests/unit/WindowsRuntimeTests.cpp` | modificar | regresión prefijo instalado/fallback de build |
| roadmap, expediente y resumen SH53 | crear/modificar | cierre normativo |

No se añaden tipos públicos. `digest(path)` fija identidad SHA-256; `run()` impone captura y timeout;
`main()` instala dos veces, prueba corrupción/rollback, verifica convivencia y recuperación, ejecuta
la expresión CTest exacta y publica un JSON aun al fallar. `shutil.copytree` sólo opera dentro de la
raíz temporal creada por el gate.

## 5. Pruebas y resultados

| Dimensión | Resultado |
|---|---|
| instalación vacía | aprobada y verificada |
| actualización in-place | aprobada; cero temporales filtrados |
| manifiesto | estable, SHA-256 `2183fc26c8a0aac292dd8be10659554e70f0d1f75025eb35670c85f963eeda3f` |
| corrupción y rollback | rechazo y restauración aprobados |
| convivencia | nsc0/nsc2 ejecutables por ruta, hashes distintos |
| recovery lock | 8/8 artefactos físicos verificados |
| contratos CTest | 9/9 |

La matriz CTest incluye Stage 63, 64, 66 y 69, compatibilidad, ledger, guía de migración, freeze y
resolución relocatable del runtime Win32.
Reporte dirigido SHA-256 `e1cb29cc61d389e088da59ebf9a45808c6efd8b1426c6332815461321edd859a`;
contrato `c7e6ca22f50bc325df9ab7ae31250a27ebb73db20ea4028aa4c3b23774674d30`;
verificador `1823bc4131d7ec481791ea34e0fd514e0e785a746c8bd588c80eada018c92fc6`.

La instalación operativa en `C:\native-sharp` quedó en build `5500000000000001`, SHA-256
`b14c6f0d8716ecf2a577a111023e363eeeeb02e230fc2913e555c7553d9beef4`, y emitió una DLL O0
desde el runtime relativo sin variable de entorno.

## 6. Baseline, riesgos e higiene

El baseline total queda acumulado para T56. SH53 no probó una actualización desde un paquete 1.x
real; Stage 69 y los artefactos de migración fijan esa frontera, mientras la matriz final valida que
no haya regresiones. La evidencia voluminosa permanece bajo el build y el commit sólo conserva su
resumen y hashes.

- [x] instalación, actualización, convivencia y rollback comprobados;
- [x] nsc0 y toolchain congelados y verificables;
- [x] 9/9 pruebas dirigidas;
- [x] cero binarios, manifiestos generados o temporales en el commit;
- [x] baseline acumulativo reservado a T56;
- [x] commit único de tanda.
