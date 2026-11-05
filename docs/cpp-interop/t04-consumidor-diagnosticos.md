# Consumidor de diagnósticos Clang

Se separa el consumidor que copia severidad, mensaje, identificador, ubicación y
notas al contrato de la sesión. El vector sigue siendo propiedad de la sesión y
el consumidor debe sobrevivir al motor de diagnósticos que lo usa.

La conexión y validación se completan con el conjunto de la tanda 04.
