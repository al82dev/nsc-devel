# Alias C publicado con encabezado compartido y conflicto

El control agrega un consumidor que incluye el mismo encabezado que el
productor y otro que declara `NAlias` como entero en vez de record. El primero
debe ejecutar O0–O3; el segundo debe rechazar el conflicto. Aún falta ejecutar
ambos casos; se conserva el consumidor sin encabezado.
