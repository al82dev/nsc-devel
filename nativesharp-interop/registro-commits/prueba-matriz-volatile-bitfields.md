# Matriz de campos de bits C volatile

La prueba cubre unidades de 8, 16, 32 y 64 bits, una barrera de ancho cero, lectura e incremento. C inicializa y comprueba resultados y se compara el IR con Clang. El fixture debe ejecutarse antes de afirmar soporte.

La primera comparación encontró dos diferencias: para el campo de 8 bits al inicio de un struct alineado a ocho, Clang anota alineación 8 y NSC 1; y para cada incremento Clang emite dos cargas volatile mientras NSC emite una. La segunda diferencia es observable para memoria de dispositivo. El control informa todos los casos y falla mientras difieran; el script ejecuta primero las cuatro optimizaciones para no perder evidencia funcional. No se ha decidido que el operador NativeSharp deba copiar la cantidad de accesos de C, cuyo contrato requiere análisis adicional.

El fixture compiló, enlazó y produjo el resultado numérico esperado en O0, O1, O2 y O3. Después el control IR terminó con error y enumeró las cinco discrepancias. La anotación de alineación menor es conservadora en esta muestra; no demuestra por sí misma una lectura incorrecta. La diferencia de cantidad de lecturas sí puede cambiar efectos observables y no queda resuelta por el resultado numérico.

Tras la recarga para campos C importados, la matriz ampliada pasó O0–O3 y el control IR verificó anchos y secuencias de lectura/escritura frente a Clang, incluyendo prefijo, postfijo y asignación compuesta. La anotación i8 de NSC sigue siendo 1 frente a 8 en Clang y el control la acepta como garantía conservadora, sin declarar identidad de IR. Pasaron también los fixtures de volatile directo y metadata, union y metadata no volatile, la sesión Clang y las pruebas nativas 329 y 330 con el runtime experimental de A:.
