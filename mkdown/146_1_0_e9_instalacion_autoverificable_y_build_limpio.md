# 146 — E9 Stage 63: instalación autoverificable y construcción limpia

**Fecha:** 2026-09-07  
**Versión compatible:** `nsc 0.9.0`  
**Versión del lenguaje:** NativeSharp 0.1  
**Etapa del compilador:** 63  
**Estado:** instalado, `LANDED` y certificado mediante G00

**Promoción:** G00 `20260907-190718-3fc07605`, 301/301 CTest por linker, 324/324 casos
del runner, 0 fallos. El acta exacta está en `147_baseline_certificado_1_0_e9_stage63.md`.

## 1. Punto de partida

Stage 62 dio al toolchain un contrato determinista. El propio compilador podía imprimirlo,
escribirlo, instalarlo y rechazar diferencias de versión, ABI, metadata, target o linker. Ese
contrato resolvió la pregunta «¿son compatibles estos componentes?», pero todavía no resolvía una
pregunta anterior y más física:

> ¿El prefijo instalado contiene realmente todas las piezas mínimas que el producto promete?

CMake podía terminar de copiar sin que el ejecutable instalado inspeccionara el resultado como un
producto completo. La existencia de `bin/nsc.exe` no demostraba por sí sola que estuvieran los
proyectos Core, el contrato del toolchain y los documentos normativos que describen lo que ese
binario implementa.

Stage 63 cierra esa separación. La instalación ya no termina cuando CMake agotó su lista de copias;
termina cuando el `nsc.exe` recién instalado acepta el prefijo que lo contiene.

## 2. El contrato nuevo

Se añadió la cláusula normativa `NSC-PKG-003 — Installed layout verification` al contrato del
compilador. Define tres obligaciones:

1. El paquete Windows soportado contiene el ejecutable, contrato, los manifiestos de cinco
   proyectos Core y cinco documentos normativos.
2. `nsc --verify-installation <prefix>` comprueba esa superficie y el contenido del contrato.
3. El flujo de instalación ejecuta el comando desde el binario recién copiado y falla si el
   resultado es incompleto o incompatible.

El mapa de conformidad conecta la cláusula con implementación y prueba ejecutable. Así, la regla no
vive únicamente en un comentario de CMake o en este capítulo.

## 3. Arquitectura implementada

El flujo completo queda así:

```text
build/bin/nsc.exe
        |
        | cmake --install
        v
<prefix>/bin/nsc.exe + Core + spec + toolchain contract
        |
        | ejecuta el nsc instalado
        v
nsc --verify-installation <prefix>
        |
        +-- valida el layout mínimo
        +-- delega el contrato a verifyToolchainContract
        +-- éxito: CMake confirma la instalación
        `-- error: CMake aborta con el componente exacto
