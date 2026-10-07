# Tanda Universal 06: herramientas oficiales para CTest y reejecución dirigida

## Estado

`IMPLEMENTADO; VALIDACIÓN ESTÁTICA APROBADA; PRUEBAS FUNCIONALES NO EJECUTADAS`

## SCOPE LOCK

- Objetivo exacto autorizado: hacer que NSC Builder, CTest y el runner de pruebas fallidas ejecuten los
  productos NativeSharp con `C:\native-sharp\bin\nsc.exe` y
  `C:\native-sharp\bin\nslink.exe`.
- Naturaleza: corrección de infraestructura de pruebas.
- Comportamiento que debe permanecer idéntico: selección de pruebas, optimización, fixtures, expectativas,
  compilación del host C++, generación de evidencia y códigos de salida.
- Revisión conocida como correcta: el estado previo al presente commit conserva la batería y sus pruebas;
  esta tanda sólo corrige la procedencia de los dos ejecutables NativeSharp.
- Rango evaluado: árbol `development` posterior a `fd25ae369`, incluyendo los cambios locales de esta tanda.
- Archivos modificables: `CMakeLists.txt`, `nscbuilder.py`, `nscbuilder.pyw`, `run_failed_tests.py` y este
  documento.
- Archivos y subsistemas protegidos: compilador C++, semántica, parser, fixtures, expectativas, proyectos,
  bibliotecas, runtime, NSLink, instalaciones y cualquier archivo bajo `C:`.
- Pruebas que deben permanecer sin modificaciones: todo el inventario CTest existente y `test.run`.
- Acciones prohibidas: ejecutar baseline, instalar, desplegar, copiar archivos a `C:`, cambiar Python, CMake,
  CTest, Ninja o LLVM a rutas absolutas, y modificar el compilador o NSLink instalados.
- Criterio de terminación: todas las entradas CTest que ejecutan NativeSharp reciben el NSC oficial; todos los
  enlaces NativeSharp reciben el NSLink oficial; las herramientas auxiliares continúan resolviéndose por
  `PATH` o por el cache/configuración existente; el diff no altera verificaciones del host recién construido.

## Problema y causa

La infraestructura conservaba varias autoridades para los ejecutables NativeSharp: el target `nsc` del árbol,
el prefijo de staging, el prefijo elegido en la GUI y rutas de NSLink derivadas del directorio de instalación.
Eso permitía que una corrida seleccionada desde NSC Builder y una reejecución desde `test.run` midieran binarios
distintos aunque compartieran el mismo árbol CTest.

La corrección no consiste en fijar toda la toolchain a una unidad. Sólo los dos productos bajo prueba tienen
autoridad explícita:

```text
NSC    -> C:\native-sharp\bin\nsc.exe
NSLink -> C:\native-sharp\bin\nslink.exe
```

Python, CMake, CTest, Ninja y LLVM no reciben rutas nuevas en esta tanda. CMake y CTest se invocan por nombre;
las demás herramientas conservan la resolución ya configurada por el entorno o el árbol de build.

## Cambios por archivo

### `CMakeLists.txt`

- `NSC_TEST_COMPILER` tiene como valor predeterminado el NSC oficial.
- `NSC_NSLINK_EXECUTABLE` tiene como valor predeterminado el NSLink oficial.
- Las pruebas que antes invocaban `$<TARGET_FILE:nsc>` usan `NSC_TEST_COMPILER`.
- Las pruebas que derivaban NSLink del prefijo de instalación usan `NSC_NSLINK_EXECUTABLE`.
- Las verificaciones `POST_BUILD` y los generadores de contratos conservan `$<TARGET_FILE:nsc>` porque validan
  o consumen el host que se acaba de construir; no son pruebas del compilador oficial.
- `Python3_EXECUTABLE`, `CMAKE_COMMAND`, `CMAKE_LINKER` y las demás herramientas existentes no cambian.

### `nscbuilder.py`

- `TEST_NSC` y `TEST_NSLINK` definen exclusivamente los dos productos NativeSharp oficiales.
- La configuración de los árboles CTest transmite ambos valores mediante cache CMake.
- El entorno de pruebas publica `NSC_EXE` y `NSC_LINKER` con esos valores.
- El cache incremental incluye los hashes de los mismos ejecutables que realmente mide la corrida.
- Los casos compilados directamente por NSC Builder usan el mismo NSC oficial.
- La compilación del host conserva su toolchain y su flujo independientes.

### `nscbuilder.pyw`

- El entry point de ventana delega inmediatamente en el `main` canónico de `nscbuilder.py`.
- La copia histórica que permanece debajo no se ejecuta al abrir el `.pyw`; por tanto, la GUI no puede omitir
  esta corrección ni las mejoras recientes de `test.run`.

### `run_failed_tests.py`

- Los valores predeterminados `--nsc` y `--nslink` apuntan a los ejecutables oficiales.
- El árbol CTest se reconfigura antes de la reejecución para impedir que sobreviva un cache con otra autoridad.
- El comando usa `cmake` y `ctest` por nombre; no fija su ubicación.
- El entorno hijo sólo agrega `NSC_EXE` y `NSC_LINKER`.
- Los reportes JSON y Markdown registran las rutas exactas de NSC y NSLink utilizadas.

## Algoritmo, complejidad y ownership

La selección de herramientas es constante por corrida. La reconfiguración conserva complejidad temporal y
espacial propia de CMake; no añade recorridos de fuentes ni duplicación de artefactos. Cada proceso hijo recibe
una copia del entorno y no muta el entorno del proceso padre. No se incorpora recursión.

## Fronteras y errores

El runner verifica que ambos ejecutables NativeSharp existan antes de modificar el árbol CTest. Un fallo de
configuración conserva la salida completa en `configure.log` y termina con código distinto de cero. No se
intenta localizar herramientas auxiliares en rutas alternativas ni instalar dependencias.

## Alternativas descartadas

- Copiar NSC o NSLink desde `C:` hacia `A:`: viola la autoridad única solicitada y crea residuos divergentes.
- Fijar Python, CMake, CTest, Ninja o LLVM a rutas absolutas: contradice la resolución administrada por el
  sistema y puede seleccionar otra instalación.
- Usar siempre `$<TARGET_FILE:nsc>` en CTest: vuelve a medir el host de desarrollo en lugar del compilador
  oficial indicado por el usuario.
- Reemplazar `$<TARGET_FILE:nsc>` en pasos `POST_BUILD`: validaría el binario equivocado; por eso esos pasos se
  mantienen asociados al target recién construido.

## Verificación ejecutada antes del commit

- `git diff --check`: aprobado.
- Parseo AST de `nscbuilder.py`, `nscbuilder.pyw` y `run_failed_tests.py` mediante `python` resuelto por `PATH`:
  aprobado.
- `python run_failed_tests.py --list`: aprobado; enumeró las 47 pruebas del manifest sin ejecutarlas.
- Auditoría determinista de todas las referencias CTest a NSC y NSLink: aprobada.
- Auditoría del diff: las únicas rutas nuevas bajo `C:` son los dos ejecutables NativeSharp autorizados.

No se ejecuta baseline, build, instalación ni prueba funcional durante este commit de infraestructura. La
batería O0 completa permanece como gate posterior a la tanda funcional, conforme a la metodología vigente.

## Pendientes y límites

- La existencia de los binarios oficiales se comprueba al ejecutar; este commit no escribe ni los reemplaza.
- El resultado funcional de la batería no se afirma en este documento.
- Esta tanda no resuelve los defectos de compatibilidad universal ni modifica `test.run`.
