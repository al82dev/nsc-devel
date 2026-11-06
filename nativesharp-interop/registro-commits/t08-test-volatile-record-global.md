# Prueba de struct global volatile

El proyecto prepara lectura y escritura del campo de un struct global volatile,
contrastadas con C y con las instrucciones LLVM, y rechaza su dirección como
puntero mutable. Un puntero global volatile a struct se usa para verificar que
su calificador no se transfiera al campo apuntado. Pendiente de validación.
