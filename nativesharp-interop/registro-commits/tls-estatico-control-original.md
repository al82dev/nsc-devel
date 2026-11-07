# Control original de TLS estático con LLD

Se ejecutó `nativesharp-interop\pruebas-tanda-08-tls-c\validar.cmd` con
NSC build `b151b46f0be04227`, el compilador y LLD de `A:/al82dev-clang`, y
las bibliotecas declaradas por el proyecto en `A:/vc2026-runtime`. El script
selecciona explícitamente LLD mediante `NSC_LINKER` y la prueba de aislamiento
TLS entre dos hilos pasó en O0, O1, O2 y O3; salida y proceso terminaron con
código 0. El log de NSC está en `A:/nsc-interop-build/tanda08-tls-c/link-O0.log`.

Esta evidencia confirma ese fixture; no prueba detección ni selección
automática del enlazador y no cierra los demás fallos de TLS ni la tanda 8.
No se modificaron el código de NSLink ni los fixtures existentes.

Como contraste, se volvió a invocar el mismo proyecto en O0 con `NSC_LINKER`
apuntando expresamente a `A:/nsc-interop-build/cpp-fixtures/command-cpp/nslink.exe`.
Con la misma versión de NSC el enlace falló con `NSL1023`: las copias de
`_guard_dispatch_icall` de `libucrt.lib` y `libcmt.lib` discrepan según la
regla COMDAT `NODUPLICATES`. Esta reproducción no señala `_tls_index` como el
primer error y no justifica modificar NSLink. La ruta LLD sigue funcionando.
