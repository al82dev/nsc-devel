# 148 — E9 Stage 64: inventario criptográfico de la instalación

**Fecha:** 2026-09-07  
**Versión compatible:** `nsc 0.9.0`  
**Versión del lenguaje:** NativeSharp 0.1  
**Etapa del compilador:** 64  
**Estado:** implementación terminada, instalación aislada verificada y matrices locales verdes;
pendiente de promoción mediante G00

## 1. De «están las piezas» a «son exactamente estos bytes»

Stage 63 convirtió la instalación en un producto comprobable. El `nsc.exe` recién instalado ya
podía exigir el ejecutable, los proyectos Core, las especificaciones y un contrato compatible.
Esa verificación contestaba si el esqueleto mínimo estaba presente, pero no podía detectar tres
casos importantes:

1. un archivo de Core existía, pero sus bytes habían cambiado;
2. faltaba un archivo interno que no era el manifiesto `.nsproj` del proyecto;
3. una actualización había dejado un archivo viejo dentro de un árbol propiedad de `nsc`.

Stage 64 añade una segunda capa. La instalación conserva ahora la lista completa que debía copiarse
y un manifiesto con tamaño y SHA-256 de cada archivo. El compilador instalado genera ese manifiesto
y lo verifica inmediatamente antes de que CMake declare éxito.

La distinción entre las capas queda así:

```text
Stage 62  ¿los componentes declaran el mismo contrato de compatibilidad?
Stage 63  ¿existe la superficie mínima del paquete?
Stage 64  ¿están todos los archivos esperados, con exactamente los bytes esperados?
```

Ninguna sustituye a la anterior. Un hash correcto no explica compatibilidad, y un contrato
compatible no demuestra integridad física.

## 2. La decisión central: el origen define qué debe existir

Una implementación ingenua podría recorrer el prefijo después de instalar y generar hashes de todo
lo encontrado. Eso produciría un manifiesto consistente, pero legitimaría residuos de una versión
anterior: el archivo viejo aparecería en la lista precisamente porque ya estaba en el destino.

Stage 64 evita esa circularidad. Durante la configuración, CMake deriva la lista esperada a partir
del payload público seleccionado por el árbol fuente:

- `bin/nsc.exe`;
- todo archivo bajo `lib/`;
- todo archivo bajo `spec/`, instalado como `share/nativesharp/spec/`;
- `NativeSharpToolchain.contract`;
- la propia lista esperada.

Las rutas se normalizan, se ordenan y se escriben antes de instalar. Sólo después de copiar el
payload, el ejecutable instalado abre esa lista y calcula los hashes sobre el prefijo final.

```text
árbol fuente
    |
    | CMake selecciona y ordena rutas
    v
NativeSharpInstallation.files
    |
    | cmake --install copia el payload
    v
<prefix>/bin/nsc.exe --write-installation-manifest
    |
    | tamaño + SHA-256 de cada ruta seleccionada
    v
NativeSharpInstallation.manifest
    |
    | el mismo nsc instalado vuelve a leer y recorrer
    v
--verify-installation-manifest -> éxito o instalación fatal
```

## 3. Los dos formatos canónicos

### 3.1 `NSFILES 1`: alcance esperado

El archivo `NativeSharpInstallation.files` contiene una cabecera versionada y una ruta por línea:

```text
NSFILES 1
bin/nsc.exe
lib/NativeSharp.Core/Allocator.ns
share/nativesharp/spec/NativeSharp_Grammar_1_0.ebnf
share/nativesharp/toolchain/NativeSharpToolchain.contract
```

Sus invariantes son:

- rutas relativas con `/`;
- orden lexicográfico estricto;
- cero duplicados;
- cero rutas absolutas o con `.`/`..`;
- sólo rutas pertenecientes a la instalación administrada;
- formato actual identificado explícitamente como versión 1.

La lista forma parte del manifiesto criptográfico. Así no es posible cambiar en silencio el alcance
esperado y conservar un manifiesto que parezca válido.

### 3.2 `NSINSTALL 1`: bytes instalados

El archivo `NativeSharpInstallation.manifest` usa esta forma:

```text
NSINSTALL 1
<sha256-minúsculo>|<tamaño-decimal>|<ruta-normalizada>
```

Ejemplo conceptual:

```text
NSINSTALL 1
0123...cdef|32768|bin/nsc.exe
abcd...7890|4812|lib/NativeSharp.Core/String.ns
```

