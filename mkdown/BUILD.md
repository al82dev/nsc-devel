# Construcción, prueba e instalación de NativeSharp

**Vigencia:** Stage 66, camino oficial Windows x64.

Esta guía describe el producto que el árbol soporta hoy. Su objetivo no es enumerar todas las
combinaciones que CMake podría aceptar, sino dar una receta repetible para construir `nsc`, medir
el ejecutable correcto e instalar un paquete que se verifica a sí mismo.

## 1. Qué se está construyendo

El árbol produce tres familias de artefactos:

- `build/bin/nsc.exe`: el compilador NativeSharp;
- `build/lib/*.lib`: bibliotecas internas C++ usadas para construir el compilador;
- `build/tests/*.exe`: ejecutables de prueba cuando `NSC_BUILD_TESTS=ON`.

La instalación pública es distinta del árbol de construcción. Contiene:

- `<prefix>/bin/nsc.exe`;
- `<prefix>/lib/NativeSharp.Core*`;
- `<prefix>/share/nativesharp/spec/*`;
- `<prefix>/share/nativesharp/toolchain/NativeSharpToolchain.contract`;
- `<prefix>/share/nativesharp/toolchain/NativeSharpCompatibility.policy`;
- `<prefix>/share/nativesharp/toolchain/NativeSharpInstallation.files`;
- `<prefix>/share/nativesharp/toolchain/NativeSharpInstallation.manifest`.

Las `.lib` internas de C++ no son parte de la superficie pública instalada. Las `.lib` y DLL que
produce `nsc` para proyectos NativeSharp sí son artefactos del lenguaje y se validan mediante sus
contratos NSMETA y ABI.

## 2. Plataforma y herramientas calificadas

La ruta oficial de 1.0 es:

- Windows x64;
- CMake 3.25 o posterior;
- Ninja;
- compilador C++20 `clang-cl`;
- enlazador `lld-link` para construir `nsc`;
- LLVM-C 23.x cargado desde una biblioteca monolítica;
- `llvm-lib` disponible junto al resto de herramientas LLVM;
- PowerShell para los comandos de esta guía.

El compilador comprueba en ejecución que la biblioteca LLVM cargada pertenece a la línea 23.x.
Una configuración que encuentra otra versión no constituye un toolchain certificado.

La instalación usada en este árbol coloca las herramientas en `C:/clang-llvm`:

```text
C:/clang-llvm/bin/clang-cl.exe
C:/clang-llvm/bin/lld-link.exe
C:/clang-llvm/bin/llvm-lib.exe
C:/clang-llvm/lib/LLVM-C.lib
```

NativeSharp mantiene las declaraciones de la API C que utiliza; por eso este camino enlaza
directamente `LLVM-C.lib` y no necesita consumir la API C++ de LLVM.

## 3. Construcción limpia de Release

Desde la raíz del checkout, elija dos carpetas nuevas. El ejemplo usa rutas dentro del propio
árbol para que la instalación sea aislada y removible:

```powershell
$sourceRoot = (Get-Location).Path
$buildRoot = Join-Path $sourceRoot '_clean/build-release'
$installRoot = Join-Path $sourceRoot '_clean/install-release'

cmake -S $sourceRoot -B $buildRoot -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_CXX_COMPILER=C:/clang-llvm/bin/clang-cl.exe `
  -DCMAKE_LINKER=C:/clang-llvm/bin/lld-link.exe `
  -DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld `
  -DCMAKE_DISABLE_FIND_PACKAGE_LLVM=TRUE `
  -DNSC_LLVM_MONOLITHIC_LIBRARY=C:/clang-llvm/lib/LLVM-C.lib `
  -DNSC_BUILD_TESTS=ON `
  -DNSC_WARNINGS_AS_ERRORS=ON `
  -DNSC_TEST_COMPILER="$buildRoot/bin/nsc.exe"

cmake --build $buildRoot --parallel
```

Durante la primera configuración, CMake puede avisar que `NSC_TEST_COMPILER` todavía no existe.
Es normal en una carpeta limpia: la construcción siguiente crea exactamente ese ejecutable. No
debe cambiarse silenciosamente por otro `nsc.exe` ya instalado.

Compruebe la identidad del resultado:

```powershell
& "$buildRoot/bin/nsc.exe" --version
& "$buildRoot/bin/nsc.exe" --print-llvm-version
& "$buildRoot/bin/nsc.exe" --print-toolchain-contract
& "$buildRoot/bin/nsc.exe" --print-compatibility-policy
```

La identidad visible del binario incluye `+build.<UTC>.<id>` y cambia en cada construcción real.
El contrato de compatibilidad omite esa identidad efímera: dos builds compatibles deben producir
los mismos bytes en `NativeSharpToolchain.contract`.

## 4. Pruebas del compilador recién construido

```powershell
ctest --test-dir $buildRoot --output-on-failure --no-tests=error
```

La línea de configuración `NSC_TEST_COMPILER="$buildRoot/bin/nsc.exe"` es parte de la prueba. Sin
ella, una máquina con otra instalación podría medir un compilador diferente del recién construido.

Para repetir sólo los contratos de empaquetado de E9:

```powershell
ctest --test-dir $buildRoot --output-on-failure `
  -R 'nsc\.stage6(2\.toolchain-contract|3\.installation-verifier|4\.installation-manifest|6\.e9-e10-release-contract)'
```

