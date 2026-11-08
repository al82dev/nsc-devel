# Invocación variádica mediante typedef con record C

Se amplió el control de dos DLL TLS homónimas. Cada DLL devuelve un puntero a
su propio record `Payload` y exporta una función variádica que lee un campo del
record, su estado TLS y seis argumentos enteros. Las dos plantillas NativeSharp
precompiladas asignan esa función al mismo typedef `VarFn` y la invocan con el
puntero devuelto por su DLL. Controles C comprueban los mismos argumentos.

Pasaron O0–O3 y la restauración desde caché, con los dos encabezados borrados
antes de compilar el consumidor. Los resultados distinguen ambas DLL. La prueba
quedó incorporada al validador del caso de typedef con record. No se modificó
el compilador para esta ampliación ni se ejecutó la batería completa de la tanda.
No se han acreditado records anidados, retornos por valor ni todas las variantes.
