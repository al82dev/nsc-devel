# Validación de carriers y dependencias C combinados

Se construyó incrementalmente `nsc_cpp_c_identity_tests` y la prueba terminó con
código 0, usando TEMP y TMP en A:. Pasaron el round trip conjunto de carriers y
enumerador C y los rechazos de índices duplicados, fuera de rango y bytes sobrantes.

El fixture real terminó con código 0 en O0–O3: una biblioteca publica records con
miembros anónimos y una plantilla que usa un enumerador C; los consumidores con
encabezado y sin referencia explícita al encabezado la instanciaron y usaron los
records. El archivo del encabezado permanecía presente en el directorio de prueba,
por lo que esto no demuestra independencia física de dicho archivo.

La primera versión del fixture usaba un struct como dueño de la plantilla y fue
rechazada por la publicación de metadata; se corrigió el fixture a clase. La tanda 8
y su batería completa siguen pendientes.
