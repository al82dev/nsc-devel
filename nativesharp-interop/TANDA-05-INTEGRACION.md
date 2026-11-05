# Tanda 05: integración de la sesión y diagnósticos

Estado: implementación en curso; la tanda no está validada ni cerrada.

- El controlador crea compilaciones propias para ejecutables y bibliotecas
  estáticas y dinámicas en `src/Driver/DriverArtifactGeneration.cpp`. Cada ruta
  pasa por `configurePrecompiledModules`; existen además otras rutas de análisis
  que requieren revisión antes de declarar completo el inventario T05-001.
- `CompilerSession.cpp` configura la fábrica solamente cuando se solicita una
  raíz de Clang; `configurePrecompiledModules` abre las sesiones por proyecto
  después de rechazar configuraciones incompatibles de caché. Las referencias
  sin configuración y los binarios construidos sin interop tienen ahora errores
  específicos. Pendiente comprobar todos estos recorridos en ejecución.
- `ProjectCompilation.h` declara las sesiones antes de los modelos y callbacks
  del proyecto; el orden inverso de destrucción hace que esos miembros se liberen
  antes que las sesiones. Su inyección se rechaza tras iniciar la compilación.
- El servicio de lenguaje crea compilaciones independientes para análisis y
  autocompletado. `AnalysisSnapshotImplementation` conserva una compilación con
  `shared_ptr`, por lo que también conserva las sesiones que esta posee. Esto
  describe propiedad, no demuestra que el servicio configure o abra sesiones
  C++: esa integración y las pruebas de vida útil requieren revisión.
- El adaptador nuevo conserva severidades, notas y ubicaciones externas sin
  suponer que un archivo abierto posteriormente coincide con el visto por Clang.
  La presentación sin rango nativo no fabrica una ubicación de NativeSharp.
  Está pendiente comprobar el resolvedor, los errores del controlador y ON/OFF.

Siguiente acción: completar el inventario de creaciones y los casos de prueba de
sesiones y diagnósticos, terminar los cambios de esta tanda y ejecutar una única
validación incremental agrupada. Corregir y repetir al aparecer errores.
