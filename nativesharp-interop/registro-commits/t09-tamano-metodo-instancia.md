# T09-003: tamaño oculto en métodos C++ de instancia

En Windows x64, Clang coloca el receptor `this` antes de los parámetros declarados. Para un método con `pass_object_size` seguido de `bool`, la firma observada es `(ptr this, ptr, i64, i1 zeroext)`. La importación de NSC rechazaba ese método. Ahora conserva el atributo, calcula el tamaño sin reevaluar el argumento e inserta el tamaño después del puntero; el atributo del booleano se aplica a su posición física. El índice constante de un subobjeto se conserva en las rutas HIR de llamadas habituales e iterativas.

## Validación

- `validar-metodo-instancia.py`: ocho métodos con `pass_object_size` y `pass_dynamic_object_size`, modos 0–3, receptor `const`, argumento `bool` y subobjeto con índice constante. Controles Clang y NativeSharp pasaron O0–O3 en biblioteca estática (`A:/temp/nsc-t09-instance-object-size-jdtn52xa`) y DLL (`A:/temp/nsc-t09-instance-object-size-6m5uxpiu`). Las declaraciones y llamadas LLVM de la muestra conservan el `zeroext`.
- La prueba de métodos estáticos y consumidores precompilados pasó, incluidos sus tres rechazos (`A:/temp/nsc-t09-size-static-method-_uvuzwbv`). La regresión C directa pasó en O0–O3 con biblioteca estática y DLL (`A:/temp/nsc-t09-c-object-size-kqbptexo`). Cinco controles seleccionados de tipos de valor y ABI pasaron.
- La prueba con `--precompilado` registra una limitación real y actualmente falla: el método de instancia no se reconstruye al consumir una plantilla genérica, con encabezado (`A:/temp/nsc-t09-instance-object-size-5qht0y_s/O0-con-encabezado-build.log`) o sin él (`A:/temp/t09-instance-pre-noheader-987cdfa34d1e40d18138310ef80e9bcb/resultado.log`).

## Límites

La muestra no certifica rutas genéricas precompiladas, constructores, métodos virtuales, otras convenciones, todos los tipos de parámetros ni efectos laterales de índices. La extracción del cálculo compartido preserva el cuerpo usado por las llamadas estáticas; las formas no incluidas en estas pruebas requieren validación propia. T09-003 y la tanda 9 continúan abiertos.
