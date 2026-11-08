# Validación inicial de volatile en agregados C anónimos

El fixture de agregados anónimos pasó O0–O3 con acceso directo, dos sesiones,
Shared y Metadata. NativeSharp escribió y leyó los campos `volatile int` natural
y empaquetado; C comprobó sus valores.

La inspección del IR sin optimizar en directo y Metadata mostró carga acquire y
almacenamiento release atómicos volátiles con alineación 4 para el campo natural,
y carga y almacenamiento volatile simples con alineación 1 para `pack(1)`.
No se comparó automáticamente este caso con IR de Clang ni se inspeccionaron
todos los niveles de optimización. La validación completa de la tanda sigue pendiente.
