# Procedencia HIR del global C volatile int*

Para una desreferencia cuyo operando es directamente un global importado de C,
el HIR conserva el modo MS y la alineación natural de un int de 32 bits. La
marca solo se asigna si el tipo, el calificador y la procedencia coinciden;
parámetros, locales, casts, miembros y otros tipos no entran en esta ruta.
La emisión y la validación agrupada aún están pendientes.
