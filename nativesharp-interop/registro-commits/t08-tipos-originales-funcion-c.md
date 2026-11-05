# Tanda 08: tipos originales de funciones C/C++

El simbolo semantico importado conserva el tipo capturado por Clang del
resultado y de cada parametro, incluidos calificadores anidados. La clave de
representacion y el ABI siguen siendo los existentes: este metadato por si solo
no autoriza emitir `noalias` ni demuestra obligaciones entre argumentos.
Pendiente probar captura, proyeccion y efecto durante validacion agrupada.
