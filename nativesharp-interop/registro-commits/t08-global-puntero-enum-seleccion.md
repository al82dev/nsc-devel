# Tanda 08: seleccionar enum por puntero global

- La observación de un global demandado recorre hasta 64 niveles de punteros para encontrar el enum subyacente y seleccionar su candidato nominal. Se conserva el tratamiento diferido de conflictos y no se modifican las reglas de proyección de calificadores.
- El proyecto mixto prepara lecturas de puntero y doble puntero cuyos enums no aparecen en firmas ni nombres escritos por NativeSharp. Pendiente de validación agrupada; no cubre arreglos, todas las colisiones o calificadores.
