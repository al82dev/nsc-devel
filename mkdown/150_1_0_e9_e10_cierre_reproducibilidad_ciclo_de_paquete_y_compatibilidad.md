# 150 — Megaentrega 1.0 E9+E10: reproducibilidad, ciclo de paquete y compatibilidad

**Fecha:** 2026-09-07  
**Compilador:** `nsc 0.9.0`  
**Lenguaje:** NativeSharp 0.1  
**Fotografía:** Stage 66  
**Estado:** E9 y E10 implementadas y certificadas por G00 `20260907-233625-801b209e`

## 1. Qué cierra esta entrega

Esta entrega no agrega otra característica aislada al frontend. Cierra la parte que convierte un
compilador funcional en un producto nativo que puede construirse, instalarse, actualizarse y
versionarse sin depender de memoria humana.

E9 y E10 se entregan juntas porque sus contratos se necesitan mutuamente:

- E9 demuestra qué bytes forman una distribución, si dos construcciones son reproducibles y qué
  puede retirar una actualización;
- E10 publica qué versiones y ABI entiende esa distribución y vuelve ejecutables las decisiones
  de compatibilidad;
- el manifiesto de E9 autentica el artefacto de política de E10;
- la política de E10 congela las versiones de los formatos de E9.

Stage 65 queda absorbido por Stage 66. No existe una fotografía intermedia que afirme tener
reproducibilidad sin tener también actualización segura y política de compatibilidad.

## 2. El problema real después de Stage 64

Stage 64 ya podía responder: «¿estos 76 archivos siguen siendo exactamente los que instalé?». No
podía responder cuatro preguntas distintas:

1. ¿Dos builds limpios de la misma fuente producen la misma distribución?
2. ¿Qué diferencia es deliberada y cuál es una regresión de reproducibilidad?
3. ¿Qué archivo viejo puede retirarse al instalar una versión nueva?
4. ¿Qué significa que un consumidor requiera una versión determinada del compilador?

Usar sólo una lista de archivos no alcanza. Enumerar el destino después de copiar tampoco alcanza:
eso convertiría cualquier residuo antiguo en parte legítima del paquete nuevo. Borrar directorios
completos sería peor, porque `bin` y `lib` son raíces compartidas con otras herramientas.

La solución implementada conserva tres autoridades separadas:

- `NSFILES 1`: lo que la versión nueva espera distribuir;
- `NSINSTALL 1`: los bytes que una instalación concreta posee;
- `NSUPDATE 1`: la diferencia autenticada entre la propiedad anterior y la siguiente.

E10 agrega una cuarta:

- `NSCOMPAT 1`: las reglas y versiones que el producto promete comprender.

## 3. E9-A — comparación de construcciones independientes

### 3.1 Nuevo comparador

`compareInstallationManifests` lee dos manifiestos `NSINSTALL 1` estrictos y compara por ruta:

- presencia en ambos conjuntos;
- tamaño decimal canónico;
- SHA-256 en minúsculas;
- orden y unicidad de las rutas;
- clasificación explícita de identidad de construcción.

El resultado es un reporte versionado `NSREPRO 1`. Una diferencia determinista produce exit code
de compilación y nombra la ruta exacta; no se reduce a «los builds son distintos».

El comando público es:

```text
nsc --compare-installation-manifests <left> <right>
```

### 3.2 La identidad de build no se usa como excusa

Un build ordinario sigue recibiendo su identidad única
`0.9.0+build.<UTC>.<16-hex>`. Por eso `bin/nsc.exe` se marca como portador de identidad variable en
la comparación general.

Eso, por sí solo, habría dejado un agujero: eximir el ejecutable completo también ocultaría una
diferencia accidental en su código. El gate independiente usa una identidad reservada y fija sólo
para sus dos builds, y exige después que el SHA-256 completo de ambos `nsc.exe` coincida.

El primer intento estricto encontró una diferencia auténtica: PE/COFF conservaba el instante del
enlace en `TimeDateStamp`. La solución fue añadir `/Brepro` al enlace MSVC-compatible. No se
normalizan binarios después de producirlos; el linker genera directamente el PE reproducible.