Cada entrada autentica tres cosas: identidad de contenido, tamaño y ubicación. El tamaño permite
diagnosticar primero una truncación; el hash detecta una sustitución del mismo tamaño.

El manifiesto se excluye a sí mismo. Incluirlo exigiría conocer su hash antes de terminar de
escribir sus propios bytes. La suite rechaza explícitamente cualquier lista que intente introducir
esa dependencia circular.

## 4. Autoridad SHA-256 compartida

La metadata NSMETA ya tenía una implementación probada de SHA-256 para proteger el payload y
calcular la huella ABI. Stage 64 no introdujo otro algoritmo independiente.

**Archivos nuevos:**

- `include/nativesharp/Basic/Sha256.h`;
- `src/Basic/Sha256.cpp`.

La API compartida publica:

```cpp
Sha256Digest sha256(std::span<const std::uint8_t> input);
bool sha256File(path, digest, size, error);
std::string sha256Hex(const Sha256Digest& digest);
```

**Archivo modificado:** `src/Interop/LibraryMetadata.cpp`

La compresión probada se conserva como autoridad única y ahora se expone por la API `Basic`.
NSMETA llama a la misma función pública que usa el inventario. Las pruebas históricas de metadata
se ejecutaron de nuevo para demostrar que la extracción no cambió sus hashes ni su ABI.

## 5. Escritor y verificador

**Archivo nuevo:** `include/nativesharp/Driver/InstallationManifest.h`

Declara las versiones de ambos formatos y dos operaciones:

```cpp
bool writeInstallationManifest(prefix, expectedFiles, destination, error);
bool verifyInstallationManifest(prefix, manifest, error);
```

**Archivo nuevo:** `src/Driver/InstallationManifest.cpp`

### 5.1 Escritura

`writeInstallationManifest(...)`:

1. valida la cabecera y todas las rutas de `NSFILES 1`;
2. rechaza orden incorrecto, duplicados, traversal y autorreferencia;
3. exige que cada entrada sea un archivo regular;
4. lee y calcula todos los hashes antes de abrir el destino;
5. escribe `NSINSTALL 1` en orden canónico.

El paso 4 es deliberado. Si el último archivo esperado falta o no puede leerse, una versión anterior
válida del manifiesto no se trunca antes de descubrir el problema.

### 5.2 Verificación

`verifyInstallationManifest(...)` valida primero la sintaxis completa. Después, por cada entrada:

1. exige que el archivo exista;
2. compara el tamaño observado;
3. calcula y compara SHA-256;
4. conserva la ruta relativa para detectar archivos no inventariados.

Finalmente recorre los árboles que este paquete instala completos:

- cada subdirectorio Core seleccionado bajo `lib/`;
- `share/nativesharp/spec/`;
- `share/nativesharp/toolchain/`.

Todo archivo regular extra dentro de esos árboles es un residuo y produce error. Los contenedores
`bin/` y `lib/` se tratan de forma distinta: se autentican `bin/nsc.exe`, `.source.txt` y todos los
archivos Core esperados, pero se permiten herramientas y archivos `.lib` de otros paquetes al mismo
nivel. Durante la auditoría se comprobó que el prefijo oficial contiene las herramientas de NSLink
y archivos como `NativeAsmCore.lib` y `NativeLinkCore.lib`. Rechazarlos habría sido confundir
propiedad de paquete con proximidad física. La suite tiene regresiones para ambos contenedores y
sigue rechazando residuos dentro de un subdirectorio Core realmente propiedad de `nsc`.

El parser limita además el manifiesto a 10 000 entradas y cada línea a 16 384 bytes. Un archivo de
entrada hostil no puede forzar crecimiento sin límite ni aceptar formatos ambiguos.

## 6. Superficie pública del compilador

**Archivo modificado:** `src/Driver/Driver.cpp`

Se agregaron dos comandos:

```text
nsc --write-installation-manifest <prefix> <file-list> <manifest>
nsc --verify-installation-manifest <prefix> <manifest>
```

Los dos aparecen en `--help`. Una entrada inválida, un archivo alterado o un residuo usan
`ExitCode::CompilationError`; no se presentan como crash interno. El diagnóstico nombra la primera
ruta y la causa concreta: ausencia, tamaño, SHA-256, formato, orden o archivo inesperado.

## 7. Integración con CMake

**Archivo modificado:** `CMakeLists.txt`

Además de incorporar las fuentes al driver y registrar Stage 64, CMake:

