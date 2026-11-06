# Rechazo de encabezado C modificado

Después de generar el productor, la prueba sustituye el encabezado en su ruta
por una versión con otro layout e intenta compilar el consumidor. Exige error de
conflicto de tipos; el caso ocurre en el directorio temporal de la prueba.

La batería terminó con código 0: el encabezado modificado fue rechazado y los
casos compatibles pasaron de O0 a O3. La suite de sesión también pasó con código 0.
