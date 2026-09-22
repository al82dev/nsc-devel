# Tanda 56 — SH54: certificación final de NativeSharp 2.0 autoalojado

## 1. Identidad

| Campo | Valor |
|---|---|
| Estado | `LANDED` |
| Gate | `SH54` |
| Rama | `t56-staging` → `development-claude` → `development` |
| Commit de entrada | T55/SH53 `2856ba24b` |
| Commit de cierre | este commit (`selfhost(tanda-56): certify NativeSharp 2.0 self-hosting`) |
| Fecha de cierre | 2026-09-22 |
| Baseline de cierre | `tanda-56-sh54-final-certification-r2-20260922` |
| Build promovido | `5600000000000001` |

## 2. Objetivo y diseño

SH54 cierra el programa de self-hosting con una precertificación ejecutable y un único baseline
acumulativo desde cero. El gate exige que SH50–SH53 estén limpios, comprueba las identidades del
compilador anfitrión y del autoalojado, materializa los tres artefactos canónicos de promoción,
verifica el acta y ejecuta los contratos finales de compatibilidad, baseline y promoción.

`nscbuilder.py` reconstruye el compilador, instala en el prefijo aislado `A:\native-sharp`, crea los
dos árboles CTest y ejecuta la misma matriz con `lld-link` y NSLink. Después ejecuta fixtures y
triage en O0/O2 y registra Gate 0. Los niveles O0–O3, LTO y ThinLTO quedan cubiertos por la matriz
acumulada y por SH50. La instalación operativa se actualiza aparte en `C:\native-sharp` y se
verifica por versión, manifiesto, hash y reproducción funcional.

## 3. Corrección descubierta por el primer intento

La primera corrida `tanda-56-sh54-final-certification-20260922` quedó invalidada en
`nsc.stage46.native-libraries`. El productor escribía correctamente plantillas como
`Box<T> : ITaggedValue<T>`, pero la verificación de relectura intentaba obtener nombres conocidos
mediante `sourceTypeForMetadataKey()`. Esa función rechaza deliberadamente identidades abiertas
con aridad `` `1 `` porque no representan un tipo fuente materializable. El efecto lateral era que
el parser tampoco conocía los nombres léxicos `IValue`, `ITaggedValue` y `Box` al reconstruir el
buffer de plantillas.

`appendParserKnownTypeNames()` separa ambas responsabilidades: normaliza una identidad metadata,
elimina sólo la aridad, conserva namespace/nesting, añade el nombre terminal y, para tipos
externos, la forma calificada por módulo. `verifyPrecompiledPublicSource()` y
`verifyPrecompiledGenericTemplateSource()` siembran además todos los templates genéricos de tipo;
los métodos genéricos se excluyen porque no son nombres de tipo. La prohibición de materializar un
tipo genérico abierto permanece intacta.

El primer intento se conserva como evidencia fallida y fue sustituido por `r2`; no cuenta como
certificación. La prueba exacta pasó en el baseline definitivo con ambos linkers.

## 4. Archivos, métodos y contratos

| Archivo | Acción | Responsabilidad |
|---|---|---|
| `src/Driver/Driver.cpp` | modificar | nombres genéricos conocidos durante la relectura NSMETA |
| `selfhost/bootstrap/bootstrap.lock` | modificar | congelar el nuevo `nsc0` después de la corrección |
| `selfhost/contracts/final-certification.contract.json` | crear | identidad, prerrequisitos y artefactos SH54 |
| `selfhost/bootstrap/verify_final_certification.py` | crear | precertificación y reporte JSON determinista |
| `cmake/VerifyNativeSharpFinalCertification.cmake` | crear | puente CMake del gate final |
| `CMakeLists.txt` | modificar | registrar SH54 después de SH53 |
| `_abi/gates/G00/gate.json` | actualizar | acta mecánica del baseline acumulativo |
| resumen, roadmap y expediente | crear/modificar | cierre humano y hashes verificables |

Funciones relevantes:

- `appendParserKnownTypeNames()` deriva nombres léxicos sin fingir una especialización cerrada;
- `verifyPrecompiledPublicSource()` verifica que la API reconstruida sea fuente NativeSharp válida;
- `verifyPrecompiledGenericTemplateSource()` hace lo mismo para la superficie reusable;
- `digest()` fija SHA-256 de artefactos y reportes;
- `run()` captura salida, código y timeout sin UI;
- `main()` valida prerrequisitos, identidades, acta, expedientes y contratos finales.

## 5. Resultado del baseline acumulativo

| Dimensión | lld-link | NSLink |
|---|---:|---:|
| CTest | 489/489 | 489/489 |
| fixtures O0/O2 | 148/148 | 148/148 |
| bug triage O0/O2 | 18/18 | 18/18 |
| fallos | 0 | 0 |

- tiempo total: `8467.879 s`;
- Gate 0: `certified: true`, `state: LANDED`, `problems: []`;
- reporte: SHA-256 `370ccb60de801ebe32c655ee4cf3ecef830c66b21b3cb82899ea4a51ed7e9cae`;
- metadata: SHA-256 `b47d1e984ea0d8ec454543ed6ea49c546afaa90514f3a370f27b11baf9dacd60`;
- Gate 0: SHA-256 `67f90974e39798f732e0762e793c0c5ead04db2badc4d27306844afd94bca198`;
- contrato SH54: SHA-256 `1219b4f7b4d704666299a333ce80a0cc75d33826f5688d6022da5f26a8d46b1e`;
- verificador: SHA-256 `ad64dd7c7c58fc7120747a6b2ef1d59ec7bc43a14ad2497d2269ee860d20d76e`;
- build promovido: SHA-256 `7c8a5d2178a8bd79b85b00d4514229320e50a723894d11ca5485c893a57faf06`.

La precertificación produjo los mismos hashes de compatibilidad, baseline y promoción bajo ambos
linkers. Los compiladores autoalojados tienen hashes físicos distintos porque su enlace pertenece a
toolchains diferentes, pero ambos satisfacen la misma identidad y comportamiento 2.0.

## 6. Cierre, riesgos e higiene

SH54 certifica el alcance definido por el roadmap de NativeSharp 2.0. No afirma que LLVM deje de ser
una dependencia ni que NSLink/NSAsm se reescriban dentro de este repositorio. `nsc0` permanece como
semilla congelada y ruta de recuperación; la evolución posterior corresponde al roadmap 3.0.

La evidencia voluminosa permanece bajo `_nscbuilder/history/`; el commit conserva únicamente el
acta Gate 0, el resumen con hashes, contratos, código fuente y documentación. No se versionan
objetos, ejecutables, logs ni directorios temporales.

- [x] defecto del primer intento corregido y prueba exacta repetida;
- [x] SH50–SH54 ejecutados con ambos linkers;
- [x] 489/489 CTests por linker;
- [x] 148/148 fixtures y 18/18 triage por linker;
- [x] cero omitidos y cero incidencias de release;
- [x] Gate 0 `LANDED`;
- [x] instalación operativa verificada;
- [x] expediente, hashes, commit y tag de cierre preparados.
