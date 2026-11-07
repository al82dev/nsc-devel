# T08: proyección de variantes atómicas C globales adicionales

El diagnóstico `diagnosticar-extra.cmd` compiló el C y NSC rechazó los dos
enteros estrechos y los seis punteros de la muestra como tipo no mapeado. Los
predicados de `projectGlobalWithLookup` limitaban esas variantes a TLS o a la
muestra previamente validada.

Se habilitan los dos enteros estrechos restantes y los punteros sin calificar a
los tipos ya reconocidos en TLS, además de `const int*` y `volatile int*`. Se
mantienen los requisitos previos de tipo C, tamaño y alineación y la restricción
para otros apuntados calificados. El cambio no se ha construido ni validado;
faltan el control cruzado, los rechazos de pérdida de calificadores, IR y la
regresión TLS. NSLink no se modifica.