Los builds ordinarios continúan siendo distinguibles por su build id. `/Brepro` elimina entropía
accidental, no la identidad explícita del producto.

### 3.3 Gate de dos árboles limpios

`cmake/VerifyReproducibleNativeSharpPackage.cmake` exige cuatro directorios nuevos o vacíos:

- build A;
- build B;
- prefijo instalado A;
- prefijo instalado B.

Configura, compila e instala ambos árboles desde la misma fuente con warnings como errores. Cada
instalación se autoverifica y genera su manifiesto. El gate compara los manifiestos y finalmente
compara los dos ejecutables completos con la identidad reservada.

La evidencia final fue:

```text
NSREPRO 1
result=reproducible
deterministic_entries=76
volatile_entries=1
differences=0
volatile=build-identity|bin/nsc.exe
strict_compiler_binary=identical
compiler_sha256=cbddd8a1ae70712a876c3f249a66a0d673bc7b57ccf6f9fd2f29ea427d4501b8
```

La distribución contiene ahora 77 entradas: 76 se comparan directamente como artefactos
deterministas y la entrada del compilador se vuelve a construir con identidad fija para exigir
igualdad completa. Por tanto, ninguna entrada termina sin prueba de contenido.

El reporte quedó conservado en:

```text
A:/nsc-devel/_targeted/e9e10-final/reproducibility.nsreport
```

### 3.4 Descubrimiento de configuración limpia de LLVM

El primer build realmente independiente descubrió que el `LLVMConfig.cmake` local anunciaba
dependencias opcionales que no estaban presentes. Los árboles usados durante el desarrollo no lo
mostraban porque ya tenían una configuración válida en caché.

El gate acepta ahora tres controles explícitos:

- `NSC_FORCE_MONOLITHIC_LLVM=ON` para evitar el paquete CMake incompleto;
- `NSC_LLVM_LIBRARY=<ruta a LLVM-C.lib>` para nombrar la biblioteca monolítica;
- `NSC_CXX_COMPILER=<ruta a clang-cl.exe>` para fijar el compilador anfitrión.

Esto no oculta el entorno usado. Hace que una construcción desde cero pueda repetir exactamente la
selección de toolchain que antes sólo sobrevivía dentro de un cache viejo.

## 4. E9-B — actualización y retiro ligados a propiedad demostrable

### 4.1 Formato `NSUPDATE 1`

`writeInstallationUpdatePlan` recibe el manifiesto instalado anterior y el manifiesto nuevo. El
plan guarda el SHA-256 de ambos manifiestos y clasifica la unión ordenada de rutas:

```text
NSUPDATE 1
current_manifest_sha256=<sha256>
next_manifest_sha256=<sha256>
keep|<ruta>
replace|<ruta>
add|<ruta>
remove|<ruta>
```

`writeInstallationRemovalPlan` usa el mismo formato, escribe
`next_manifest_sha256=none` y sólo produce acciones `remove` sobre el inventario actual.
Al aplicar un retiro completo también elimina el manifiesto instalado que sirvió como autoridad —no
puede contener su propio hash sin recursión— y poda subdirectorios del paquete que queden vacíos.
El prefijo y las raíces compartidas sobreviven.

Comandos públicos:

```text
nsc --write-installation-update-plan <current> <next> <plan>
nsc --write-installation-removal-plan <current> <plan>
nsc --apply-installation-removal-plan <prefix> <current> <next|none> <plan>
```

### 4.2 Por qué se exigen los dos manifiestos al aplicar

Ligar un plan únicamente al manifiesto anterior no bastaba. Alguien podía cambiar una acción
`keep` por `remove` sin tocar el hash del manifiesto viejo. Stage 66 exige también el manifiesto
nuevo, vuelve a calcular su SHA-256 y reconstruye el plan canónico desde ambos conjuntos.

Antes de borrar se comprueba que:

