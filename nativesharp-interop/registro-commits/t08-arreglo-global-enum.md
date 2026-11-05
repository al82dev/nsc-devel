# Tanda 08: proyección inicial de arreglos globales enum C

- Se conserva la representación nominal y las dimensiones fijas de arreglos globales de enum sin calificar; la observación por demanda encuentra el enum a través de arreglos o punteros.
- La ruta de proyección valida el enum contra el índice importado y rechaza límites desconocidos, calificadores de arreglo y elementos enum calificados. No habilita arreglos de record ni de punteros a enum.
- Pendiente de fixture y validación agrupada. Este cambio no cierra los ítems de globales, enums o arreglos.
