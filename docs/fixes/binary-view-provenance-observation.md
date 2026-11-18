# Observación experimental de procedencia de vistas

Corrección de procedencia: la primera ejecución de los tres controles
semánticos descritos abajo lanzó procesos hijos desde temporales en A y es
evidencia no conforme. Se repitieron con los ejecutores corregidos desde
`A:/nsc-devel-fixes`; los tres pasaron. Comandos y salidas:
`A:/nsc-fixes-evidencia/view-probes-repo-cwd-20261112.json` y logs
`view-probes-repo-cwd-{producer,homonym,destructor}-20261112.log`.

El árbol de trabajo incorpora temporalmente un observador experimental para
estudiar transferencias de una vista extraída de una tupla hacia una salida
`ref` u `out`. Las aristas observadas son parciales y se identifican como
`unknown`; no se publican en la metadata ni se elimina la barrera que rechaza
llamadas binarias con vistas compuestas. No constituyen un contrato verificado.

Después de una compilación incremental, tres pruebas semánticas dirigidas
pasaron (`A:/nsc-fixes-evidencia/provenance-observation-20261111.cmd` y
`.log`). La primera observó las dos transferencias preparadas sin publicar un
contrato; la segunda descartó un tipo homónimo que no es vista; la tercera
examinó destructores directos y anidados. El directo se rechazó antes de la
selección. El anidado se aceptó y produjo una arista parcial `unknown`, pero
no la marca del subconjunto candidato; son observaciones distintas y ninguna
autoriza habilitar consumo. No se ejecutaron ejemplos potencialmente inseguros.

El compilador probado tenía SHA-256
`15b3e955f7eab1b471bdd2ecdfc82a35b3b1bd65239ffbd5ddd4d91f2cc36c26`,
contenía otros cambios locales y usó un Core binario preexistente. El selector
no demuestra exhaustivamente efectos implícitos: los predicados actuales de
destrucción pueden omitir tipos sin resolver. El modelo triestado aislado del
agente, que conserva `Unknown` ante información ausente o ciclos, aún no está
conectado al compilador ni a la metadata. Las pruebas no certifican una
revisión limpia ni el contrato de las bibliotecas precompiladas.

Las dos pruebas C++ aisladas del modelo de grafo y reconocimiento de cuerpo
compilaron y terminaron con código 0 (`provenance-cpp-unit-20261111.cmd` y
`.log` en `A:/nsc-fixes-evidencia`). Comprueban el prototipo manualmente
construido; no prueban por sí mismas el análisis completo del productor real.
