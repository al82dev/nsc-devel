# Conversión checked de i128 a long

En `3ebabea8a`, la reproducción
`A:/nsc-fixes-evidencia/checked-i128-long-before-20261113/check.cmd` compiló
con el compilador aislado y devolvió 31: convertir en modo checked el valor
9223372036854775808 a `long` produjo un número negativo truncado en lugar del
trap de overflow. La guarda de `convertScalar` solo cubría i128 a int.

La misma comprobación por truncamiento, extensión con signo y comparación
cubre ahora i128 a long. El trap existente es 147. La conversión ordinaria y
las demás parejas de tipos conservan su ruta.

Tras el build incremental, `tests/integration/checked_i128_long.py` pasó
ocho ejecuciones O0/O2: fuera de rango en ambos extremos, límites exactos,
cero, menos uno y conversión ordinaria. El control existente
`nsc.compiler.checked-i128-cast-int` pasó 1/1. Los comandos y resultados están
en `A:/nsc-fixes-evidencia/checked-i128-long-test-20261113.log` y
`A:/nsc-fixes-evidencia/checked-i128-int-control-20261113.log`. No se acredita
el resto de conversiones ni otras plataformas. El archivo del backend también
puede coincidir con la rama de interoperabilidad y requiere revisión al integrar.
