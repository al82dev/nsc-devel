# Prueba de metadata con campos de bits C volatile

El productor publica structs normal y empaquetado y una union con campos volatile. Los consumidores prueban acceso con encabezado compartido y con tipos inferidos de la biblioteca precompilada en O0–O3. La prueba debe ejecutarse antes de atribuir compatibilidad.

Ambos consumidores ejecutaron correctamente O0–O3. Se añadió el volcado IR para verificar por separado que el cualificador volatile llegue al código del consumidor; el resultado numérico por sí solo no comprueba ese requisito.

El control automático pasó para ambos consumidores: confirmó la secuencia esperada de cargas y escrituras volatile i32 con alineación 4 o 1 en los structs y la union del fixture. `nsc.cpp-interop.session` también pasó. No se afirma cobertura de otros tipos, unidades de almacenamiento o destinos.
