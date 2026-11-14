# T09-003: reasignación de receptor C++ en plantillas precompiladas

Una plantilla NativeSharp que reasignaba un puntero de objeto C++ perdía el método de instancia al reconstruirse. El selector ahora admite una asignación simple usada como sentencia cuando el inicializador y todos los valores asignados proceden de fábricas C++ de cero argumentos con vinculación única y coinciden la identidad del propietario, su USR de Clang y la referencia de origen. No compara solo el nombre del método ni introduce cambios al contrato NSMeta.

## Validación

- `validar-metodo-instancia.py --precompilado`: una fábrica produce un objeto y otra de la misma referencia produce otro con valor distinto. La plantilla reasigna el receptor usando la misma fábrica o la otra; ambos resultados se comprueban con y sin encabezado, O0–O3. Pasa para biblioteca estática (`A:/temp/nsc-t09-instance-object-size-qqi7pyyj`) y DLL (`A:/temp/nsc-t09-instance-object-size-gjeqg868`). Cada configuración conserva el control Clang y las dos clases con métodos homónimos.
- En las mismas configuraciones, asignar `null` no produce una identidad de fábrica y el consumidor se rechaza, sin atribuirle un método arbitrario. También se mantienen los rechazos de firma, `.lib` y DLL incompatibles.
- La prueba de identidad comprueba rechazo de referencias y USR de record distintos, operadores compuestos, asignación por referencia, sombra y escape. Pasaron cinco controles CTest de valores y ABI y la regresión de métodos estáticos precompilados (`A:/temp/nsc-t09-size-static-method-6bzjtdno`).

## Límites

La selección sigue siendo conservadora: solo cubre locales únicos, llamadas directas por flecha y asignaciones simples como sentencia, con fábricas sin argumentos, identidad y procedencia demostradas. Los receptores que escapan, asignaciones que produzcan valores, referencias distintas, inicializadores no resueltos, fábricas con argumentos y otros flujos siguen pendientes; el diagnóstico no equivale a interoperabilidad. T09-003 y la tanda 9 permanecen abiertos.