- los encabezados y digests sean válidos;
- las rutas sean relativas, canónicas, ordenadas y estén dentro de las raíces administradas;
- el hash del manifiesto anterior coincida;
- el hash del manifiesto siguiente coincida, cuando exista;
- cada acción sea exactamente la derivada de esos dos manifiestos;
- cada `remove` pertenezca al inventario anterior;
- ningún componente de la ruta sea un enlace simbólico;
- el destino sea un archivo regular;
- tamaño y SHA-256 actuales sigan siendo los registrados por el manifiesto anterior.

Todas las acciones de retiro pasan esta prevalidación antes de borrar el primer archivo. Si el
usuario modificó un archivo que la versión desea retirar, la operación se detiene y lo conserva.
Un fallo tardío de validación tampoco deja una eliminación parcial.

### 4.3 Convivencia con otras herramientas

La propiedad nunca se infiere por vivir bajo `bin` o `lib`. Sólo una ruta presente en el manifiesto
anterior puede eliminarse. Las pruebas colocan archivos vecinos en ambas raíces y demuestran que
sobreviven tanto a la actualización como al retiro completo de NativeSharp.

Este detalle cierra la lección de Stage 64: un prefijo compartido no es un directorio descartable.
Cada producto debe demostrar qué bytes son suyos.

### 4.4 Orden correcto de una reinstalación

La primera revisión de la integración reveló un error de orden: validar el layout nuevo antes de
retirar archivos exclusivos de la versión anterior convertiría un upgrade legítimo en fallo por
«archivo inesperado».

`VerifyInstalledNativeSharp.cmake` ejecuta ahora este flujo:

1. preserva temporalmente el manifiesto anterior;
2. deja que CMake copie el payload nuevo;
3. escribe el manifiesto nuevo desde `NSFILES 1`, sin adoptar residuos;
4. genera el plan ligado a los dos manifiestos;
5. prevalida y retira únicamente rutas `remove` de la versión anterior;
6. verifica el layout final;
7. verifica todos los bytes del manifiesto nuevo;
8. elimina manifiesto temporal y plan temporal.

La prueba física añadió un archivo sintético a la instalación A, lo registró como propiedad de la
versión anterior y reinstaló el build actual. El resultado fue:

```text
installation update plan written: .../.nativesharp-update.plan
installation removal plan applied: .../.nativesharp-update.plan
NativeSharp installation verified: .../prefix-a
installation manifest verified: .../NativeSharpInstallation.manifest
PASS: real reinstall retired the prior owned file and verified the final installation
```

No quedó el archivo retirado y no se tocó ninguna ruta ajena al manifiesto.

## 5. E10 — política de compatibilidad instalada y ejecutable

### 5.1 Una política, no una nota informal

`currentCompatibilityPolicy` es la autoridad única que genera `NSCOMPAT 1`. CMake la escribe con
el `nsc.exe` que acaba de enlazar, la instala bajo `share/nativesharp/toolchain`, la incluye en
`NSFILES 1` y la autentica mediante `NSINSTALL 1`.

Contenido de Stage 66:

```text
NSCOMPAT 1
compiler_release=0.9.0
language_release=0.1.0
compiler_compatibility=same-minor-before-1;same-major-after-1
language_compatibility=same-minor-before-1;same-major-after-1
deprecation=diagnostic-before-removal;one-minor-minimum
nsmeta_read=1.0-1.8
nsmeta_write=1.8
native_abi=1.0
runtime_abi=1.0
toolchain_contract=1
installation_files=1
installation_manifest=1
reproducibility_report=1
update_plan=1
```

La identidad efímera de build no aparece aquí. Compatibilidad e identidad física son preguntas
distintas: `NSCOMPAT 1` describe la promesa; `NSINSTALL 1` autentica los bytes concretos.

### 5.2 Reglas de versión

Antes de 1.0, un requisito de compilador sólo es compatible dentro del mismo major y minor; el
patch instalado debe ser igual o mayor. Así, 0.9.0 satisface 0.9.0, pero no promete sustituir 0.8.x
ni 0.9.1.

Desde 1.0, la regla se mueve a la misma línea major y exige que minor/patch instalados no sean
anteriores al requisito. Un cambio incompatible deberá mover el major.

