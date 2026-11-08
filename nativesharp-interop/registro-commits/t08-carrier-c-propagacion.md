# Propagación de la marca de carrier C

Al convertir campos C al modelo de miembros, las rutas de encabezado y de
reconstrucción precompilada transfieren la marca `isAnonymousCarrier`. El frontend
puede usarla para activar la promoción y ocultar el nombre interno.

El inventario todavía no materializa los agregados, y el formato de metadata aún
no conserva esta marca. Por tanto esta propagación no habilita el consumo de
miembros anónimos; la tanda 8 y su validación completa siguen pendientes.