El número total de casos cambia cuando se agrega una suite. Lo contractual es `100% tests passed`
y cero fallos, no un total histórico copiado en esta guía.

## 5. Instalación autoverificable

```powershell
cmake --install $buildRoot --prefix $installRoot
```

Stage 66 ejecuta al final:

```powershell
& "$installRoot/bin/nsc.exe" --verify-installation $installRoot
& "$installRoot/bin/nsc.exe" --verify-installation-manifest $installRoot `
  "$installRoot/share/nativesharp/toolchain/NativeSharpInstallation.manifest"
```

No son comprobaciones decorativas. La primera valida:

1. el prefijo y `bin/nsc.exe`;
2. el contrato instalado y cada uno de sus campos;
3. los manifiestos de los cinco proyectos Core distribuidos;
4. la especificación, gramática y mapas normativos del lenguaje y compilador.

Después, el instalador genera `NativeSharpInstallation.manifest` desde la lista esperada producida
por el árbol fuente. La segunda orden comprueba tamaño y SHA-256 de cada archivo, además de detectar
faltantes y residuos dentro de cada directorio Core seleccionado, `share/nativesharp/spec` y
`share/nativesharp/toolchain`. `bin` y `lib` son contenedores compartidos con otras herramientas:
el inventario autentica sus archivos propios sin atribuirse ejecutables o archivos `.lib` vecinos.

Si falta un componente o el contrato pertenece a otro compilador, `cmake --install` termina con
error. Una copia parcial ya no puede presentarse como instalación exitosa.

Puede repetir la comprobación cuando sospeche que el paquete fue alterado:

```powershell
& "$installRoot/bin/nsc.exe" --verify-installation $installRoot
& "$installRoot/bin/nsc.exe" --verify-toolchain-contract `
  "$installRoot/share/nativesharp/toolchain/NativeSharpToolchain.contract"
& "$installRoot/bin/nsc.exe" --verify-compatibility-policy `
  "$installRoot/share/nativesharp/toolchain/NativeSharpCompatibility.policy"
& "$installRoot/bin/nsc.exe" --verify-installation-manifest $installRoot `
  "$installRoot/share/nativesharp/toolchain/NativeSharpInstallation.manifest"
```

## 6. Separar build, instalación y baseline

Son tres evidencias diferentes:

```text
fuentes -> build/bin/nsc.exe -> instalación/bin/nsc.exe -> baseline G00
```

- La construcción demuestra que las fuentes producen un compilador.
- La instalación demuestra que el paquete mínimo quedó completo y compatible.
- G00 demuestra además que el árbol no cambió durante la corrida, que el ejecutable construido es
  idéntico al instalado y que la matriz certificada terminó sin fallos.

Una ejecución local verde no sustituye la promoción G00. Tampoco conviene configurar una matriz
para que mida una instalación vieja: revise siempre la línea `ctest mide el compilador` emitida por
CMake.

## 7. Construir sólo el compilador

Para una iteración rápida sin generar las suites:

```powershell
cmake -S $sourceRoot -B "$sourceRoot/_clean/build-nsc-only" -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_CXX_COMPILER=C:/clang-llvm/bin/clang-cl.exe `
  -DCMAKE_LINKER=C:/clang-llvm/bin/lld-link.exe `
  -DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld `
  -DCMAKE_DISABLE_FIND_PACKAGE_LLVM=TRUE `
  -DNSC_LLVM_MONOLITHIC_LIBRARY=C:/clang-llvm/lib/LLVM-C.lib `
  -DNSC_BUILD_TESTS=OFF `
  -DNSC_WARNINGS_AS_ERRORS=ON

cmake --build "$sourceRoot/_clean/build-nsc-only" --target nsc --parallel
```

Esto acelera una iteración, pero no es evidencia de cierre: antes de entregar hay que volver a una
carpeta con pruebas activadas, ejecutar la matriz y crear el baseline correspondiente.

## 8. Debug y Release: qué significan hoy

`CMAKE_BUILD_TYPE=Debug` o `Release` controla cómo se construye el ejecutable C++ `nsc.exe`.
No implica por sí mismo que un programa NativeSharp emitido contenga información de depuración.

Para programas NativeSharp, `-O0` y `-O2` ya seleccionan los niveles de optimización certificados y
deben conservar la misma semántica observable. La emisión nativa de información de fichero/línea y
PDB es una entrega separada todavía pendiente; esta guía no presenta los símbolos del compilador
host como si fueran símbolos del programa compilado.

## 9. Diagnóstico de fallos frecuentes

### CMake mide otro compilador

Síntoma: la línea `ctest mide el compilador` apunta a una instalación anterior.

Corrección: reconfigure la carpeta con la ruta absoluta del binario que desea probar:

