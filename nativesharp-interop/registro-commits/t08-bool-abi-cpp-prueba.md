# Tanda 8: ABI de bool en funciones y métodos C++

La prueba importa una función C++ libre, un método estático y un método de instancia. El control contrasta contra Clang el `zeroext` en retorno y argumento, incluida la posición del parámetro después de `this` y entre enteros. El consumidor ejecutó correctamente O0–O3; evidencia en `A:\nsc-interop-build\tanda08-bool-abi-cpp-14995-3871`.

El control solo acredita estas llamadas directas. Continúan pendientes llamadas indirectas y callbacks, entre otros casos, y la tanda no queda cerrada.
