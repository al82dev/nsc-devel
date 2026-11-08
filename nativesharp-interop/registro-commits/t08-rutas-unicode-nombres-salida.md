# T08-002-09m7c0g2b: equivalencias Unicode y nombres de producto que la página de códigos no representa

Fecha: 2026-11-08. Rama `development-cpp-clang`. Compilador de desarrollo `A:\nsc-interop-build\tanda1-ON`.

## Qué faltaba

La casilla pedía comprobar otras equivalencias Unicode y otras entradas de ruta, además del caso `É`/`é`
que ya estaba cubierto. Las pruebas nuevas encontraron cinco fallos, todos con el mismo origen. NSC
convertía rutas a texto con `std::filesystem::path::string()` o `generic_string()`. En Windows, esas
funciones usan la página de códigos activa (CP1252 en este equipo) y lanzan `No mapping for the Unicode
character exists in the target multi-byte code page` cuando un carácter no cabe en ella. NSC terminaba
entonces con `internal error: unhandled exception`.

| Entrada | Síntoma antes del cambio | Causa |
|---|---|---|
| Biblioteca `-o Liba◌́.lib` (acento combinante) | error interno al construir la identidad del paquete | `output-name` usaba `generic_string()` (`PrecompiledModuleConfiguration.cpp`) |
| Consumir `Libé.lib` escrita en NFD | error interno al cargar el grafo | `stem().string()` en `preferExactSourceModuleArtifacts` (`ProjectGraphLoader.cpp`) |
| Ejecutable `-o Prog-é.exe` | NSLink terminaba con 0xC0000409 | NSLink lee su línea de comandos y las rutas del archivo de respuesta con la página ANSI |
| Ejecutable `-o Prog漢.exe` | `NSL1056: cannot read the response file` con `Prog?.exe.link.rsp` | la misma limitación de NSLink |
| `TEMP` en un directorio con U+6F22 | error interno al emitir el objeto | `outputPath.string()` en `LlvmTargetMachine::emitImpl` |

## Cambios

- `PrecompiledModuleConfiguration.cpp`: los campos de identidad `output-name`, `link-tool-*`,
  `extra-link-library-*`, `debug-prefix-*` y `debug-source-root`, así como los mensajes de error de
  sesión C/C++, ahora usan UTF-8 (`pathToUtf8`). Los valores ASCII no cambian.
- `ProjectGraphLoader.cpp`: el nombre base de la biblioteca y de los metadatos se compara en UTF-8
  (`stemUtf8`), igual que los nombres de módulo.
- `LlvmTargetMachine.cpp`: la ruta de salida se pasa a LLVM en UTF-8, que es la codificación que LLVM
  espera en Windows.
- `ThinLtoBackend.cpp`: las rutas que se pasan a LLVM (objetos, archivos y directorio de caché) se
  convierten a UTF-8.
- `CompilationArtifactCache.cpp` y `CacheEntryLock.cpp`: la ruta del candado se forma como
  `path += ".lock"` y los diagnósticos usan UTF-8. Antes se formaba como
  `path.string() + ".lock"`, que lanzaba con un directorio de caché no representable.
- `LldLinker.cpp`, solo cuando el enlazador es NSLink (lld-link lee UTF-8 y no cambia):
  - Si la ruta del archivo de respuesta no es ASCII, el archivo se escribe como `__nsc_link.rsp` dentro
    del directorio de enlace privado y se pasa relativo a ese directorio, que es el directorio de trabajo
    de NSLink.
  - Si `/out:`, `/implib:` o el PDB tienen una ruta no ASCII (nombre o directorio temporal), el enlace
    escribe nombres ASCII relativos (el propio nombre si ya es ASCII; si no, `__nsc_output.<ext>`).
    `publishRelocatedProducts` los mueve después a las rutas pedidas. Cuando el nombre del PDB cambia,
    `/pdbaltpath:` lleva el nombre UTF-8 del PDB publicado. NSLink copia esos bytes, así que el registro
    RSDS del PE nombra el PDB que el depurador encontrará junto al ejecutable.
  - Los objetos con nombre base no ASCII se vinculan en el directorio de enlace como `__nsc_output.obj`,
    con el prefijo ordinal habitual para separar duplicados.
  - Una DLL con nombre de archivo no ASCII se rechaza con el diagnóstico `NSLink cannot link a DLL whose
    file name has characters outside ASCII ('…'); use an ASCII name or link with lld-link`. Una DLL
    conserva el nombre con que se enlazó (directorio de exportaciones e importación). Se probó un archivo
    `.def` con `LIBRARY`, pero NSLink da prioridad a `/out:`. Por eso no se puede enlazar con un nombre
    ASCII y renombrar después, como se hace con un ejecutable.
  - `executableName` usa UTF-8.