```powershell
cmake -S $sourceRoot -B $buildRoot `
  -DNSC_TEST_COMPILER="$buildRoot/bin/nsc.exe"
```

### La versión LLVM es rechazada

Compruebe la biblioteca realmente cargada:

```powershell
& "$buildRoot/bin/nsc.exe" --print-llvm-version
```

Stage 64 exige la línea LLVM 23.x. Cambiar sólo el nombre del archivo no cambia la versión que el
runtime reporta.

### La instalación falla al final

Lea el primer componente nombrado por `--verify-installation`. El diagnóstico incluye la ruta
esperada y distingue ejecutable, contrato, proyecto Core o documento normativo. Corrija la regla
de instalación o el artefacto productor; no copie manualmente el archivo para ocultar un paquete
incompleto.

### Una prueba falla después de recompilar

Conserve la salida completa del caso, la identidad de `nsc --version`, la ruta mostrada por
`NSC_TEST_COMPILER` y el linker medido. Esos cuatro datos permiten saber si el fallo pertenece a
las fuentes actuales, al producto instalado o a una matriz configurada contra otro binario.

## 10. Lista de comprobación para una máquina limpia

- checkout en una ruta nueva;
- carpeta de build inexistente antes de configurar;
- herramientas LLVM 23.x disponibles en rutas explícitas;
- warnings tratados como errores;
- `NSC_TEST_COMPILER` apuntando al binario del build que se mide;
- construcción completa sin reutilizar objetos anteriores;
- CTest al 100%;
- prefijo de instalación nuevo;
- mensaje final `NativeSharp installation verified`;
- identidad, contrato y hashes conservados con la evidencia;
- G00 ejecutado después, sin modificar el árbol durante la corrida.

Cumplir esta lista prueba una construcción e instalación limpias y deja cada archivo público
inventariado. Para Stage 66, la reproducibilidad entre dos construcciones independientes se mide
con el gate siguiente.

## 11. Gate de reproducibilidad de Stage 66

Use cuatro rutas nuevas o vacías. El gate no permite reciclar un build porque su propósito es
detectar dependencias accidentales del cache o del directorio de salida:

```powershell
cmake `
  -DNSC_SOURCE="$sourceRoot" `
  -DNSC_BUILD_A="$sourceRoot/_repro/build-a" `
  -DNSC_BUILD_B="$sourceRoot/_repro/build-b" `
  -DNSC_PREFIX_A="$sourceRoot/_repro/prefix-a" `
  -DNSC_PREFIX_B="$sourceRoot/_repro/prefix-b" `
  -DNSC_REPORT="$sourceRoot/_repro/reproducibility.nsreport" `
  -DNSC_FORCE_MONOLITHIC_LLVM=ON `
  -DNSC_LLVM_LIBRARY=C:/clang-llvm/lib/LLVM-C.lib `
  -DNSC_CXX_COMPILER=C:/clang-llvm/bin/clang-cl.exe `
  -P cmake/VerifyReproducibleNativeSharpPackage.cmake
```

El resultado válido contiene `result=reproducible`, `differences=0` y
`strict_compiler_binary=identical`. La identidad fija usada internamente pertenece sólo a esta
comparación. No configure `NSC_BUILD_ID_OVERRIDE` en una distribución ordinaria: los builds
normales deben conservar su identidad única.

## 12. Actualización y retiro

Una reinstalación normal mediante `cmake --install` detecta un manifiesto anterior, escribe el
nuevo, calcula `NSUPDATE 1`, retira únicamente archivos que sólo pertenecían a la versión anterior
y verifica el árbol final. Si un archivo retirado fue modificado desde que se generó el manifiesto,
la instalación se detiene y conserva ese archivo.

Para inspeccionar un cambio sin aplicarlo:

```powershell
& "$installRoot/bin/nsc.exe" --write-installation-update-plan `
  previous.manifest next.manifest update.plan
Get-Content update.plan
```

Para retirar una instalación se necesita ejecutar un `nsc.exe` externo al prefijo que se va a
retirar; un proceso Windows no debe intentar borrar su propio ejecutable:

```powershell
& "C:/ruta-separada/nsc.exe" --write-installation-removal-plan `
  "$installRoot/share/nativesharp/toolchain/NativeSharpInstallation.manifest" `
  retirement.plan
& "C:/ruta-separada/nsc.exe" --apply-installation-removal-plan `
  $installRoot `
  "$installRoot/share/nativesharp/toolchain/NativeSharpInstallation.manifest" `
  none `
  retirement.plan
```

El comando no elimina directorios compartidos ni archivos que no figuren en el manifiesto.

## 13. Consultar compatibilidad

```powershell
& "$installRoot/bin/nsc.exe" --print-compatibility-policy
& "$installRoot/bin/nsc.exe" --check-compiler-compatibility 0.9.0
```

Antes de 1.0, un requisito sólo se satisface dentro del mismo major y minor y con patch igual o
mayor. La política instalada publica además los rangos NSMETA, Native ABI, Runtime ABI y versiones
de todos los formatos del paquete. El capítulo 150 explica la razón y la evidencia de cada campo.
