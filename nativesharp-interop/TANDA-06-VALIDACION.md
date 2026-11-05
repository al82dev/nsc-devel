# Tanda 06: validación de sesiones

Estado: implementación agrupada registrada; validación en curso.

- La suite de sesiones está registrada en CTest cuando se habilita
  `NSC_CPP_INTEROP_SESSION_TESTS`, incluso con `NSC_BUILD_TESTS=OFF`. Usa el
  Clang configurado y el directorio de build como raíz de temporales; en esta
  ejecución se usa `A:/nsc-interop-build/tanda1-ON`.
- La prueba de paquete incompatible usa un paquete simulado Clang 22.1.0 y una
  búsqueda CMake `EXACT` de la versión configurada. Comprueba resolución de
  paquetes, no verifica la versión de un binario externo en tiempo de ejecución.
- Las pruebas existentes comparan identidades de opciones, aíslan sesiones con
  macros distintas y cubren cancelación. Se amplió el caso de cancelación para
  observar la destrucción del sistema de archivos usado por el análisis.
- La prueba de inicio usa una acción inyectada que rechaza realmente
  `BeginSourceFileAction` y verifica que no se invoque `EndSourceFileAction`.
- `pruebas-tanda-06/validar.cmd` compila y ejecuta el mismo programa en ON/OFF
  y guarda la inspección PE en `A:/nsc-interop-build/tanda06`. La implementación
  de LLVM 23.1.0 consultada en `llvm/tools/llvm-readobj/COFFDumper.cpp` procesa
  importaciones normales y diferidas con `--coff-imports`.
- Construcción agrupada OFF terminada con código 0; construcción ON y ejecución
  de CTest y del escenario de ejecutables todavía pendientes de resultado.
