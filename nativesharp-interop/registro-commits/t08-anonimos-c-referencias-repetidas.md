# Comparación de agregados C anónimos entre sesiones

El control con dos encabezados que incluyen el mismo archivo falló porque Clang
no puede buscar los nombres internos sintetizados de los agregados anónimos.
Estos registros conservan ahora la procedencia del propietario. El refinamiento
admite el grupo solo si todas las observaciones son internas C, tienen el mismo
USR conocido, archivo equivalente, hash y offset de origen, dimensiones y
contrato de campos. Sin esas pruebas el grupo sigue sin verificarse.

El cambio está preparado pero aún no se ha construido ni ejecutado. La tanda 8
permanece abierta.
