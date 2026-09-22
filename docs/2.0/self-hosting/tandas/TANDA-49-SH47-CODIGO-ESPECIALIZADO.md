# Tanda 49 — SH47: código especializado y NSAsm

| Campo | Valor |
|---|---|
| Estado | `LANDED` — gate dirigido y baseline acumulado aprobados |
| Base | T48/SH46 `e7a8d586e` sobre la cadena lineal T46→T48 |
| Gate | SH47 |
| Instalación | `A:\native-sharp` |
| LLVM | `C:\clang-llvm` (sólo lectura) |
| NSAsm | `A:\native-sharp\bin\nsasm.exe` |

## Objetivo

Cerrar el código especializado exigido por NativeSharp 2.0: vectores, matrices, `Half`, formatos
empacados, instrucciones de hardware, ensamblador inline, modos de punto flotante y objetos NSAsm.
La prueba debe compilar y ejecutar O0–O3 con LLD y NSLink y auditar el código máquina, no sólo la
aceptación sintáctica.

## Diseño e implementación

`LLVMGetInlineAsm` se incorporó al binding LLVM C auditado. El backend transforma cada
`InlineAssemblyStatement` en una llamada LLVM de dialecto Intel con efectos laterales, construye la
plantilla y las restricciones a partir de salidas, entradas y clobbers, y admite cero o una salida.
Los operandos NativeSharp `$outN` y `$N` se traducen a los índices físicos de LLVM.

Los locales conservan además su nombre fuente. Esta búsqueda nominal es deliberada para el límite
de ensamblador inline: el analizador semántico todavía no asigna identificador de símbolo a cada
expresión operando de esa forma, pero el lowering debe resolverla sin omitir validación de tipo.

La fixture integrada prueba `Vector4.Dot`, determinante de matriz, bits de `Half`, color empacado,
una suma inline con salida y una mutación de memoria inline. El objeto NSAsm ejecuta `stmxcsr`,
`paddd`, `roundss`, `pause`, `rdtsc` y `ldmxcsr`; otra función no ejecutada congela la superficie
AVX/FMA/packed mediante `vaddps`, `vfmadd213ps`, `packssdw` y `vzeroupper`.

El primer baseline integrado reveló un defecto acumulado de SH44: una referencia con `..` medía
261 caracteres antes de normalizarse. `Path.GetFullPath` cruzaba todavía `GetFullPathNameA`, y
`SourceGraphBuilder` calculaba la forma normalizada pero consultaba y cargaba la ruta original.
El runtime usa ahora `GetFullPathNameW`, convierte UTF-8→UTF-16→UTF-8 y el grafo usa la ruta
normalizada para existencia, carga y directorio propietario. Las tres pruebas de runtime/rutas/
linker pasaron 3/3; SH41 volvió a pasar y SH44 aprobó sus cuatro variantes en 460.13 s.

## Archivos, métodos y funciones

| Archivo | Responsabilidad |
|---|---|
| `lib/NativeSharp.LLVM.C/LLVMApi.ns` | binding auditado de `LLVMGetInlineAsm` |
| `selfhost/nsc/CodeGen/LlvmBackend.ns` | resolución de locales y lowering de ensamblador inline |
| `selfhost/contracts/specialized-code.contract.json` | matriz, superficies e instrucciones obligatorias |
| `selfhost/bootstrap/verify_specialized_code.py` | ensamblado, emisión, auditoría, enlace y ejecución |
| `cmake/VerifyNativeSharpSpecializedCode.cmake` | gate acumulado reproducible SH47 |
| `selfhost/tests/specialized-2.0/Main.ns` | tipos especializados y ensamblador inline |
| `selfhost/tests/specialized-2.0/specialized.asm` | superficie ejecutada y auditable de NSAsm |
| `src/Runtime/WindowsRuntimeBuilder.cpp` | normalización UTF-16 sin frontera ANSI `MAX_PATH` |
| `selfhost/nsc/Projects/SourceGraph.ns` | uso efectivo de la ruta normalizada en todo I/O de proyecto |

Métodos principales: `LocalBinding.Name`, `FindLocalByName`, `FindLocalTypeByName`,
`InlineAssemblyTemplate`, `InlineAssemblyConstraints`, `EmitInlineAssembly` y `EmitStatement`.

## Evidencia dirigida

- 51/51 pruebas de infraestructura;
- 8/8 ejecuciones especializadas: O0–O3 por LLD y NSLink;
- desensamblado nativo con `addl` para salida y memoria;
- desensamblado NSAsm con las diez familias contractuales;
- SH41 real aprobado: `nsc1 → nsc2 → nsc3`, 16 ejecuciones, O0–O3 y ambos linkers;
- objetos `nsc2`/`nsc3` idénticos por perfil y cero invocaciones de `nsc0` durante generación.

SHA-256 de fuentes SH41: `ae83937f7827e5ee98ec690a237101a656841b63eaf0f23f2d379bf1f3fe37fd`.
SHA-256 del contrato: `ecd874b071d5d950a0e274ae0678f71d28d09232c221c1e945189fa027913096`.
SHA-256 del verificador: `2daad6b533242201b14c0e8729871aa478d099c7eb3953d049753edab2ea914e`.
SHA-256 del reporte dirigido: `ad0631f93182b05b36e078dbfa3ee1079d9a7fd499ab334a992e0ea61a608091`.
SHA-256 del reporte SH41: `00f0a67bc53e6a1ed7e5d8b404b5f4fd1fc09b470d33c854c8e3cd778efb399a`.
SHA-256 del reporte SH44: `cba07165086e1a1b3e3f135b3a0b930a0301b01743a8b26f4d6845c7fb2489d1`.

## Baseline acumulado

Corrida congelada `tanda-49-sh47-specialized-code-r2-20260921`, identidad
`4900000000000002`, aprobada en 4638.981 s y sin fallos:

- 482/482 CTests con LLD en 2261.300 s;
- 148/148 fixtures y 18/18 casos de triaje con LLD;
- 482/482 CTests con NSLink en 2228.859 s;
- 148/148 fixtures y 18/18 casos de triaje con NSLink;
- instalación verificada exclusivamente en `A:\native-sharp`;
- Gate 0 certificado en estado `LANDED`.

SHA-256 del reporte del baseline:
`c72b382c8376525e402aed4dff9d18057d4d8f93dc3d56c002a6db3c72e0d9a0`.

## Criterios de cierre

- [x] vectores, matrices, `Half` y formatos empacados ejecutados;
- [x] ensamblador inline emitido por LLVM C y visible en código máquina;
- [x] NSAsm instalado desde `A:\nsasm-devel` sin mezclar sus fuentes con NSC;
- [x] instrucciones SIMD, AVX/FMA, packed y control de punto flotante auditadas;
- [x] O0–O3 con LLD y NSLink;
- [x] punto fijo SH41 posterior a los cambios;
- [x] baseline completo acumulado y Gate 0;
- [x] commit único de T49.
