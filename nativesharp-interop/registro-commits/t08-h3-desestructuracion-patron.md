# T08 H3: identificadores del patrón de desestructuración

Después de `ef1ab4cbe`, el control continuó detectando la global C en O0–O3.
Además de los usos posteriores, el recorrido recoge como candidato el propio
identificador del patrón de desestructuración. Se descarta cuando pertenece al
patrón de declaración, sin descartar referencias dentro del inicializador.

La corrección requiere una nueva compilación incremental y validación; este
registro no declara aprobada la prueba.
