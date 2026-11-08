# Tanda 8: ABI bool para callbacks y llamadas indirectas C

El thunk de callback y las llamadas mediante `extern C delegate` aplican `zeroext` al retorno y a los argumentos booleanos pasados por valor. Se comprueba el tipo físico LLVM para no atribuirlo a punteros o parámetros por referencia. También se aplica al reenvío de importaciones C resueltas dinámicamente.

La construcción incremental de NSC pasó. El control cruzado de puntero C invocado desde NativeSharp y callback NativeSharp invocado desde C comparó IR con Clang y pasó O0–O3: `A:\nsc-interop-build\tanda08-bool-callback-c-15782-5564`. Las regresiones de llamadas directas C y C++ pasaron O0–O3 en `A:\nsc-interop-build\tanda08-bool-abi-c-15817-25493` y `A:\nsc-interop-build\tanda08-bool-abi-cpp-15821-3473`.

El reenvío dinámico está ajustado pero no tiene una prueba específica en este cambio. Los demás tipos, variantes de callbacks y la batería completa de la tanda siguen pendientes.