- observa cambios bajo `lib/` y `spec/` con `CONFIGURE_DEPENDS`;
- forma `NSC_INSTALLATION_EXPECTED_PATHS` desde el origen;
- ordena la lista;
- genera `NativeSharpInstallation.files`;
- instala la lista junto al contrato del toolchain.

**Archivo modificado:** `cmake/VerifyInstalledNativeSharp.cmake`

El orden final del install es ahora contractual:

1. ejecutar la verificación mínima de Stage 63;
2. generar el manifiesto con el `nsc.exe` recién instalado;
3. verificar inmediatamente ese manifiesto;
4. convertir cualquier retorno no cero en `FATAL_ERROR`.

Una copia parcial, alterada o contaminada dentro de los árboles propios no puede terminar con el
mensaje de instalación exitosa.

## 8. Contrato normativo y trazabilidad

**Archivos modificados:**

- `spec/NativeSharp_Compiler_Contract_1_0.md`;
- `spec/NativeSharp_Compiler_1_0_conformance.map`.

Se añadieron dos cláusulas:

- `NSC-PKG-004`: producción determinista de la lista y del inventario;
- `NSC-PKG-005`: verificación completa de integridad y archivos inesperados.

El mapa relaciona cada cláusula con CMake, API, implementación, driver y la suite ejecutable. Stage
60 volvió a pasar después de modificar el contrato.

## 9. Suite Stage 64: 55 checks

**Archivo nuevo:** `tests/unit/Stage64InstallationManifestTests.cpp`

La prueba cubre:

- vectores SHA-256 estándar para cadena vacía y `abc`;
- hash de archivo, tamaño y representación hexadecimal;
- generación canónica y determinista en dos destinos;
- inclusión de la lista esperada y exclusión del propio manifiesto;
- paquete fresco válido;
- herramienta independiente coexistiendo en `bin/`;
- archivo estático independiente coexistiendo en `lib/`;
- cambio de bytes con el mismo tamaño;
- cambio de tamaño;
- archivo esperado ausente;
- residuo dentro de `lib/`;
- cabecera futura;
- tamaño decimal no canónico;
- traversal;
- hash con mayúsculas;
- entradas reordenadas;
- lista esperada futura, desordenada o duplicada;
- autorreferencia semántica;
- preservación del manifiesto previo cuando falla un input;
- comandos públicos del driver y sus exit codes;
- integración fatal del script de instalación;
- cláusulas normativas y mapa de conformidad.

Resultado directo:

```text
PASS: 55 checks
```

También se repitieron Stage 48.2 para metadata, Stage 60 para trazabilidad, Stage 62 para el contrato,
Stage 63 para layout y la identidad general del compilador.

## 10. El fallo heredado encontrado durante la matriz

La primera matriz completa terminó 301/302. El único fallo era la fixture de Stage 63, que fabricaba
un contrato incompatible buscando literalmente `stage=63`. Stage 64 hizo visible el acoplamiento:
la búsqueda ya no encontraba el campo, no modificaba nada y después fallaba la expectativa.

**Archivo modificado:** `tests/unit/Stage63InstallationVerifierTests.cpp`

La fixture usa ahora `kCompilerStage`, la misma autoridad que escribe el contrato. Para corromper el
valor construye una etapa incompatible sin depender del número vigente. El test reparado pasó 25/25
en ambos árboles y las matrices completas se repitieron sobre la fotografía final.

No se relajó código productivo para acomodar una prueba vieja.

### 10.1 El prefijo real reveló que `lib` también es compartido

La primera instalación sobre `C:\native-sharp` superó la verificación mínima y escribió el
manifiesto, pero la comprobación completa informó:

```text
nsc: error: unexpected installed file 'lib/NativeAsmCore.lib'
```

La causa era de modelado: el primer cierre trataba todo `lib/` como propiedad exclusiva de `nsc`.
El prefijo real demostró que ahí conviven `NativeAsmCore.lib`, `NativeLinkCore.lib` y bibliotecas
internas del toolchain. La corrección no agregó una lista de excepciones por nombre. El verificador
deriva de `NSFILES 1` cuáles subdirectorios de `lib` fueron seleccionados por el paquete y sólo
busca residuos dentro de esos namespaces. Los archivos propios ubicados directamente en `lib` se
siguen autenticando individualmente; los archivos hermanos pertenecientes a otro paquete pueden
coexistir.

Se añadieron dos checks: uno crea `lib/NativeAsmCore.lib` junto a Core y otro demuestra que la
verificación continúa exitosa. Como prueba física, el install aislado se repitió después de copiar
los `nslink.exe` y `NativeAsmCore.lib` reales desde el prefijo oficial. La instalación preservó
ambos y terminó verificada. Finalmente, el binario corregido verificó de forma sólo lectura el
manifiesto que había quedado en `C:\native-sharp`.

