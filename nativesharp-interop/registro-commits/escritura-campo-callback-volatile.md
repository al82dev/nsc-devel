# Escritura de campo callback volatile

Se prepara `release` en las escrituras de campo con modo MS y alineación
efectiva acreditada; el camino de lectura de una operación compuesta usa
`acquire`. Los accesos sin alineación suficiente conservan volatile ordinario.
Falta construcción y comparación de IR con Clang.
