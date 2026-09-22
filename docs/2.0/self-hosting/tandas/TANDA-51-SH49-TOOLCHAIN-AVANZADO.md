# Tanda 51 — SH49: toolchain avanzado

| Campo | Valor |
|---|---|
| Estado | `LANDED` — gate dirigido y baseline acumulado aprobados |
| Base | T50/SH48 `85659b643` |
| Gate | SH49 |
| Instalación | `A:\native-sharp` |

## Objetivo

Cerrar el toolchain avanzado de NativeSharp 2.0 alrededor del compilador autoalojado: comptime
aislado, caché reproducible, sanitizer nativo, SDK declarativo, metadata N−1/N, paquetes y bindings
precompilados. El cierre combina artefactos emitidos por `nsc2` con la matriz contractual heredada;
no atribuye esas superficies únicamente al compilador semilla.

## Diseño e implementación

`ProjectModel` conserva ahora `PackageReference` y `NativeReference` como dependencias distintas de
fuentes y proyectos. El dump canónico y la clave de caché incluyen ambas listas. El manifiesto
autoalojado publica SDK, compatibilidad metadata, modo comptime, sanitizer y cantidades de paquetes
y bindings, y su repetición debe ser idéntica.

El gate usa el `nsc2-o0-lld.exe` recién producido por SH41 para emitir dos manifiestos, generar
NSMETA/2, aceptar NSMETA/1 y rechazar una versión futura desconocida. Después ejecuta 14 pruebas
contractuales de SDK, comptime sandboxed, caché, reproducibilidad, sanitizer, metadata y
compatibilidad. Así separa claramente la lectura/emisión autoalojada de las implementaciones
avanzadas ya congeladas por el toolchain 2.0.

## Archivos y funciones

| Archivo | Responsabilidad |
|---|---|
| `selfhost/nsc/Projects/ProjectSystem.ns` | paquetes, bindings, dump, caché y manifiesto |
| `selfhost/tests/toolchain-2.0/AdvancedToolchain.nsproj` | fixture de SDK/paquete/binding |
| `selfhost/contracts/advanced-toolchain.contract.json` | inventario obligatorio SH49 |
| `selfhost/bootstrap/verify_advanced_toolchain.py` | manifiesto, metadata y matriz CTest |
| `cmake/VerifyNativeSharpAdvancedToolchain.cmake` | integración reproducible del gate |

Métodos principales: `AddPackageReference`, `AddNativeReference`, `DependencyAttributes`,
`ProjectDumper.Dump`, `ProjectCache.Key`, `ToolchainManifest.Create` y
`MetadataContract.ReadCompatible`.

## Evidencia dirigida

- SH41 `nsc1 → nsc2 → nsc3` aprobado en 1017.21 s;
- 14/14 pruebas avanzadas aprobadas en 5.51 s;
- manifiestos autoalojados idénticos;
- NSMETA/2 y NSMETA/1 aceptados; NSMETA/3 rechazado;
- SDK, un paquete y un binding precompilado presentes en el manifiesto y la clave.

SHA-256 del contrato: `c24ee165a3c2fec1fa1fc7c36eee646cbf3ba5fcebcb3015b056a9ed44c144a1`.
SHA-256 del verificador: `d7a2c3a8d228fb4cef7885291b6159efe50d7916b897944e6ac6ad919be10a52`.
SHA-256 del reporte dirigido: `b1031ebb2204a3d99c0405cbbc709c801467eef8c5bd3b3b33297ec13cf01574`.

## Baseline acumulado

Corrida `tanda-51-sh49-advanced-toolchain-20260922`, identidad `5100000000000001`,
aprobada en 7916.541 s:

- 484/484 CTests con LLD en 3919.169 s;
- 148/148 fixtures y 18/18 triaje con LLD;
- 484/484 CTests con NSLink en 3848.655 s;
- 148/148 fixtures y 18/18 triaje con NSLink;
- cero fallos y Gate 0 `LANDED`.

SHA-256 del reporte: `6e715dbc2467fda52ec72e0e45422b5290312748dfdbf22ed27f51d2e0bcddd1`.

## Criterios de cierre

- [x] comptime sandboxed y caché reproducible certificados;
- [x] sanitizer nativo certificado;
- [x] SDK declarativo conservado por el modelo autoalojado;
- [x] NSMETA N−1/N validado;
- [x] paquetes y bindings precompilados modelados y fingerprinted;
- [x] baseline dual-linker y Gate 0 aprobados;
- [x] commit único de T51.