Cuando todas las rutas son ASCII, el enlace recibe los mismos argumentos que antes. Los PE y PDB de los
builds existentes no cambian.

## Pruebas

`pruebas-tanda-08-identidad-c/rutas-unicode/validar-equivalencias.py` (nueva):

- El oráculo es `os.path.samefile` sobre el sistema de archivos real. Para cada par, NSC debe cargar un
  solo archivo cuando Windows ve uno solo y cargar los dos cuando Windows ve dos.
- Pares: NFC/NFD `é`, `İ`/`i`, `ı`/`I`, `ß`/`ẞ`, `ǅ`/`ǆ`, `Σ`/`ς`, K (Kelvin)/`k` y el control `é`/`É`.
  Windows trata como el mismo archivo solo el control.
- Cubre fuentes, `ProjectReference` (el duplicado se diagnostica) y encabezados C.
- En cada caso, el ejecutable de salida va en un directorio con el otro carácter del par y un nombre con
  el primero; la caché va en un directorio con el primero. Se comprueba la construcción, `not-found`
  seguido de `hit` y la ejecución.
- Resultado: aprobada, 8/8.

`pruebas-tanda-08-identidad-c/rutas-unicode/validar-nombre-salida.py` (nueva):

1. Bibliotecas `.lib` con nombres NFC, NFD, combinante, U+6F22 y Kelvin, consumidas por un ejecutable
   ASCII en O0.
2. Ejecutables con esos cinco nombres, en O0, O1, O2 y O3 con `-g`. Se comprueba la ejecución, que el PDB
   se publica con el nombre pedido, que el RSDS nombra ese PDB en UTF-8 y que no quedan restos en la
   carpeta de salida.
3. `TEMP`/`TMP` y la caché en directorios con U+6F22, en O0 y O2 con `-g`: construcción, restauración
   (`hit`) y ejecución.
4. DLL:
   - NSLink con `TEMP` CJK: una DLL ASCII se construye en O0 y O2. El nombre exportado y el RSDS son
     correctos y un consumidor que la importa se ejecuta.
   - Negativo: NSLink rechaza las cinco DLL no ASCII con el diagnóstico, sin error interno y sin dejar
     producto.
   - lld-link: las cinco DLL no ASCII se enlazan en O0 y O2 con el nombre exportado y el RSDS correctos,
     y `LoadLibraryW` las carga. Además, una DLL ASCII de lld-link se importa desde un consumidor.
- Resultado: aprobada.

Regresión dirigida, todas aprobadas:

- `rutas-unicode/validar.py`.
- `tls-dll-unicode/validar.py --cache --unicode-source --unicode-producer`: DLL C U+6F22, caché,
  productor y fuente Unicode.
- `pruebas-tanda-08-tls-c/precompiled/validar-dll-precompilado.cmd`: DLL NativeSharp con NSLink en O0–O3.

## Límite encontrado: decisión T08-002-09m7c0g2d

Una DLL NativeSharp con nombre no ASCII se puede producir con lld-link y la carga `LoadLibraryW`. Sin
embargo, un ejecutable que la importa por nombre termina con `STATUS_DLL_NOT_FOUND` (0xC0000135). Esto
se observó con los cinco nombres, aunque la DLL esté junto al ejecutable y la tabla de importación tenga
el nombre en UTF-8. Una DLL ASCII en la misma disposición sí carga.

En cambio, la DLL C con U+6F22 de `tls-dll-unicode` sí carga en su consumidor, aunque el nombre UTF-8
también aparece en el ejecutable. No se determinó qué distingue ese camino; la hipótesis no verificada es
la forma en que NSC carga las DLL C. NSLink, además, no puede producir estas DLL NativeSharp. Por eso hay
que elegir entre:

- rechazar siempre los nombres de DLL NativeSharp no ASCII; o
- importarlas con carga diferida UTF-8, como las DLL C, y mantener lld-link como requisito o esperar
  soporte de NSLink.

Queda anotado en el tracking como decisión pendiente. Mientras tanto, NSLink da un diagnóstico claro en
lugar de terminar con 0xC0000409.