```

La decisión importante es que el script no usa el `nsc.exe` del árbol de build. Usa
`<prefix>/bin/nsc.exe`. Esto prueba que el archivo realmente instalado arranca, entiende el comando
y puede verificar el paquete desde su ubicación final.

## 4. Implementación por archivo y función

### 4.1 Nueva API de verificación

**Archivo nuevo:** `include/nativesharp/Driver/InstallationVerifier.h`

Publica:

```cpp
bool verifyInstallation(const std::filesystem::path& prefix, std::string& error);
```

La función devuelve `true` sólo para un layout completo. En error, entrega un texto estable y útil
para el driver y para CMake.

**Archivo nuevo:** `src/Driver/InstallationVerifier.cpp`

Contiene tres piezas:

- `RequiredFile`: asocia una ruta relativa con el nombre humano de su componente;
- `requireRegularFile(...)`: exige un archivo regular y construye el diagnóstico con ruta exacta;
- `verifyInstallation(...)`: valida prefijo, ejecutable, contrato, Core y especificaciones en orden.

Las listas mínimas se expresan como arreglos constantes. Los cinco manifiestos exigidos pertenecen
a estos proyectos distribuidos:

- `NativeSharp.Core`;
- `NativeSharp.Core.Collections.Generic`;
- `NativeSharp.Core.Diagnostics`;
- `NativeSharp.Core.IO`;
- `NativeSharp.Core.Threading`.

Los cinco documentos obligatorios son la especificación del lenguaje, gramática EBNF, mapa de
conformidad del lenguaje, contrato del compilador y mapa de conformidad del compilador.

El verificador no vuelve a implementar el parser de `NativeSharpToolchain.contract`. Delega a
`verifyToolchainContract(...)`, la autoridad creada en Stage 62. Esto evita dos definiciones de
compatibilidad que podrían divergir.

### 4.2 Comando público del driver

**Archivo modificado:** `src/Driver/Driver.cpp`

`Driver::run(...)` reconoce:

```text
nsc --verify-installation <prefix>
```

En éxito escribe una sola confirmación con el prefijo validado. En error conserva la clasificación
controlada `ExitCode::CompilationError` y antepone `nsc: error:` al diagnóstico. La ayuda pública
incluye la orden nueva.

### 4.3 Verificación al instalar

**Archivo nuevo:** `cmake/VerifyInstalledNativeSharp.cmake`

El script:

1. forma la ruta `${CMAKE_INSTALL_PREFIX}/bin/nsc.exe`;
2. falla inmediatamente si no existe;
3. ejecuta `--verify-installation` con el prefijo efectivo, incluido uno dado mediante
   `cmake --install --prefix`;
4. captura salida, error y código de retorno;
5. convierte cualquier retorno no cero en `FATAL_ERROR`.

**Archivo modificado:** `CMakeLists.txt`

Registra el script después de todas las reglas que instalan compilador, Core, especificaciones y
contrato. La posición es contractual: ejecutarlo antes sólo demostraría una instalación todavía
incompleta. También incorpora `InstallationVerifier.cpp` a `nsc_driver` y registra la suite de
Stage 63.

### 4.4 Versión y contrato normativo

**Archivos modificados:**

- `include/nativesharp/Basic/Version.h`: etapa 62 → 63;
- `tests/unit/Cc1BuildIdentityTests.cpp`: expectativa de identidad actualizada;
- `spec/NativeSharp_Compiler_Contract_1_0.md`: cláusula `NSC-PKG-003`;
- `spec/NativeSharp_Compiler_1_0_conformance.map`: vínculo cláusula–código–evidencia.

### 4.5 Fixture de Stage 62 endurecida

**Archivo modificado:** `tests/unit/Stage62ToolchainContractTests.cpp`

Al subir a Stage 63 apareció una falla útil en un caso heredado. La prueba de orden canónico todavía
construía su mutación buscando el literal `stage=62`. Como ya no lo encontraba, terminaba
verificando el contrato sin modificar y la aserción fallaba.

Se reemplazó el literal por `kCompilerStage`. La prueba ahora deriva la etapa de la misma autoridad
que genera el contrato y seguirá probando exclusivamente el orden al avanzar Stage 64 o posteriores.
No se relajó el verificador para hacer pasar la prueba.

## 5. Prueba nueva: 25 checks

**Archivo nuevo:** `tests/unit/Stage63InstallationVerifierTests.cpp`

La suite construye un paquete sintético aislado y cubre:

- prefijo vacío;
- prefijo inexistente;
- layout mínimo completo;
- error limpio después de un éxito;
- ausencia de `bin/nsc.exe`;
- contrato perteneciente a otra etapa;
- ausencia exacta de Collections.Generic;
- ausencia exacta del contrato normativo del compilador;
- restauración de cada componente y retorno al estado válido;
- comando público exitoso;
- comando público fallido y exit code controlado;
- presencia y orden del script CMake;
- instalación fatal cuando falla el comando;
- cláusula normativa y entrada del mapa de conformidad.

Resultado en ambos árboles:

```text
lld-link: PASS: 25 checks
NSLink:   PASS: 25 checks
```

## 6. Instalación aislada real

Después de la auditoría final se construyó e instaló el árbol exacto de Stage 63 en:

```text
A:/nsc-devel/_targeted/e9-stage63-final
```

La última línea de `cmake --install` fue:

```text
NativeSharp installation verified: A:/nsc-devel/_targeted/e9-stage63-final
```

Identidad instalada:

```text
nsc 0.9.0+build.20260908T010255Z.f1ee061296d6ea6c
NativeSharp 0.1, stage 63
LLVM 23.1.0 (supported: 23.x)
```

Huellas SHA-256:

```text
nsc.exe:                           2043EAFECF9956DC767FD254130D26FA1C7B52FDBE1C29715E5E6D4626EA8BFE
NativeSharpToolchain.contract:     631FE550BA132985F59CC6CDBB29C2EDBC4CBF143242C8F9ABD1FC6509792B00
```

El hash del contrato cambia respecto de Stage 62 porque `stage` es un campo de compatibilidad. El
build id y timestamp siguen ausentes del contrato, por lo que reconstruir el mismo Stage 63 no debe
cambiar sus bytes.

## 7. Matrices completas

Antes de CTest, ambos árboles se reconfiguraron para medir explícitamente:

```text
A:/nsc-devel/_targeted/e9-stage63-install-verify/bin/nsc.exe
```

Esto evita que una matriz verde corresponda al Stage 62 instalado anteriormente.

Resultados secuenciales:

| Árbol | Casos | Fallos | Tiempo |
|---|---:|---:|---:|
| `build-test-lldlinker` | 301/301 | 0 | 833.49 s |
| `build-test-nslink` | 301/301 | 0 | 835.17 s |

El total aumenta en uno por `nsc.stage63.installation-verifier`. Las matrices conservaron en verde
las suites de ejecución, diferencial O0/O2, errores negativos, ownership, excepciones, genéricos,
interop, `.lib`, DLL, metadata y ABI.

La auditoría posterior precisó que Stage 63 comprueba los **manifiestos** Core y que Stage 64 será
quien inventaríe cada archivo. Ese ajuste cambió únicamente el nombre humano de esos componentes y
la redacción normativa. Se recompilaron y repitieron `nsc.stage60.language-contract` y
`nsc.stage63.installation-verifier` en ambos árboles, 2/2 y 2/2 verdes, antes de producir la
instalación final registrada en la sección anterior. G00 será la matriz completa del árbol final.

## 8. Guía de construcción corregida

**Archivo reemplazado:** `mkdown/BUILD.md`

La guía anterior mezclaba rutas históricas y afirmaciones obsoletas sobre plataforma y versiones
LLVM. Stage 63 la sustituye por una receta única y comprobable:

- Windows x64 con `clang-cl`, `lld-link`, Ninja y LLVM-C 23.x;
- carpetas nuevas para build e instalación;
- `NSC_TEST_COMPILER` apuntando de forma explícita al binario medido;
- distinción entre build, instalación y baseline;
- instalación que se verifica sola;
- diagnóstico de los fallos frecuentes;
- distinción honesta entre construir el compilador en configuración Debug y emitir símbolos para
  un programa NativeSharp.

La guía no declara cerrada la depuración de fuente. Explica que `-O0`/`-O2` ya están certificados,
pero que información de fichero/línea y PDB pertenecen a una entrega posterior.

## 9. Qué demuestra Stage 63

Stage 63 demuestra que:

- existe una definición ejecutable del paquete mínimo;
- el ejecutable instalado verifica su propio prefijo;
- la compatibilidad del contrato sigue teniendo una sola autoridad;
- una omisión física produce un diagnóstico preciso y falla la instalación;
- el procedimiento soportado de construcción limpia está escrito con las versiones vigentes;
- el cambio conserva las dos matrices completas en verde.

No demuestra todavía que cada byte instalado esté inventariado criptográficamente. Tampoco prueba
que dos compilaciones independientes produzcan un `nsc.exe` idéntico: la identidad por build hace
que esa pregunta requiera separar artefactos compatibles de artefactos reproducibles.

## 10. Archivos creados y modificados

### Nuevos

- `include/nativesharp/Driver/InstallationVerifier.h`
- `src/Driver/InstallationVerifier.cpp`
- `cmake/VerifyInstalledNativeSharp.cmake`
- `tests/unit/Stage63InstallationVerifierTests.cpp`
- `mkdown/146_1_0_e9_instalacion_autoverificable_y_build_limpio.md`

### Modificados

- `CMakeLists.txt`
- `include/nativesharp/Basic/Version.h`
- `src/Driver/Driver.cpp`
- `tests/unit/Cc1BuildIdentityTests.cpp`
- `tests/unit/Stage62ToolchainContractTests.cpp`
- `spec/NativeSharp_Compiler_Contract_1_0.md`
- `spec/NativeSharp_Compiler_1_0_conformance.map`
- `mkdown/BUILD.md`
- `mkdown/121_next_steps_for_the_1_0_glory.md`
- `mkdown/CONTENIDO.txt`

## 11. Qué sigue dentro de E9

En orden:

1. Promover Stage 63 con G00 y registrar su baseline sin mover el árbol durante la corrida.
2. Generar un inventario determinista de todos los archivos instalados con tamaños y SHA-256, no
   sólo verificar la superficie mínima.
3. Verificar el inventario desde `nsc` y usarlo para detectar archivos faltantes, alterados o
   inesperados.
4. Ejecutar dos construcciones independientes y comparar por categoría: contrato, bibliotecas Core,
   especificaciones, objetos y ejecutable.
5. Definir perfiles NativeSharp Debug/Release explícitos y emitir información de fichero/línea y
   PDB comprobable por herramientas nativas.
6. Registrar el inventario necesario para actualizar o retirar una instalación sin dejar residuos.

Stage 63 ya fue certificado. El siguiente paso inmediato es hacer que el paquete completo tenga una
identidad criptográfica verificable archivo por archivo.
