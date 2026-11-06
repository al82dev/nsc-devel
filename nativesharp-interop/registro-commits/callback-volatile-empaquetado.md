# Callback volatile en campo empaquetado

Se prepara un puntero a función volatile en un campo con `pack(1)` y offset
uno, con lectura y escritura cruzadas. El caso permitirá comprobar que no se
emitan accesos atómicos suponiendo alineación ocho. Falta validar ejecución e
IR frente a Clang.
