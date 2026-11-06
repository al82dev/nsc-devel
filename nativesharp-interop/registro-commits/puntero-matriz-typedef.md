# Puntero a matriz declarada mediante typedef

El consumidor conserva el caso real `const CMatrix*` que no se importó en O0.
El script separado exige compilación, enlace y ejecución O0–O3 y su marcador
final; no considera éxito el rechazo ni el código de salida del lanzador aislado.
Pendiente diagnosticar y resolver la proyección del tipo.

Repetido con la biblioteca y el consumidor separados sobre `f3f7271196c4666b`:
se detiene en O0 con `unknown identifier 'c_matrix_ptr'`; no llega al marcador
final. `mapFunctionType` no proyecta el tipo array apuntado. Faltan diseño de
representación, validación de calificadores y comprobación de llamadas.
