# 144 — 1.0-E9: contrato reproducible del toolchain

**Fecha:** 2026-09-07  
**Entrega:** primera megaentrega E9  
**Compilador:** `0.9.0`, Stage 62  
**Estado:** implementada, promovida y certificada por G00

## Qué problema cierra

Hasta Stage 61 era posible preguntar al ejecutable su versión, build id, target, linker y LLVM.
También existían versiones formales para NSMETA, el ABI nativo, el runtime y sus subcontratos. El
problema era que esas respuestas estaban repartidas entre comandos, headers y documentos.

Eso dejaba tres huecos propios de una distribución:

1. Un paquete no llevaba una sola declaración legible por herramientas que dijera con qué era
   compatible.
2. CMake podía instalar `nsc.exe`, Core y la especificación, pero no instalaba un contrato producido
   por ese mismo ejecutable.
3. Un consumidor podía leer una nota de versión, pero no pedir al compilador que comprobara cada
   identidad y explicara exactamente cuál era incompatible.

Stage 62 introduce `NativeSharpToolchain.contract`: un manifiesto pequeño, determinista,
versionado, escrito por el propio `nsc` y comprobable por el propio `nsc`.

No sustituye al build id. Ambos resuelven preguntas distintas:

| Identidad | Pregunta que responde | ¿Es reproducible entre builds compatibles? |
|---|---|---|
| build id | «¿qué ejecutable exacto produjo esto?» | no, cambia en cada construcción |
| toolchain contract | «¿qué contratos acepta y produce este compilador?» | sí |

Meter el build id o una fecha dentro del manifiesto habría hecho diferentes dos contratos
compatibles. Por eso el manifiesto conserva sólo datos que cambian la compatibilidad o la
superficie certificada.

## El formato `NSTOOLCHAIN 1`

El artefacto generado por Stage 62 contiene exactamente:

```text
NSTOOLCHAIN 1
compiler_release=0.9.0
language=0.1
stage=62
target=x86_64-pc-windows-msvc
llvm=23.x
nsmeta=1.8
native_abi=1.0
runtime_abi=1.0
mangling_abi=1
object_abi=1
delegate_abi=1
exception_abi=1
managed_delegate_cell_size=32
supported_linkers=lld-link,NSLink
certified_optimizations=O0,O2
artifact_kinds=exe,static-lib,dll
```

La forma canónica exige cabecera versionada, claves conocidas y únicas, orden fijo, valores
exactos, LF entre líneas y un único salto final. No contiene timestamp ni build id.

El lector tolera CRLF como diferencia de transporte en Windows, pero el escritor siempre produce
los mismos bytes canónicos. Así un fichero copiado por una herramienta que transforme finales de
línea todavía puede verificarse, mientras dos artefactos generados directamente siguen teniendo
el mismo SHA-256.

## Una sola fuente de verdad

`currentToolchainContract()` no repite números propios. Compone el documento leyendo las
autoridades que ya gobiernan al compilador:

| Campo | Autoridad existente |
|---|---|
| `compiler_release`, `language`, `stage` | `Basic/Version.h` |
| `target` | `TargetInfo::WindowsX64()` |
| `llvm` | `kSupportedLlvmMajor` |
| `nsmeta` | `kMetadataFormatMajor/Minor` |
| ABI nativo y runtime | `kNativeAbi*`, `kRuntimeAbi*` |
| subcontratos físicos | constantes de mangling, objeto, delegate y excepción |
| celda de delegate | `kManagedDelegateCellSize` |

Esto evita que CMake, un script de paquete y el compilador mantengan tres copias que puedan
divergir. Cuando una autoridad cambia, el siguiente enlace de `nsc` produce automáticamente el
nuevo contrato.

## Flujo de construcción e instalación

```text
headers con versiones reales
        |
        v
se enlaza nsc.exe Stage 62
        |
        v
nsc --write-toolchain-contract
        |
        v
build/generated/NativeSharpToolchain.contract
        |
        v
cmake --install
        |
        v
<prefix>/share/nativesharp/toolchain/NativeSharpToolchain.contract
```

El paso posterior al enlace ejecuta `$<TARGET_FILE:nsc>`. No existe una plantilla CMake con
versiones copiadas a mano: el ejecutable que se entrega escribe lo que él mismo considera
compatible.

## Superficie pública nueva

### `--print-toolchain-contract`

Imprime únicamente los bytes canónicos. Esto permite capturar o comparar el contrato sin limpiar
texto decorativo.

### `--write-toolchain-contract <ruta>`

Escribe el contrato y crea directorios padre faltantes. CMake usa exactamente esta operación para
producir el artefacto instalable.

### `--verify-toolchain-contract <ruta>`

Compara el fichero contra todas las identidades compiladas en el `nsc` que ejecuta la orden. En
éxito devuelve 0. Un fichero ausente, malformado o incompatible devuelve la clase de error de
compilación (1); no se disfraza como error interno.

El diagnóstico distingue versión de formato no soportada, línea malformada, campo desconocido,
repetido o ausente, valor incompatible, orden no canónico y fallos de lectura o escritura.

## Métodos y responsabilidades

### `currentToolchainContract()`

Construye el formato en un orden estable. Es la única función que decide los bytes correctos; el
comando de impresión, el escritor, el post-build y las pruebas consumen el mismo resultado.

### `writeToolchainContract()`

Valida el destino, crea su directorio padre, abre en modo binario y escribe el resultado canónico.
Cada fallo devuelve una causa para que el driver la prefije uniformemente con `nsc: error:`.

### `verifyToolchainContract()`

