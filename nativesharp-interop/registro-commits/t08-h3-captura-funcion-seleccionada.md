# T08 H3: captura preparada de funcion C seleccionada

Se prepara una funcion interna para construir la instantanea de una funcion C ya seleccionada a partir de su observacion de Clang. Exige coincidencia de proyecto, referencia, destino, USR, simbolo, identidad canonica y proyeccion; no confunde candidatos especulativos con una seleccion. La salida permanece intacta si la observacion falta o es ambigua.

Los controles sinteticos preparados cubren coincidencia, referencia equivocada y observaciones multiples. Aun no se han compilado ni ejecutado. La funcion todavia no esta conectada al recolector de plantillas: sigue pendiente obtener selecciones reales con sombras y sobrecargas, capturar globales y aliases, determinar procedencia estable y conservar el enlace de definiciones internas. No se modifica NSMETA ni se da por cerrada la tanda.
