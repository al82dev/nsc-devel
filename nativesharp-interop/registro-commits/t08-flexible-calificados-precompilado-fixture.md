# T08: conversiones flexibles calificadas precompiladas

El productor pasa el miembro `volatile` a una función C calificada. El
consumidor obtiene desde el productor un record con miembro `const`, lo
convierte a `int const*` y observa también el resultado volatile del productor.
Se preparan rutas con encabezado compartido y solo metadata, sin validar aún.
