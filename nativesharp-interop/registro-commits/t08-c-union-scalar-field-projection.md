# Tanda 08: proyección inicial de campos de union C

Se permite proyectar un campo de union analizada como C mediante las mismas
comprobaciones de tamaño, alineación, offset, tipo y calificadores que para un
struct. Las unions C++ conservan la barrera previa, al tener reglas de vida de
miembros distintas. Esta proyección no acredita almacenamiento propio, ABI por
valor, campos de bits ni ejecución; todo ello permanece pendiente de prueba.
