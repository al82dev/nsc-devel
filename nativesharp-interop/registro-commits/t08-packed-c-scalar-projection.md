# T08-010 / T08-012: proyección de campos escalares C empaquetados

El proyector calcula la alineación garantizada por la alineación del record y
el offset de Clang, limitada por la alineación natural del campo. Permite una
alineación reducida para escalares C booleanos, enteros y flotantes y la pasa
al modelo. Mantiene el rechazo de C++, punteros, agregados y bitfields cuando
la alineación sería insuficiente. Las direcciones escalares desalineadas se
mantienen protegidas por el análisis de miembros. Falta verificar el conjunto
con Clang y el proyecto mixto antes de considerarlo funcional.
