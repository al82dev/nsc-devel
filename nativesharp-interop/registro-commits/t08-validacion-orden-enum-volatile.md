# T08-015: validación dirigida del orden volatile de enum

NSC y la suite de sesión se construyeron incrementalmente; la suite pasó.
Layout pasó O0–O3 y comparó el IR acquire/release del caso alineado con Clang,
manteniendo los controles de alineación 1 de los ejemplos empaquetados. La
suite se construyó con 23 advertencias de inicializadores. La tanda sigue abierta.