El requisito debe usar exactamente `major.minor.patch`, sin ceros no canónicos ni componentes
omitidos.

Comandos públicos:

```text
nsc --print-compatibility-policy
nsc --write-compatibility-policy <file.policy>
nsc --verify-compatibility-policy <file.policy>
nsc --check-compiler-compatibility <major.minor.patch>
```

La política declara además la regla de retiro: un comportamiento soportado requiere diagnóstico de
deprecación durante al menos una versión minor antes de eliminarse. Una corrección de seguridad o
miscompilación puede endurecer la aceptación, pero sus notas deben identificar la excepción.

### 5.3 NSMETA, ABI y formatos de paquete

E10 no mezcla versiones que cumplen papeles distintos:

- `nsmeta_read=1.0-1.8`: rango que el consumidor puede leer;
- `nsmeta_write=1.8`: formato exacto que produce;
- `native_abi=1.0`: contrato físico de bibliotecas y DLL;
- `runtime_abi=1.0`: contrato del runtime nativo;
- los cuatro formatos de E9 quedan congelados individualmente en versión 1.

El verificador de instalación exige que la política instalada coincida exactamente con la que
genera el compilador. No se acepta una política futura, incompleta, reordenada o con campos
desconocidos.

## 6. Contrato normativo y trazabilidad

Se añadieron cuatro obligaciones a `NativeSharp_Compiler_Contract_1_0.md`:

- `NSC-PKG-006`: reproducibilidad entre builds independientes;
- `NSC-PKG-007`: actualización y retiro ligados a manifiestos;
- `NSC-COMPAT-001`: política de compatibilidad instalada;
- `NSC-COMPAT-002`: decisión ejecutable de requisitos de versión.

Las cuatro aparecen como `implemented` en
`NativeSharp_Compiler_1_0_conformance.map`, con implementación y prueba ejecutable. Stage 66 falla
si se pierde cualquiera de esos enlaces.

El contrato de toolchain `NSTOOLCHAIN 1` creció de 16 a 21 campos. Los cinco nuevos campos son:

- `installation_files`;
- `installation_manifest`;
- `reproducibility_report`;
- `update_plan`;
- `compatibility_policy`.

La primera matriz completa encontró que la fixture Stage 62 seguía contando 16. Se corrigió la
fixture para verificar primero los cinco nombres y después exigir un encabezado más 21 campos
ordenados. No se relajó el producto para acomodar el test.

## 7. Pruebas de Stage 66

`Stage66E9E10ReleaseContractTests.cpp` cubre, entre otros:

- manifiestos equivalentes y diferencia determinista nombrada;
- reporte `NSREPRO 1` canónico;
- clasificación `keep`, `replace`, `add` y `remove`;
- exigencia del manifiesto siguiente;
- rechazo de plan con acción alterada;
- rechazo de manifiesto anterior cambiado después de planear;
- rechazo de archivo propiedad del paquete modificado localmente;
- preflight completo sin eliminación parcial;
- supervivencia de ejecutable y biblioteca vecinos;
- retiro completo basado exclusivamente en propiedad registrada;
- bytes canónicos y rechazo estricto de `NSCOMPAT 1` incompatible;
- decisiones 0.9.0, 0.9.1, 0.8.9 y versión no canónica;
- comandos públicos y sus exit codes;
- orden operativo del script de reinstalación;
- gate de dos builds, normalización reservada y comparación SHA-256 del ejecutable;
- contrato normativo y mapa de conformidad;
- identidad final Stage 66.

Resultado directo final: **77 checks por configuración**. Stage 62 queda en **44 checks** y Stage
63 en **28 checks**.

El humo concurrente de ambas configuraciones encontró además una colisión exclusiva de la fixture:
los dos ejecutables Stage 66 compartían el mismo directorio bajo `%TEMP%` y cada uno limpiaba la
evidencia del otro. La raíz de la suite incluye ahora el identificador del proceso. Esto no cambia
el producto, pero permite comprobar simultáneamente NSLink y `lld-link` sin falsos negativos ni
archivos cruzados entre corridas.

## 8. Archivos creados

