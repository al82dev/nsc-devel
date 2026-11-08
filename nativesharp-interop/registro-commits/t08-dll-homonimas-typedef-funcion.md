# Typedefs de puntero de función en productores distintos

El lookup rechazaba alias de proyectos distintos incluso cuando ambos describían
el mismo puntero de función C. Ahora concilia la forma que puede proyectar a una
firma de escalares y punteros a escalares, siempre que coincidan la firma, la
convención, la variadicidad, el triple y la representación canónica. Se conservan
las comprobaciones existentes de nombre e identidad del typedef. No se amplió la
regla a firmas con records o enums.

La prueba usa dos DLL TLS homónimas y dos productores precompilados que publican
el mismo typedef variádico: pasó O0–O3, incluida la restauración desde caché y la
llamada a ambas funciones. Otra muestra cambia el primer parámetro de un typedef
de `int` a `long long`; NSC rechaza el conflicto en O0. Ambos controles se agregan
al validador de optimizaciones. No se ejecutó la batería completa de la tanda 8.
