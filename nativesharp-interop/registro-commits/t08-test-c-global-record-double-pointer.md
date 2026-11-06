# Puntero global doble a struct C

El fixture prepara la lectura de un global `struct GlobalOnly**` y de un campo
después de dos indirecciones. Comparte el objeto del caso de puntero sencillo,
por lo que también comprueba identidad y almacenamiento compartidos. Pendiente
de ejecución agrupada; no cubre calificadores de apuntado.
