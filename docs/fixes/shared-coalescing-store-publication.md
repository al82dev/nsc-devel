# Publicación del receptor mediante `??=`

La guarda aceptaba `Alias ??= this` en constructor y destructor de un payload
de clase usado por Rc o Arc. Los cuatro casos pasaron semántica y emisión O0;
cuatro publicaciones directas se rechazaron y ocho controles inocuos emitieron.
Registro:
`A:/nsc-fixes-agente-2-evidencia/receiver-coalescing-store-20261112/run-20261112T072815598356Z/results.json`.
Una matriz adicional comprobó campo de contenedor estático y local seguido de
publicación: ocho casos publicadores y ocho inocuos superaron análisis y
emisión. Registro:
`A:/nsc-fixes-agente-2-evidencia/receiver-coalescing-store-expanded-20261112/run-20261112T073003043605Z/results.json`.
No se ejecutaron consumidores.

El MIR muestra `null-coalescing-store` con dirección estática y `this` como
fallback:
`A:/nsc-fixes-agente-2-evidencia/receiver-coalescing-store-mir-20261112/run-20261112T072857921135Z`.
La guarda ahora procesa ese nodo mediante la misma ruta de destino y valor que
un `Store`, por lo que reconoce la publicación y registra la copia local.

La compilación incremental y la regresión integrada pasaron 32 verificaciones:
16 rechazos por publicación y 16 controles seguros de global, campo y local,
para constructor/destructor y Rc/Arc. También pasaron las regresiones de
coalescencia, condicional, publicación directa y accesores binarios. Sin Core
se devolvió 77 y con ruta inválida 1. Los procesos usaron la copia principal,
temporales en A y hashes estables de compilador/Core/metadata/runtime:
`A:/nsc-fixes-agente-2-evidencia/coalescing-store-integrated-validation-20261112/run-20261112T073539281760Z/results.json`.

La regla es conservadora si el destino ya es no nulo y el fallback no se
ejecutaría, y la propagación de alias locales no distingue reasignaciones en
el tiempo. No demuestra la procedencia del resultado completo de `??=` usado
como valor, setters, otros tipos de expresiones ni seguridad general de
publicación. El compilador incluye otros cambios locales y el Core binario
era preexistente.
