# Validación dirigida de campos de bits C

Se construyó incrementalmente el compilador experimental en `A:/nsc-interop-build/tanda1-ON` y se ejecutó `nativesharp-interop/pruebas-tanda-08-bitfields-c/validar.cmd`. En O0, O1, O2 y O3, código C inicializó los structs y NativeSharp leyó y modificó los campos; después C comprobó los valores. Se cubrieron campos enteros con y sin signo, separación mediante anchura cero y un struct empaquetado con alineación uno.

La prueba es dirigida y no cubre todos los campos de bits C: unions, accesos volatile y otras combinaciones continúan pendientes. La tanda 08 permanece abierta.

La suite `nsc.cpp-interop.session` pasó. Las pruebas nativas `nsc.item329.bitfield-access` y `nsc.item330.volatile-bitfields` también pasaron con `NSC_WINDOWS_RUNTIME_LIBRARY` apuntando al runtime generado en A: y `NSC_LINKER` y `NSC_LLVM_LIB` a las herramientas experimentales. El primer intento de ambas falló porque la configuración predeterminada buscó un runtime inexistente en `C:/Program Files (x86)/NativeSharp`; no se alteró esa instalación.

`validar-metadata.cmd` pasó en O0–O3 con dos consumidores de un productor precompilado: uno importa el mismo encabezado y otro infiere los tipos de la API publicada sin importar el encabezado. Ambos leen y escriben campos normales y empaquetados, y C comprueba los valores. Esto no demuestra que los nombres C se puedan escribir directamente sin importar su encabezado: el primer intento con nombres explícitos fue rechazado como identificadores desconocidos.
