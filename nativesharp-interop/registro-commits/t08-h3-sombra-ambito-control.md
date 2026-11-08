# T08 H3: ejecución del control de ámbitos

El control construye el productor y el consumidor en un directorio de trabajo
independiente, verifica la metadata y ejecuta el consumidor en O0, O1, O2 y O3.
El resultado observable debe ser 179: la función C debe seguir disponible antes
y después del bloque que contiene una variable local homónima.

El fixture y este control se agregan antes de corregir el importador para evitar
suprimir dependencias legítimas al resolver referencias sombreadas. Los resultados
se registrarán con la compilación concreta utilizada.