## 11. Instalación aislada real

Prefijo:

```text
A:/nsc-devel/_targeted/e9-stage64-inventory
```

Identidad instalada final:

```text
nsc 0.9.0+build.20260908T022812Z.893a0a91badf7cd2
NativeSharp 0.1, stage 64
```

El flujo de instalación terminó con sus tres confirmaciones:

```text
NativeSharp installation verified: A:/nsc-devel/_targeted/e9-stage64-inventory
installation manifest written: .../NativeSharpInstallation.manifest
installation manifest verified: .../NativeSharpInstallation.manifest
```

Conteo:

```text
expected entries: 76
manifest entries: 76
self entries: 0
```

Huellas del cierre técnico:

```text
nsc.exe                         f87af410a622e519c588019bedd6c170ad2e3c8744ef62d9bcf79659487460e3
NativeSharpInstallation.files d09dbd37423bc156d82a4a5037a2a485aefc69fda0f47a75dbe209f3a4c48b17
NativeSharpInstallation.manifest
                                dc0d272d7545694953af7689bcdceffd79e5fa34c059f7065b02db1517bdf54e
NativeSharpToolchain.contract  77f7ef3962c60bdf8c993550fac3ee347798b8c230491ddfa657e63f7273c16e
```

Estas huellas identifican el cierre local, no sustituyen el hash que produzca G00 al reconstruir.
El build id cambia por construcción; el manifiesto debe corresponder a los bytes de su propio
prefijo.

## 12. Matrices de regresión

Ambas matrices midieron el Stage 64 instalado y usaron directorios de evidencia separados:

| Ruta de enlace | Casos | Fallos | Tiempo |
|---|---:|---:|---:|
| lld-link | 302/302 | 0 | 151.13 s |
| NSLink | 302/302 | 0 | 139.92 s |

La prueba nueva aumenta el total histórico de 301 a 302. El resultado conservó en verde las rutas
de lenguaje, ownership, excepciones, genéricos, Core, ABI, metadata, bibliotecas estáticas, DLL y
hosts nativos.

Después del hallazgo específico de `NativeAsmCore.lib`, se recompilaron la implementación y la
suite en ambos árboles. La fotografía corregida pasó `nsc.stage60.language-contract` y
`nsc.stage64.installation-manifest` 2/2 con cada configuración, además de los 55 checks directos y
la verificación del prefijo oficial descrita arriba. G00 repetirá la matriz completa sobre esa
fotografía al promoverla.

## 13. Archivos creados y modificados

### Nuevos en Stage 64

- `include/nativesharp/Basic/Sha256.h`;
- `src/Basic/Sha256.cpp`;
- `include/nativesharp/Driver/InstallationManifest.h`;
- `src/Driver/InstallationManifest.cpp`;
- `tests/unit/Stage64InstallationManifestTests.cpp`;
- `mkdown/148_1_0_e9_inventario_criptografico_de_instalacion.md`.

### Modificados en Stage 64

- `CMakeLists.txt`;
- `cmake/VerifyInstalledNativeSharp.cmake`;
- `include/nativesharp/Basic/Version.h`;
- `src/Driver/Driver.cpp`;
- `src/Interop/LibraryMetadata.cpp`;
- `tests/unit/Cc1BuildIdentityTests.cpp`;
- `tests/unit/Stage63InstallationVerifierTests.cpp`;
- `spec/NativeSharp_Compiler_Contract_1_0.md`;
- `spec/NativeSharp_Compiler_1_0_conformance.map`;
- `mkdown/121_next_steps_for_the_1_0_glory.md`;
- `mkdown/BUILD.md`;
- `mkdown/CONTENIDO.txt`.

## 14. Qué demuestra y qué sigue

Stage 64 demuestra localmente que la instalación tiene un alcance derivado del origen, una
identidad criptográfica archivo por archivo y un verificador ejecutado por el producto instalado.
Detecta ausencia, modificación y residuos sin apropiarse de herramientas vecinas.

Todavía falta la promoción G00 de esta fotografía. Después del baseline, el siguiente bloque E9
debe comparar dos construcciones independientes por categoría y explicar cuáles artefactos deben
ser idénticos, cuáles contienen identidad efímera y por qué. Esa comparación será la base para
cerrar reproducibilidad y después formalizar actualización/retiro sin residuos.
