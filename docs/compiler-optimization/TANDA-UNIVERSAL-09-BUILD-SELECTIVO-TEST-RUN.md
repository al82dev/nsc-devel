# Tanda universal 09: construcción selectiva de `test.run`

## SCOPE LOCK

- Objetivo: preparar y ejecutar exclusivamente las pruebas enumeradas en `test.run`.
- Fuentes y build: `A:\nsc-devel`.
- NSC oficial: `C:\native-sharp\bin\nsc.exe`.
- NSLink oficial: `C:\native-sharp\bin\nslink.exe`.
- Herramientas restantes: resolución por `PATH`.
- Prohibido: construir la batería completa, instalar, desplegar, modificar `C:` o ejecutar baseline.
- Criterio: sólo se construyen los targets C++ seleccionados y sus dependencias declaradas.

## Diseño

El manifiesto contiene dos clases de pruebas:

- ejecutables C++ registrados directamente por CTest;
- pruebas orquestadas por CMake o Python que utilizan NSC sobre fixtures NativeSharp.

Después de configurar el árbol dedicado, el runner analiza el `CTestTestfile.cmake` generado. Para cada nombre de
`test.run`, identifica el comando registrado. Si el comando es un `.exe` producido dentro del árbol de build, su
nombre de archivo determina el target CMake correspondiente. Los comandos CMake y Python no agregan targets.

El runner llama una sola vez a `cmake --build` con la lista deduplicada de targets. Ninja construye esos targets
y únicamente sus dependencias normales. No se usa el target `all`.

## Flujo

1. Leer y deduplicar `test.run`.
2. Configurar `build-failed-tests` con Ninja y LLVM descubiertos mediante `PATH`.
3. Registrar NSC y NSLink oficiales como herramientas de ejecución.
4. Traducir cada CTest seleccionada a cero o un target C++.
5. Construir la lista completa de targets seleccionados en una sola invocación.
6. Crear el overlay de metadatos sin modificar el árbol construido.
7. Ejecutar exclusivamente los nombres originales de `test.run`.

La única evidencia persistente es un log consecutivo junto al script:

```text
putadas_que_fallan_NUMERO.log
```

Los metadatos CTest adaptados son temporales y se eliminan automáticamente. No se generan reportes Markdown,
JSON, carpetas de historial ni logs individuales.

La configuración y la compilación selectiva se muestran únicamente en consola. El log consecutivo se crea
después de ambas etapas y contiene exclusivamente la ejecución de las pruebas de `test.run`.

El árbol dedicado se configura con `CMAKE_INSTALL_PREFIX=C:/native-sharp`. El entorno de ejecución publica
`NS_ROOT`, `NS_LIB_ROOT`, `NS_LIB_READY` y `NSC_WINDOWS_RUNTIME_LIBRARY` desde esa misma instalación. El overlay
reemplaza únicamente NSC y NSLink; no degrada las rutas de herramientas descubiertas por CMake a nombres que
los verificadores confundan con archivos inexistentes.

## Complejidad

El análisis del metadato cuesta `O(t * n)`, donde `t` es la cantidad de pruebas solicitadas y `n` el tamaño del
catálogo CTest. La compilación queda limitada al cierre de dependencias de los targets seleccionados.

## Archivos

- `run_failed_tests.py`: configuración dedicada, extracción de targets y construcción selectiva.
- Este documento: contrato, algoritmo y límites del cambio.

## Validación previa al commit

- `git diff --check`: aprobado.
- Ejecución de pruebas: prohibida antes del commit.
- Baseline, instalación y deploy: no ejecutados.
