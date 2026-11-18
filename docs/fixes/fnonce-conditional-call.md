# FnOnce en una llamada con destino condicional

Estado: corrección parcial integrada y validada para el caso indicado. La comprobación previa solo
examinaba el destino de una llamada cuando era un identificador, por lo que aceptaba
`action(); (condition ? action : action)()` aunque ambas llamadas apuntan al mismo entorno.

La corrección recorre las alternativas y usa el binding semántico de cada una. Solo comprueba
el consumo cuando todas las alternativas alcanzables son locales y cada una tiene exactamente
la misma procedencia única de entorno FnOnce. Las condiciones booleanas literales, y las
constantes que el análisis ya haya demostrado, seleccionan únicamente la rama alcanzable.
No modifica la disposición ABI ni depende del prototipo descartado de forwarding o campos.

Esta regla es deliberadamente parcial. Alternativas de entornos distintos, procedencias con
múltiples posibles entornos, campos, parámetros y expresiones cuya identidad no es un local
conocido quedan sin proteger. La procedencia hereda los límites existentes de identidad por
sitio de lambda y no constituye un análisis general de alias o de concurrencia.

Verificación integrada, 2026-11-11: la compilación incremental pasó y las
pruebas `nsc.compiler.fnonce-conditional-call`, `nsc.compiler.fnonce-direct-parameter`
y `nsc.compiler.fnonce-shared-environment` pasaron 1/1 cada una. Los logs
`fnonce-conditional-integrated-20261111.log`,
`fnonce-conditional-parameter-control-20261111.log` y
`fnonce-conditional-alias-control-20261111.log` se conservan en
`A:/nsc-fixes-evidencia`. El compilador tenía otros cambios locales, por lo
que la evidencia no certifica una revisión limpia.

La referencia WIP congelada SHA-256
`8b353807e6c081d8e1c59f5c952c972c7f3b5d1d1c9c3c21549d2bf1d5ed8610` aceptó seis negativos
que el candidato WIP SHA-256
`bad9ec92fa3f0b66e9605bc0638c7fadb22a0d9d5974f77df6c3253ff1663ee0` rechaza con NS3020.
Incluyen órdenes opuestos, aliases, anidamiento y bucle. Los casos potencialmente inseguros se
limitaron al análisis. Los controles de una llamada, ramas excluyentes, alternativas de entornos
distintos y `Fn` llamado dos veces compilaron y ejecutaron O0/O2. La prueba conserva como casos
de solo análisis los usos que siguen sin cobertura. Pasaron también las regresiones dirigidas
de aliases locales, parámetros directos y ambas de préstamos.

Evidencia: `A:\nsc-fixes-agent1-temp\fnonce-conditional-tests\same-before.log`,
`same-environment.log`, `same-local.log`, `same-direct.log` y
`A:\nsc-fixes-agent1-temp\fnonce-borrow-conditional-same`. La biblioteca runtime se congeló en
el área de trabajo en A. La comparación usa bibliotecas y objetos WIP copiados; no es una
compilación de un commit limpio. Manifiesto inicial de bibliotecas:
`A:\nsc-fixes-agent1-temp\conditional-hybrid\first-inputs.json`.

Parche: `A:\nsc-fixes-agent1-temp\fnonce-conditional-prototype.patch`. Modifica solo
`src/Frontend/Bodies/CallAnalyzer.cpp`; se verificó su aplicación sobre el archivo WIP
congelado SHA-256 `e286ec0331fba84c2c0f3d0bdfb81212d00a9cec9deb61fcd4520ef02d611c32`.
Prueba portable: `tests/integration/fnonce_conditional_call.py`.
