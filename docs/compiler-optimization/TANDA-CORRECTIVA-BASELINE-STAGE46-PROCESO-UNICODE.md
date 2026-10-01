# Tanda correctiva — Stage 46 en proceso aislado y rutas Unicode

**Fecha:** 2026-10-01  
**Rama:** `development`  
**Estado:** terminada; pendiente de certificación mediante el baseline ejecutado por Alberto  
**Configuración de validación local:** Debug / O0

## Problema observado

El baseline terminó `nsc.stage46.native-libraries` con `0xc0000409` después de más de 160 segundos.
La evidencia conservada mostró que el fallo aparecía al final de un único proceso de prueba que ya
había ejecutado decenas de compilaciones embebidas. El mismo consumidor de la cadena
`NativeMath.dll -> CompositeNative.dll -> CompositeConsumer.exe`, compilado en un proceso nuevo,
se generaba y devolvía `42`.

La validación dirigida encontró además un defecto real independiente: `nsc.exe` recibía rutas
Unicode de Windows a través de argumentos estrechos y la publicación atómica convertía después
las rutas UTF-16 a la página de códigos local mediante `path::string()`. Un directorio como
`paquete_áé_运行` podía fallar antes de publicar el artefacto.

## Corrección

1. Stage 46 recibe desde CMake la ruta del `nsc.exe` construido por la configuración activa.
2. Los consumidores finales de las cadenas DLL se compilan mediante un proceso nuevo de ese
   ejecutable. Esto conserva el contrato productivo y evita que el estado acumulado por docenas de
   compilaciones embebidas contamine la comprobación final.
3. Se añadió un modo dirigido, no registrado como CTest, para comprobar exclusivamente el cierre
   DLL O0 sin ejecutar el corpus Stage 46 completo ni sus variantes O2.
4. En Windows, `nsc.exe` entra por `wmain` y convierte cada argumento UTF-16 a UTF-8 de forma
   explícita antes de reutilizar el driver existente.
5. El driver convierte rutas UTF-8 a `std::filesystem::path` mediante
   `MultiByteToWideChar(CP_UTF8)`.
6. `ArtifactTransaction` deja de formar nombres hermanos mediante `path::string()`, conserva la
   ruta nativa y usa UTF-8 únicamente para diagnósticos. La creación de directorios intenta primero
   la ruta UTF-16 normal y mantiene `\\?\` como fallback de rutas largas.
7. La prueba de publicación de T34 incorpora una regresión permanente que crea y confirma un
   artefacto dentro de un directorio UTF-16.

## Archivos modificados

- `CMakeLists.txt`
  - Inyecta `NSC_TEST_NSC_EXECUTABLE` con `$<TARGET_FILE:nsc>` en Stage 46.
- `src/nsc/main.cpp`
  - Añade la entrada Windows `wmain` y la conversión estricta UTF-16 → UTF-8.
- `src/Driver/Driver.cpp`
  - Hace explícita la conversión UTF-8 → UTF-16 para las rutas de salida.
- `src/Basic/ArtifactTransaction.cpp`
  - Elimina conversiones ANSI de destinos y diagnósticos; conserva publicación atómica y fallback
    de rutas largas.
- `tests/unit/Stage46NativeLibrariesTests.cpp`
  - Ejecuta los consumidores DLL finales con el `nsc.exe` recién construido y expone el modo O0
    dirigido; en Windows usa `wmain` para no corromper la fixture Unicode.
- `tests/unit/NativeSharp3T34LongPathPublicationTests.cpp`
  - Añade la regresión de publicación en `paquete_áé_运行`.
- `MANIFEST_SHA256.txt`
  - Actualiza los hashes de los archivos cubiertos por el manifiesto.

## Archivos nuevos

- Este documento. Se crea porque la metodología exige una memoria técnica por commit con causa,
  alcance, archivos, mejora y evidencia.

## Mejora obtenida

- Stage 46 prueba el límite de proceso real usado por los usuarios y deja de depender del estado
  global acumulado por el arnés monolítico.
- El ejecutable de prueba nunca toma `nsc.exe` de `C:\native-sharp`; usa el compilador de
  `A:\nsc-devel` construido en la configuración activa.
- NSC puede crear, publicar y ejecutar un consumidor O0 en una ruta Windows que contiene caracteres
  latinos y CJK.
- La transacción multiarchivo conserva sus garantías de staging, commit y rollback sin estrechar
  rutas Unicode.

## Validación dirigida realizada

- Construcción exclusiva de `nsc` y `nsc_stage46_native_libraries_tests`: correcta.
- Modo dirigido Stage 46 O0 desde un directorio Unicode inexistente: compilación correcta,
  `packaged-runtimes: 2` y ejecución con código `42`.
- `nsc.ns3.t34.long-path-publication`: `1/1` correcta, incluida la nueva regresión Unicode.
- No se ejecutaron O1, O2 ni O3.
- No se ejecutó el baseline.
- No se modificó ni se leyó como origen `C:\native-sharp`.

## Pendiente

Alberto ejecutará el baseline completo. Esta tanda no se considera certificación del Gate 0 hasta
que esa ejecución externa termine sin fallos.
