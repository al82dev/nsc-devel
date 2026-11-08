# T08 H3: límites del ámbito de declaraciones

Se agrega un control del filtro de `catch` y otro independiente en el que el
inicializador de una desestructuración llama a la función C que tendrá el mismo
nombre que una variable declarada por el patrón. El segundo consumidor exige
el resultado correcto y que la biblioteca conserve el símbolo inline C.

Con NSC build `1965527e723a8dbb`, el diagnóstico agrupado terminó con código 0
en O0–O3. El filtro se compiló y no capturó la global homónima; el camino que
lanza y evalúa el filtro no se ejecutó. El consumidor del productor independiente
obtuvo 83 y se comprobó que el símbolo inline C estaba presente. Esto no cierra
la tanda ni sustituye su validación completa.
