# Tanda 07: corrección de prueba de mismo tamaño

El primer header se escribía en modo binario y el reemplazo en modo texto. En
Windows, el salto de línea del reemplazo podía ocupar dos bytes y rompía la
condición de igual tamaño. Ambos se escriben ahora en binario; falta repetir la
validación agrupada.
