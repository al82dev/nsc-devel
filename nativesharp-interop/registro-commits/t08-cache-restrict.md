# Tanda 08: restrict en serializacion de sintaxis

El codec compartido guarda y restaura el indicador restrict de cada modificador,
valida el byte y avanza las versiones de cache de plantillas y de unidades
sintacticas y la clave de clones especializados C3 para no interpretar formatos
antiguos como el nuevo. Se adapta el serializador de referencia de la prueba de
equivalencia y se prepara una prueba de restauracion. Falta ejecutar las pruebas
y verificar las rutas de cache durante la bateria agrupada.
