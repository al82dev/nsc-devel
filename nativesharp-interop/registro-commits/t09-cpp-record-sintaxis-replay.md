# T09: observacion de sintaxis C++ simple en el catalogo vigente

## Cambio parcial

La sesion Clang observa si un struct C++ global es completo, trivialmente copiable, de layout estandar, sin bases ni atributos, con campos builtin sin atributos ni bitfields. El productor conserva esta observacion en `plainReplaySyntax`, junto a la observacion de valor existente, dentro del unico contrato NSMeta vigente. El validador admite esa combinacion y sigue rechazando una observacion C++ empaquetada; la sintaxis simple por si sola no certifica el ABI. No se introduce versionamiento historico.

## Validacion

- Compilacion incremental `build 0`: `A:/temp/t09-cpp-record-replay-build.log`.
- `nsc.cpp-interop.record-value-observation`: 1/1. Comprueba struct global simple, rechazos de atributo, herencia y namespace, ida y vuelta de la observacion y rechazo de indicadores contradictorios.
- Verificador de catalogo sobre un productor real: `A:/temp/nsc-t09-cpp-variadica-sysv-record-precompilado-4bc0nlec/estatica-O0/producer/Producer.nsmeta`; constata el record con observacion C++ y el plan base de la funcion. El consumidor sigue fallando por plan variadico no reconstruido.
- Regresion de la ruta viva C++: `A:/temp/nsc-t09-cpp-variadica-sysv-record-t7aeo8na`; regresion C SysV y rutas precompiladas C: `A:/temp/nsc-t09-sysv-varargs-zgb27tfw`.

## Pendiente

Reconstruir el plan completo del consumidor a partir del catalogo, compararlo con el plan base y la identidad original, y comprobar ejecucion con y sin encabezado. No se habilita en este cambio; T09-003 y T10-003 siguen abiertos. Campos no builtin, records en namespaces, herencia, atributos y tipos no triviales no quedan acreditados.

Actualizacion posterior: la muestra tambien funciona precompilada; vease [reconstruccion y limites](t09-cpp-variadica-sysv-record-precompilado.md).
