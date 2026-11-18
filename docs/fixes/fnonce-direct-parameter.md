# Prototipo acotado: FnOnce en parámetros llamados directamente

Estado: corrección parcial integrada para cuerpos fuente lineales. Depende
del seguimiento previo de identidad y consumo de entornos para aliases locales.

La compilación incremental integrada de 2026-11-11 pasó. CTest ejecutó
`nsc.compiler.fnonce-direct-parameter` y `nsc.compiler.fnonce-shared-environment`
con resultado 1/1 cada una, sin omisiones. Logs en
`A:/nsc-fixes-evidencia/fnonce-parameter-integrated-20261111.log` y
`A:/nsc-fixes-evidencia/fnonce-parameter-alias-control-20261111.log`. El
compilador contiene otros cambios locales; no certifica una revisión limpia.

Para un método estático fuente, no virtual, que no es async ni generador, el prototipo reconoce
el uso directo y lineal de un parámetro callable. Si el argumento es una closure FnOnce local
con entorno rastreado, una invocación directa consume ese entorno; dos o más producen NS3020.
Un parámetro sin uso no consume el entorno. El recorrido abandona el reconocimiento si encuentra
ramas, bucles, lambda, usos indirectos del parámetro u otras expresiones condicionales. El
argumento Fn normal conserva el comportamiento anterior.

Se rechazan el doble uso dentro del método, dos llamadas al método, llamada directa mezclada con
llamada al método en ambos órdenes, el mismo entorno en dos parámetros y llamada repetida en un
bucle. Los ejemplos negativos se analizan pero no se ejecutan. Se compilaron y ejecutaron O0/O2
los controles de una sola llamada FnOnce, parámetro ignorado, entornos independientes, dos llamadas
Fn y ramas excluyentes con una sola llamada. La regresión previa de aliases locales también pasó.

El análisis no es interprocedimental general: no sigue forwarding, recursión, alias del parámetro,
campos, métodos virtuales o sin cuerpo, cuerpos con ramas/bucles ni funciones async/generadoras.
Estas rutas no están protegidas por este prototipo. El consumo se estima conservadoramente al
llamar; una excepción previa a la invocación podría causar rechazo de un uso posterior válido.
No debe presentarse como solución completa de FnOnce por parámetros.

El parche `A:\nsc-fixes-agent1-temp\fnonce-parameter-prototype.patch` contiene solo cambios
en `src/Frontend/Bodies/CallAnalyzer.cpp` sobre la instantánea del prototipo de aliases anterior,
no sobre un HEAD limpio. La prueba portable es `tests/integration/fnonce_direct_parameter.py`.
El ejecutable de comparación WIP anterior tiene SHA-256
`78aef06df829e03e23beb15a8c2f3d8d15dbb3d9e252e6231789d2f58ac5370d`; el candidato incremental
tiene SHA-256 `4b6a7d2915914f9e28f34ef735affb5833da6bf9ee6915fff2a39e8f1a7a5cab`.
Los comandos y resultados están en `A:\nsc-fixes-agent1-temp\fnonce-param-tests` y
`A:\nsc-fixes-agent1-temp\fnonce-baseline`. El candidato se obtuvo recompilando únicamente
`CallAnalyzer.cpp`, recreando la biblioteca de frontend y reenlazando la instantánea WIP aislada.
