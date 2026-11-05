# Etapa A: línea base y límites

Fecha: 2026-11-02. Fuentes detalladas, comandos y limitaciones: `ETAPA-A.md` y la carpeta `evidencia`.

## Resultado

La construcción realizada por el usuario produjo `A:/al82dev-clang/bin/clang.exe`, Clang 23.1.0 del commit `ea7d852a70e8bdfaf601d6626a760f9771b2c4b4`, target `x86_64-pc-windows-msvc`. Su SHA-256 está en `evidencia/clang-sha256.txt`. El script de construcción está en `A:/llvm-project-al82dev/build-nativesharp-clang.cmd` y la configuración CMake en `A:/al82dev-clang/CMakeCache.txt`.

El conjunto de comandos generados para ese target contiene 2673 compilaciones `clang-cl` con `-MT`. Se localizaron sus 2673 objetos: 2664 declaran `MT_StaticRelease` y nueve solicitan `libcmt.lib`; ninguno declara una biblioteca de runtime dinámico de la lista examinada. La auditoría no incluye objetos de ensamblador ni bibliotecas externas y no recupera una bitácora completa de la construcción que ejecutó el usuario. Evidencia y método: `evidencia/auditoria-runtime.json`, `evidencia/auditoria-objetos.json` y los scripts asociados.

Las tablas de importaciones del Clang y del `nsc.exe` instalado no muestran DLL de runtime de Visual C++; se conservan en `evidencia/clang-imports.txt` y `evidencia/nsc-imports.txt`. Esto no verifica dependencias transitivas ni carga dinámica. NSC reporta versión 3.0.0 y build hash `4998f7ed435b91d5`; no se ha establecido que corresponda al commit observado en `A:/nsc-devel`.

## Evidencia reconstruida

Todas estas ejecuciones se verificaron nuevamente con código cero mediante `A:/clang-tests/verificar-resultados.cmd`; el registro es `evidencia/ejecuciones-reconstruidas.txt`. Los scripts que produjeron los resultados crean carpetas de trabajo nuevas y no reutilizan los binarios históricos.

| Caso | Resultado reconstruido |
|---|---|
| Compilación C y C++ con STL | `repro-20261102-01/hello-c.exe`, `hello-cpp.exe` |
| Función NS `extern C`, biblioteca estática, consumidores C y C++ | `ns-estatico-02/consumer-c.exe`, `consumer-cpp.exe` |
| Función NS `extern C`, DLL, consumidores C y C++ | `dll-ns-01/consumer-c.exe`, `consumer-cpp.exe` |
| Función NS nativa, biblioteca estática y DLL | `nativos-01/static/consumer.exe`, `nativos-01/native-function/consumer.exe` |
| Clase NS en estática, métodos estáticos y clase en DLL | `nativos-01/class/consumer.exe`, `nativos-01/class-static/consumer.exe`, `nativos-01/class-dll/consumer.exe` |
| Clase NS con estado en O0 y O2 | `clase-estado-01/consumer-o0.exe`, `consumer-o2.exe` |
| EXE NS que llama función `extern C` de DLL C++ | `reverso-01/reverse.exe` |

Los ejemplos de clases y símbolos nativos usan adaptaciones manuales. No demuestran integración automática, semántica general de objetos ni interoperabilidad completa. La inspección de exportaciones C++ y la reconstrucción de DLL NS sin caché están documentadas por separado en `ETAPA-A.md`.

`verificar-evidencia.py` comprobó la consistencia de 188 hashes de archivos de prueba, las 14 salidas y los conteos de objetos e insumos, sin errores; su resultado está en `evidencia/verificacion-evidencia.json`. Comprueba la evidencia capturada y no reemplaza las verificaciones que faltan para la distribución o la interoperabilidad completa.

## Herramientas e insumos observados

La traza del ejemplo C++ muestra que el Clang nuevo usa `lld-link.exe` de `C:/clang-llvm/bin`, recursos de `A:/al82dev-clang/lib/clang/23` y el sysroot `A:/vc2026-runtime`. Se inventariaron con SHA-256 253 headers y siete bibliotecas observadas en ese ejemplo: `evidencia/entradas-ejemplo-cpp.json`; sus registros originales están en `A:/clang-tests/archivos-01/compilacion-inventario.txt` y `A:/clang-tests/enlace-01/enlace.txt`. No es un inventario universal. Los hashes de las herramientas locales están en `evidencia/tool-*-sha256.txt` y los de pruebas, en `evidencia/huellas-pruebas.json`.

Con `zlib`, `zstd` y `libxml2` desactivados existen limitaciones de compresión y no se construye `llvm-mt` en esa configuración. `ETAPA-A.md` identifica ejemplos en las fuentes. **Aún no existe evidencia de un paquete C/C++ autocontenido probado en otra computadora**, ni se ha decidido qué soporte de compresión debe incluir. Los derechos de redistribución de los archivos concretos también están pendientes antes de distribuirlos.

## Frontera

La etapa A fija y documenta esta línea base con sus límites; no demuestra compatibilidad completa ni autoriza iniciar la etapa B. No se trabajó en Daikon durante esta etapa. Cualquier incorporación de archivos al paquete, distribución o prueba de instalación limpia requiere su trabajo y verificación correspondientes.
