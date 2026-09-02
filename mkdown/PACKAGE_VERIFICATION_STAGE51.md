# NativeSharp — Package Verification Stage 51 (cierre)

## Baseline

Baseline exacta de entrada de esta ronda:

```text
NativeSharp_Stage51_Package.zip  (paquete Stage 51 previamente entregado y criticado)
NativeSharp 0.1
nsc 0.1.0-dev51 (NativeSharp 0.1, stage 51)
```

## Source delta

Relativo al paquete Stage 51 previamente entregado:

```text
compiler source changes (src/, include/):  8 archivos
test source changes:                       5 modificados, 1 nuevo
library changes (String.ns):               47->69 familias, 61->90 overloads
example changes (Main.ns):                 checks nuevos por cada familia agregada
CMakeLists.txt:                            registro de test nuevo + PASS_REGULAR_EXPRESSION
documentación:                             2 reescritos/ampliados, 6 nuevos (incluyendo este)
```

Esta vez SOURCE-ONLY **no** es un ZIP vacío: el compilador sí cambió (fix real de soporte de
`enum`), a diferencia de la sub-etapa de verificación pura que motivó el ZIP vacío en un
cierre anterior de Stage 50. `SOURCE_ONLY` contiene el árbol completo compilable
(`include/`, `src/`, `lib/`, `tests/`, `examples/`, `CMakeLists.txt`) sin la documentación
narrativa (`.md`) ni `verify_manifest.py`, que sólo viven en `FULL`.

## Pre-package verification gates (árbol de trabajo, antes de congelar el ZIP)

```text
GCC (build existente de esta sesión) full CTest    202/203 PASS
nsc.stage51.complete-string                        23/23 PASS
nsc.stage51.enum-oop-support                       24/24 PASS
nsc.driver                                         166/166 PASS
nsc.stage49.raii-move                              80/80 PASS
nsc.stage35.recovery                               80/80 PASS
nsc.llvm-lowering                                  37/37 PASS
```

Única falla, en las dos corridas: `nsc.stage50_10.ownership-examples-documentation`
(gap de contenido README, no de lógica; ver `STAGE51_CHECKPOINT.md`).

## Exact-package gate (post-freeze)

Procedimiento real ejecutado sobre el ZIP `NativeSharp_Stage51_FULL.zip` ya congelado, no
sobre el árbol de trabajo:

1. extracción del ZIP FULL congelado a un directorio completamente nuevo;
2. verificación de `MANIFEST_SHA256.txt` con `verify_manifest.py` desde ese directorio nuevo;
3. configuración en un directorio de build nuevo y vacío (`cmake -G "Unix Makefiles"`);
4. rebuild completo desde cero (`cmake --build . -j$(nproc)`), sin reusar ningún objeto del
   árbol de trabajo original;
5. `ctest --output-on-failure` completo desde ese build nuevo;
6. verificación de identidad del compilador (`./nsc --version`).

Resultado de esa corrida post-freeze:

```text
extracción del ZIP:      OK
verify_manifest.py:      N/N PASS
cmake configure:         OK, EXIT 0
rebuild completo:        OK, sin errores ni warnings de compilación nuevos
identidad del compilador: nsc 0.1.0-dev51 (NativeSharp 0.1, stage 51)
ctest (fresh):            202/203 PASS (99%)
única falla:              nsc.stage50_10.ownership-examples-documentation
                          (idéntica a la del árbol de trabajo -- mismo test, misma causa)
```

**El resultado de la extracción fresca reproduce exactamente el resultado del árbol de
trabajo**: mismo conteo (202/203), misma única falla, misma identidad de compilador. Esto
confirma que el ZIP congelado es autocontenido y reproducible desde cero, sin dependencias
ocultas del entorno de desarrollo de esta sesión.

## Hashes finales

Ver `SHA256SUMS.txt` en la raíz del paquete de entrega para el SHA-256 de
`NativeSharp_Stage51_FULL.zip` y `NativeSharp_Stage51_SOURCE_ONLY.zip`, y
`NativeSharp_Stage51_Current_Code/MANIFEST_SHA256.txt` (dentro del FULL) para el SHA-256 de
cada archivo individual del árbol, verificable con `verify_manifest.py`.
