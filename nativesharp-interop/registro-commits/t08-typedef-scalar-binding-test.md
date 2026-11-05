# Tanda 08: prueba integrada de typedef escalar

La prueba preparada entrega al frontend el `typedef unsigned int Count`
capturado por Clang, analiza un archivo NativeSharp que lo utiliza y comprueba
que el tipo enlazado se expande a `uint`. Usa un proveedor explícito de prueba;
no afirma todavía que el driver de producción lo instale ni cubre tipos
compuestos. La ejecución corresponde a la validación agrupada de la tanda.
