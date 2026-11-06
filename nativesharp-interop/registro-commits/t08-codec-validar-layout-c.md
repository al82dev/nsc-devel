# Validación inicial de layouts C del codec

El codec rechaza identidades duplicadas o desordenadas, layouts presentes en
records incompletos, dimensiones completas incoherentes y offsets o anchuras de
campo fuera del record. Valida tanto al escribir como al leer y conserva la
salida anterior ante error. Es una comprobación inicial; no certifica el ABI ni
la integridad semántica completa de los árboles de tipos.
