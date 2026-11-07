# T08: comparación de operación compuesta en puntero atómico

La primera ejecución del fixture falló al compilar `CompoundProbe.c`: Clang
rechazó `value_pointer += 1` para `_Atomic(int*)` en C11. Por ello no es una
referencia C válida para esa expresión. El control de Clang conserva los siete
operandos numéricos; la operación de puntero de NativeSharp se comprueba por
separado en su IR y mediante el valor observado desde C. La corrección del
fixture está preparada sin volver a ejecutar; no modifica producción.
