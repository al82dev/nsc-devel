# T08: proyección de globales C atómicos no TLS

El diagnóstico `pruebas-tanda-08-atomic-global-matrix/diagnosticar.cmd` compiló
la biblioteca C y mostró que NSC rechaza los nueve globales de la matriz como
tipo C no mapeado. En `projectGlobalWithLookup`, los predicados que ya
reconocían estos tipos bajo TLS exigían `global.threadLocal`.

Se retira esa condición de los predicados de enteros, flotantes, bool y puntero;
se conservan sus requisitos previos de idioma C, tipo subyacente, tamaño y
alineación. Para los enteros estrechos y punteros sin TLS se admiten únicamente
las variantes presentes en la matriz; las demás conservan la restricción TLS.
La condición equivalente para int de 32 bits se cambió por
separado. La modificación aún no se ha construido ni validado; faltan IR,
O0–O3 y regresión TLS. Otros tipos y formas quedan fuera de esta evidencia.
No se modifica NSLink.
