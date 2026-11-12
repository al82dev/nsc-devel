# Propuesta no aplicada: registro dirigido de realloc

## Hallazgo

Al revisar `CMakeLists.txt` de `A:/nsc-limitaciones-asan` y
`A:/nsc-devel-fixes`, no se encontró referencia a
`tests/integration/asan_heap_realloc_preserve.py`. El test fue ejecutado
directamente y pasó O0/O2 según `docs/fixes/asan-realloc-exitoso.md`; eso no
demuestra que CTest lo descubra o ejecute.

El archivo vecino `cmake-registro-realloc-propuesta.cmake` contiene un
fragmento propuesto para que lo revise el integrador. No se insertó en
`CMakeLists.txt`, no se configuró ni se ejecutó con CTest. No altera los
ejecutores existentes. Es una propuesta, no un parche ya validado.

## Dependencias y condiciones

- Windows; Python accesible como `python` desde el PATH autorizado.
- `${NSC_TEST_COMPILER}` debe identificar el `nsc` construido desde la copia
  y configuración que se pretende validar. Su valor por defecto puede resolver
  una instalación anterior: registrar ruta y hash y corregir la configuración
  mediante el procedimiento autorizado por el usuario, no asumir procedencia.
- `NSC_TEST_ASAN_CLANG`, `NSC_TEST_ASAN_RUNTIME` y `NSC_TEST_TEMP_ROOT` deben
  existir **al configurar CMake**: el fragmento captura sus valores mediante
  `$ENV{...}`. Para cambiarlos es necesario volver a configurar. Clang y DLL son
  dependencias externas de referencia; no modificar sus fuentes ni instalación.
- Al ejecutar el test, `NSC_WINDOWS_RUNTIME_LIBRARY` debe apuntar al runtime
  aislado y `NSC_WINDOWS_RUNTIME_IMPORT_LIBRARY_DIRECTORY` a sus import
  libraries. También se requieren `NS_LIB_ROOT`, `NSC_LINKER` y temporales
  `TEMP`, `TMP`, `APPDATA`, `LOCALAPPDATA` acordes con el build autorizado;
  todas las salidas, cachés y directorios temporales deben permanecer en A.
- El test requiere que el directorio temporal exista y que estén disponibles
  las bibliotecas de importación `KERNEL32`, `KERNELBASE`, `SHELL32` y
  `UCRTBASE`. El ejecutor valida entradas, registra sus hashes y termina con
  fallo si no existen. Con configuración ausente no se propone declarar éxito
  ni asignar automáticamente `SKIP_RETURN_CODE`.
- Mantener el cwd en la raíz de la copia, no cambiar PATH, y respetar las
  autorizaciones concretas de rutas de la configuración que utilice el
  integrador. Los directorios de build y temporales deben ser independientes.

## Validación pendiente para el integrador

Revisar la ubicación del registro y sus nombres/variables en su árbol; aplicar
la adaptación mínima si corresponde. Configurar sin escribir fuera de A,
comprobar mediante el mecanismo de descubrimiento de CTest que el nombre existe
y ejecutar **solo esta prueba**. Confirmar comandos efectivos, cwd, archivos,
hashes y resultado; contrastar con la ejecución directa previa. Probar por
separado la ausencia de configuración sin presentarla como éxito ni como
cobertura. No ejecutar una batería completa.

## Límites

El fragmento es específico para este ejecutor y no registra las otras pruebas
dirigidas ni implica que el SDK esté instrumentado. Una omisión del test en
plataformas distintas de Windows no demuestra que pase allí. La ejecución
histórica usó `detect_leaks=0`; no demuestra ausencia de fugas ni que `realloc`
haya movido el bloque. No se ha modificado ninguna otra copia o archivo
compartido, ni se ha aplicado el fragmento.
