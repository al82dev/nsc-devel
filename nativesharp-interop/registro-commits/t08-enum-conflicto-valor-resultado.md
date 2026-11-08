# Resultado del control de enum C incompatible

`validar-referenciados.cmd` terminó con código 0 usando `c2f36ec37b2775bf`.
Los casos positivos con y sin encabezado pasaron O0–O3. El consumidor del
header con `NHappy = 5` fue rechazado en O0 con `C/C++ type alias conflicts
with a native type 'NMood'`.

Ese mensaje no identifica el valor diferente como causa; por lo tanto, la
prueba demuestra el rechazo de esta entrada, no que el contraste de valores
sea por sí solo responsable. La procedencia física de enums y la validación
completa de la tanda siguen pendientes.
