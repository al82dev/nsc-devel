# T08: descenso LLVM del puntero atómico global a float

Se agrega la clave `float*` a los accesos atómicos de puntero existentes en
LLVM: carga, escritura y suma/resta compuesta, con almacenamiento de ocho bytes
y avance de cuatro bytes. El cambio está preparado sin build ni validación; no
demuestra ejecución ni cubre apuntados calificados o TLS. NSLink y LLVM externo
permanecen intactos.
