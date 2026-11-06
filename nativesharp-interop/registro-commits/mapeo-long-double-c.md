# Mapeo condicionado de long double

El proyector escalar y el de typedef preparan `long double` como `double`
solamente si Clang informa semántica IEEE binary64 y tamaño de 64 bits. El
resto de formatos sigue rechazado. Falta construir y ejecutar el fixture; el
atributo capturado aún no se conserva en catálogos precompilados.
