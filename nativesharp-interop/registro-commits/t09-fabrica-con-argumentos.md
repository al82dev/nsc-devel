# T09-003: fábrica C++ con argumentos en una plantilla precompilada

Una plantilla que inicializaba o reasignaba el receptor de un método C++ mediante `makeWith(int)` perdía el método al consumirse. El selector ahora acepta fábricas C++ no variádicas con cantidad exacta de argumentos cuando el vínculo tiene un único candidato y el tipo retornado, su USR y la referencia se verifican. El consumidor sigue comprobando los tipos y la ejecución de los argumentos; no se modificó NSMeta.

## Validación

- `validar-metodo-instancia.py --precompilado`: una fábrica con argumento selecciona un objeto con valor distinto. La plantilla la usa como inicializador y como reasignación, con y sin encabezado, O0–O3. Pasan el control C++ y NativeSharp con biblioteca estática (`A:/temp/nsc-t09-instance-object-size-2s7g_4i_`) y DLL (`A:/temp/nsc-t09-instance-object-size-dd78g6xj`).
- Una llamada dentro del argumento incrementa un contador; la prueba verifica que se ejecuta una sola vez en control C++, llamada directa y plantilla, en las mismas configuraciones.
- Un argumento `null` para `int` se rechaza al instanciar la plantilla con y sin encabezado. En esta revisión inicial una fábrica sobrecargada con retornos distintos era rechazada, sin atribuirle un propietario arbitrario. La prueba de identidad con Clang real comprobó la fábrica única y el caso sobrecargado; una ampliación posterior se describe abajo.
- Pasaron cinco controles CTest seleccionados de valores y ABI, y la regresión de métodos estáticos precompilados (`A:/temp/nsc-t09-size-static-method-pcf_s3xw`).

## Límites

La muestra inicial acredita un parámetro entero y una expresión con efecto lateral, con candidato único, aridad exacta y fábrica no variádica. Una [ampliación posterior](t09-fabricas-sobrecargadas.md) admite una muestra de sobrecargas con propietarios distintos y conserva el rechazo de un empate. Otras sobrecargas, fábricas variádicas, valores predeterminados y receptores escapados requieren validación propia. Un rechazo seguro no constituye interoperabilidad. T09-003 y la tanda 9 permanecen abiertos.
