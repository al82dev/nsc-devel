# Validación dirigida de matriz C empaquetada

En `2343368d8` las construcciones incrementales de NSC y de la suite de sesión
terminaron con código 0, al igual que la suite y el proyecto layout en O0–O3.
El fixture verificó `int[2][3]` bajo `pack(1)`, comparación con Clang, IR O0
con alineación 1 y rechazo de dirección. El cálculo interno de strides también
pasó. Se observaron advertencias existentes. La tanda 08 continúa abierta.
