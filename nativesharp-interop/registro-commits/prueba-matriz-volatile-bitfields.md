# Matriz de campos de bits C volatile

La prueba cubre unidades de 8, 16, 32 y 64 bits, una barrera de ancho cero, lectura e incremento. C inicializa y comprueba resultados y se compara el IR con Clang. El fixture debe ejecutarse antes de afirmar soporte.

La primera comparación encontró dos diferencias: para el campo de 8 bits al inicio de un struct alineado a ocho, Clang anota alineación 8 y NSC 1; y para cada incremento Clang emite dos cargas volatile mientras NSC emite una. La segunda diferencia es observable para memoria de dispositivo. El control informa todos los casos y falla mientras difieran; el script ejecuta primero las cuatro optimizaciones para no perder evidencia funcional. No se ha decidido que el operador NativeSharp deba copiar la cantidad de accesos de C, cuyo contrato requiere análisis adicional.
