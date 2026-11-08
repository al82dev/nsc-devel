# T08 H4: enum anónimo de campo precompilado en O0–O3

El validador anterior esperaba que la publicación del record con enum anónimo fallara. Ahora construye la biblioteca y un consumidor que usa únicamente su metadata, lee y escribe ambos campos del enum y contrasta el resultado con una función C de la misma biblioteca.

El control terminó con código 0 en O0, O1, O2 y O3. También pasaron la referencia C y los controles de rechazo para el enumerador C++ y para la escritura del campo `const`. Registro: `A:/nsc-interop-build/tanda08-enum-campo-actualizado.log`. La validación completa de la tanda sigue pendiente.
