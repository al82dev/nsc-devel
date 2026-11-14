# T09-003 y T11: acceso a un vector C++ empaquetado

Un record C++ trivial puede tener un vector `float4` en offset 1 mediante `packed`. Clang ejecutaba la muestra, pero NSC no proyectaba el campo por su alineación reducida. La proyección ahora admite el vector con la alineación efectiva del campo; el acceso a cada componente usa la alineación propagada desde la dirección del record. La modificación no cambia la regla de otros campos C++.

El fixture `validar-packed.py` contrasta el layout con Clang, comprueba los cuatro componentes y que un byte vecino permanece intacto, y prueba retorno y callback. Pasaron ocho comparaciones C++/NativeSharp en estática y DLL, O0–O3 (`A:/temp/nsc-t09-preserve-cpp-retorno-record-vector-fdsys0xn`), y 16 consumidores precompilados con y sin encabezado (`A:/temp/nsc-t09-preserve-cpp-retorno-record-vector-precompilado-9duix4h0`). Verifica que las lecturas y escrituras de los cuatro componentes tienen `align 1` en el IR directo y en un consumidor precompilado sin encabezado; el registro está en `A:/temp/nsc-t09-cpp-vector-packed-nlio1mkv`.

Se conserva el rechazo en vivo del campo vectorial volatile. El control negativo precompilado usa ahora un vector `_Float16` aún no admitido, conservando la verificación de rechazo tras reconstruir metadata. Pasaron las pruebas dirigidas de planes ABI (`A:/temp/t09-scoped-enum-unit-tests.log`) y las matrices de regresión de `double3` (`A:/temp/nsc-t09-cpp-vector3-5kv0lio9`) y record alineado a 64 bytes (`A:/temp/nsc-t09-cpp-vector-align64-5qe6wx20`).

El resultado acredita el caso y la alineación comprobados en Win64; no acredita todos los layouts packed, tipos vectoriales, operaciones SIMD ni vectores volatile. T09-003 sigue abierto.
