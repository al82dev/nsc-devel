# Modelo volatile en codec de records C

El codec interno representa los tres estados del modelo de volatile: no
conocido, desactivado y activado. Rechaza otros valores al leer. El formato
NSMETA todavía no utiliza este codec y la validación agrupada está pendiente.
