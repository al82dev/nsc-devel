# Publicación mediante el valor de `??=`

Una clase payload publicaba su receptor mediante
`Payload local = null; Alias = (local ??= this);`. Otra forma inicializaba el
local con `this` y usaba `Other` como fallback. Las cuatro combinaciones de
Rc/Arc y constructor/destructor pasaron análisis y emisión para cada forma.
La variante con copia del resultado a un segundo local también se aceptó en
cuatro combinaciones. Los controles sin publicación emitieron, mientras la
forma de dos sentencias `local ??= this; Alias = local;` se rechazó. Ningún
consumidor se ejecutó. Evidencia:
`A:/nsc-fixes-agente-2-evidencia/receiver-coalescing-store-result-20261112/run-20261112T075925575645Z/results.json`,
`A:/nsc-fixes-agente-2-evidencia/receiver-coalescing-store-prior-20261112/run-20261112T080051932981Z/results.json`
y `A:/nsc-fixes-agente-2-evidencia/receiver-coalescing-store-alias-20261112/run-20261112T080303348285Z/results.json`.

El MIR asigna a un campo estático el valor de `null-coalescing-store`, cuyos
hijos son la dirección de un local y el fallback:
`A:/nsc-fixes-agente-2-evidencia/receiver-coalescing-store-result-mir-20261112/run-20261112T080127800679Z`.
El análisis ahora sigue el fallback y distingue específicamente la dirección
local como lectura de su valor previo. No trata indiscriminadamente todas las
direcciones locales como valores.

La compilación incremental y la regresión integrada pasaron 32 controles de
fallback, valor previo, copia y local inocuo, para Rc/Arc y constructor o
destructor. Pasaron también las regresiones de `??=`, `??`, condicional,
publicación directa y accesores binarios. Core ausente devolvió 77 y ruta
inválida 1; no se ejecutaron consumidores. Registro con comandos y hashes
estables de compilador, Core, metadata y runtime:
`A:/nsc-fixes-agente-2-evidencia/coalescing-result-integrated-validation-20261112/run-20261112T080707239524Z/results.json`.

El seguimiento es conservador para ramas no tomadas y asignaciones posteriores
al local. El valor previo de direcciones no locales y otras expresiones siguen
sin acreditarse. El compilador contiene otros cambios locales y el Core
binario era preexistente; la prueba no certifica el contrato completo.
