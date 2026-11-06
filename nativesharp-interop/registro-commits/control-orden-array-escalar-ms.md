# Controles de arreglos C volatile escalares

NativeSharp escribe y lee arreglos enteros y flotantes; una función C verifica
los valores. El script exige acquire/release y alineación por ancho en el IR.
Los casos están preparados para la validación agrupada, todavía no ejecutada.

La prueba de alineación también cubre ushort y double multidimensionales,
rechaza una raíz con tipo distinto y mantiene bool fuera de esta ruta.
