# Typedef C en un proyecto NativeSharp real

Se prepara una biblioteca C y un proyecto NativeSharp con alias encadenados,
matriz fija, enum, struct y callback, para validar compilación, enlace y
ejecución de O0 a O3. El caso aún no se ha ejecutado y no acredita soporte.

Con `f3f7271196c4666b` el primer caso se detuvo en O0: la función con
parámetro `const CMatrix*` no se importó (`unknown identifier`). Se aisló
esa firma pendiente y el caso principal pasa los elementos como escalares
para poder evaluar los otros alias. El código de salida del lanzador fue cero
pese al error, por lo que el resultado requiere el marcador final explícito.
