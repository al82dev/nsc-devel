# T08-010 / T08-012 / T08-014: campo puntero C empaquetado

Se permite proyectar un campo puntero C cuando el almacenamiento del puntero
está subalineado, conservando su alineación efectiva para el acceso. El análisis
impide que la dirección del propio campo escape como `T**`; esa protección no
califica ni cambia el objeto apuntado. Permanecen las restricciones existentes
para tipos apuntados y para C++, agregados y bitfields. Falta comprobarlo con
el fixture mixto y la validación de la tanda.