- `include/nativesharp/Driver/PackageLifecycle.h`;
- `src/Driver/PackageLifecycle.cpp`;
- `include/nativesharp/Driver/CompatibilityPolicy.h`;
- `src/Driver/CompatibilityPolicy.cpp`;
- `cmake/VerifyReproducibleNativeSharpPackage.cmake`;
- `tests/unit/Stage66E9E10ReleaseContractTests.cpp`;
- `mkdown/149_baseline_certificado_1_0_e9_stage64.md`;
- `mkdown/150_1_0_e9_e10_cierre_reproducibilidad_ciclo_de_paquete_y_compatibilidad.md`.

## 9. Archivos modificados y función del cambio

- `CMakeLists.txt`: Stage 66, fuentes nuevas, `/Brepro`, build id reservado, política generada e
  instalada, inventario de 77 entradas y registro de la suite;
- `cmake/GenerateBuildIdentity.cmake`: conserva identidad única por omisión y admite la identidad
  canónica exclusiva del gate;
- `cmake/VerifyInstalledNativeSharp.cmake`: actualización ligada a manifiestos antes de validar el
  layout final;
- `include/nativesharp/Basic/Version.h`: identidad Stage 66;
- `src/Driver/Driver.cpp`: ocho operaciones públicas nuevas y ayuda correspondiente;
- `src/Driver/InstallationVerifier.cpp`: exige y verifica `NativeSharpCompatibility.policy`;
- `src/Driver/ToolchainContract.cpp`: publica las cinco versiones de formato nuevas;
- `tests/unit/Cc1BuildIdentityTests.cpp`: espera Stage 66;
- `tests/unit/Stage62ToolchainContractTests.cpp`: verifica 21 campos, incluidos los cinco nuevos;
- `tests/unit/Stage63InstallationVerifierTests.cpp`: añade política válida e incompatible a sus
  instalaciones de prueba;
- `spec/NativeSharp_Compiler_Contract_1_0.md`: cuatro cláusulas normativas nuevas;
- `spec/NativeSharp_Compiler_1_0_conformance.map`: trazabilidad de esas cláusulas;
- `mkdown/121_next_steps_for_the_1_0_glory.md`: estado de cierre E9+E10;
- `mkdown/148_1_0_e9_inventario_criptografico_de_instalacion.md`: continuación histórica desde
  Stage 64;
- `mkdown/BUILD.md`: operación de los comandos y gates nuevos;
- `mkdown/CONTENIDO.txt`: índice de la evolución.

## 10. Evidencia acumulada

Antes de la promoción G00, esta fotografía tiene:

- compilación de `nsc.exe` Stage 66: aprobada;
- Stage 66 directo: 77/77 por configuración;
- Stage 62 reparado: 44/44 por configuración;
- comparación de dos construcciones e instalaciones limpias: aprobada;
- paquete: 76 entradas deterministas, una portadora de identidad, cero diferencias;
- `nsc.exe` con identidad normalizada: SHA-256 completo idéntico;
- reinstalación física con un archivo retirado: aprobada;
- matriz CTest lld-link: 303/303, cero fallos, 894.61 s;
- matriz CTest NSLink: 303/303, cero fallos, cero pruebas omitidas y 854.31 s acumulados.

Las dos matrices corresponden al árbol final de código de Stage 66. El capítulo se actualizó sólo
después de leer los 303 ordinales de NSLink y comprobar que el registro no contenía fallos,
timeouts ni pruebas sin ejecutar.

## 11. Promoción de la fotografía

G00 `20260907-233625-801b209e` reconstruyó e instaló Stage 66 y terminó `LANDED`, con 11/11 etapas,
303/303 pruebas CTest por linker, 324/324 casos del runner y cero fallos. El árbol permaneció
inmóvil en 1034 archivos y `built_nsc == installed_nsc` con SHA-256
`9958ba8ab401ada17879b412670770b4504adb32b8ec9f3cee2fc6d7783d63b1`.

La identidad visible del binario, los cuatro cuadrantes y los hashes del paquete instalado se
conservan en el acta 151. E9 y E10 quedan cerradas y certificadas.
