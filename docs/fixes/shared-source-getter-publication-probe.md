# Publicación en getters fuente de payload compartido

El sondeo `A:/nsc-fixes-agente-2-evidencia/receiver-getter-probe.py` compara
getters de propiedad e indexador que devuelven un entero: en el caso negativo
también asignan `this` a un campo estático; el control no publica el receptor.
Cada getter se invoca desde el constructor o el destructor de una clase usada
como payload de Rc o Arc. Los consumidores se analizan y emiten, nunca se
ejecutan, incluso si el compilador acepta una forma publicadora.

Los 16 cruces superaron el análisis semántico. Los ocho getters inocuos
emitieron; los ocho publicadores fueron rechazados en emisión con
`shared payload publishes its receiver during construction or destruction`.
La evidencia, fuentes generadas, comandos, diagnósticos y hashes estables de
compilador, Core, metadata y runtime están en
`A:/nsc-fixes-agente-2-evidencia/receiver-getter-20261112/run-20261112T063358020169Z/results.json`.
Los procesos arrancaron desde la copia aislada y utilizaron temporales en A.

No se observó defecto en estas formas directas. El sondeo solo cubre getters
fuente en O0 y no demuestra accesores binarios, retorno de receptor, propiedad
anidada, indexadores sobre otro objeto, despacho virtual/interfaz ni seguridad
completa de publicación.

La regresión independiente `tests/integration/shared_receiver_getter_regression.py`
exige esos 16 resultados, sin ejecutar consumidores. Core no configurado
devuelve 77 y Core configurado inválido devuelve 1. La corrida configurada
pasó, con hashes de compilador/Core/metadata estables; los tres resultados y
sus comandos están en
`A:/nsc-fixes-agente-2-evidencia/receiver-getter-regression-config-20261112/run-20261112T063531151471Z/results.json`.
La versión incorporada a la copia principal se ejecutó de nuevo: 16 resultados
esperados, omisión 77 y configuración inválida 1, con procesos desde el
repositorio y temporales en A. Comandos y salidas:
`A:/nsc-fixes-agente-2-evidencia/receiver-getter-integrated-20261112/run-20261112T063651600317Z/results.json`.
