# T08-002: reproducción de H8 (catálogo 1.42 solo de enums)

Se añaden el fixture mínimo `catalogo/legacy-enum-only.hex`, seis productores
reales (`catalogo/PCat*`) que cubren las envolturas 1 a 4, y el modo
`--catalogue-version-controls`. El modo reescribe el catálogo de cada productor
en cada minor 1.42–1.45 que puede representarlo, con el escritor de versiones
anteriores existente, y lo relee con el lector actual y con una réplica de la
regla propuesta (formato elegido por el minor declarado).

El lector actual rechaza solo la variante 1.42 solo de enums, con `metadata
before 1.43 cannot represent C source provenance`. La regla propuesta decodifica
las catorce variantes. En `validar.cmd` se añade la sección `catalogo`. El enum
sintético de la sonda 7 pasa a una función común.

`T08-002-H8-CATALOGO.md` documenta qué bytes mira cada lector, la propuesta y los
archivos afectados. El codec y el formato no cambian.
