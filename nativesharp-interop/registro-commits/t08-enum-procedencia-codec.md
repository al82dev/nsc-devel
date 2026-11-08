# Codec preparado para procedencia de enums

Se prepara un envoltorio versión 7 alrededor del catálogo existente, con un
indicador y ruta, SHA-256 y desplazamiento por enum. Se conserva la base con
carriers y dependencias cuando corresponde; el lector verifica límites,
conteo y recodificación canónica antes de entregar datos. Aún no se conecta a
NSMETA ni al despacho de versiones y falta validación agrupada.