Lee la cabecera, analiza cada línea, comprueba el conjunto de claves, compara los valores contra
las constantes vigentes y finalmente verifica el orden. No se reduce a comparar un hash opaco:
informa el contrato concreto que dejó de coincidir.

### `Driver::run()`

Expone las tres operaciones sin introducir otro ejecutable auxiliar. La aridad continúa bajo el
mismo parser de comandos y los errores de artefacto conservan la taxonomía de E8.

## Contrato normativo y trazabilidad

Se agregaron dos cláusulas a `NativeSharp_Compiler_Contract_1_0.md`:

- `NSC-PKG-001`: obliga a generar e instalar el contrato determinista desde las identidades del
  ejecutable;
- `NSC-PKG-002`: define las tres órdenes, el rechazo estricto y la clase de salida.

Ambas tienen filas en `NativeSharp_Compiler_1_0_conformance.map` con caminos de implementación y
evidencia ejecutable. La suite de E8 ya no exige que el mapa tenga exactamente ocho filas; exige
que sus ocho cláusulas históricas sigan presentes. Así E9 puede ampliar el contrato sin romper una
prueba cuyo propósito era impedir la pérdida de cobertura de E8.

## Pruebas agregadas

`Stage62ToolchainContractTests.cpp` ejecuta 43 checks. Cubre:

- cabecera, orden, salto final y los dieciséis campos;
- exclusión explícita de build id;
- escritura byte por byte y creación de directorios;
- auto-verificación del contrato vigente y lectura CRLF;
- cabecera futura, clave desconocida, duplicada o faltante;
- valor incompatible, campos reordenados y línea malformada;
- los tres comandos del driver;
- ruta ausente y exit code controlado;
- contrato generado después de enlazar `nsc`;
- regla de instalación, cláusulas normativas y mapa de conformidad.

Las selecciones focales pasaron en los dos árboles:

```text
lld-link: Stage 61 + Stage 62 + build identity  3/3 PASS
NSLink:   Stage 61 + Stage 62 + build identity  3/3 PASS

Stage 62 directo, lld-link  PASS: 43 checks
Stage 62 directo, NSLink    PASS: 43 checks
```

Después se reconstruyó el objetivo completo de pruebas en ambos árboles, se apuntó
`NSC_TEST_COMPILER` al Stage 62 instalado y se ejecutó la batería completa con el proveedor de
enlace correspondiente:

```text
lld-link  300/300 PASS  160.85 s
NSLink    300/300 PASS  142.32 s
```

Esto incluye las rutas de EXE, biblioteca estática, DLL, hosts externos, O0/O2, ownership,
excepciones, genéricos entre proyectos y las suites normativas E7–E9. No quedó evidencia retenida
por fallo porque las dos corridas terminaron completamente verdes.

## Instalación aislada comprobada

Se instaló el producto en `A:/nsc-devel/_targeted/e9-stage62-contract`. La identidad instalada
fue:

```text
nsc 0.9.0+build.20260907T233630Z.d780279fbd649615
NativeSharp 0.1, stage 62
```

El propio ejecutable instalado aceptó el contrato bajo `share/nativesharp/toolchain`. El contrato
de build y el instalado dieron el mismo SHA-256:

```text
23009776C7B35C42D0515E5E588D0CD4C80E25C1E463FCC4FF9BF52612128D18
```

Esto demuestra que la instalación no reescribe ni sustituye el manifiesto producido por el
compilador.

## Archivos nuevos

| Archivo | Función |
|---|---|
| `include/nativesharp/Driver/ToolchainContract.h` | API pública y versión del formato |
| `src/Driver/ToolchainContract.cpp` | composición, escritura y verificación estricta |
| `tests/unit/Stage62ToolchainContractTests.cpp` | 43 checks positivos, negativos y de paquete |
| `mkdown/144_1_0_e9_contrato_reproducible_del_toolchain.md` | historia técnica y método de esta entrega |

## Archivos modificados

| Archivo | Cambio |
|---|---|
| `CMakeLists.txt` | Stage 62, módulo, generación post-build, instalación y suite E9 |
| `include/nativesharp/Basic/Version.h` | Stage 61 → 62 |
| `src/Driver/Driver.cpp` | comandos de imprimir, escribir y verificar |
| `tests/unit/Cc1BuildIdentityTests.cpp` | identidad esperada Stage 62 |
| `tests/unit/Stage61CompilerHardeningTests.cpp` | umbral histórico extensible del mapa |
| `spec/NativeSharp_Compiler_Contract_1_0.md` | cláusulas `NSC-PKG-001/002` |
| `spec/NativeSharp_Compiler_1_0_conformance.map` | trazabilidad nueva |
| `mkdown/121_next_steps_for_the_1_0_glory.md` | estado real de E9 |
| `mkdown/CONTENIDO.txt` | incorporación a la lectura histórica |

## Qué no afirma todavía esta entrega

Stage 62 cierra el contrato de composición y compatibilidad del paquete. Todavía no certifica una
construcción completa desde máquina limpia, depuración de punta a punta, la matriz completa
Debug/Release, inventario de desinstalación/actualización, igualdad byte por byte de dos
ejecutables independientes ni G00 sobre el Stage 62 instalado oficialmente.

Esos puntos siguen dentro de E9. No se maquillan como cerrados porque el manifiesto ya sea
determinista.

## Estado de cierre

La primera megaentrega E9 quedó implementada, verde y certificada. G00
`20260907-175537-29408b6e` promovió Stage 62 con 12/12 etapas, 324/324 casos, cero fallos, árbol
inmóvil y ejecutable construido idéntico al instalado. El acta completa está en el documento 145.
E9 continúa con la construcción limpia y la depuración nativa, usando el contrato de esta entrega
como identidad verificable del paquete probado.
