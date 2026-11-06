# Callback C que retorna puntero a record

El fixture prepara `CallbackPayload* (*)(void)` para tres fuentes: un callback
retornado por una función C, uno almacenado en campo C y otro en global C.
NativeSharp los invoca y consulta el valor del record. El caso está preparado
en la matriz de callbacks O0–O3.

Con NSC build `3f82235fd35e7b9a`, `validar.cmd` terminó con código 0 y
compiló, enlazó y ejecutó las tres rutas en O0, O1, O2 y O3. Esto cubre la
firma `CallbackPayload* (*)(void)` del fixture, no records por valor ni otras
convenciones o combinaciones.
