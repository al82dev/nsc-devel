# Integración local y validación dirigida del 12 de noviembre

La revisión de producto probada fue `0fd35666d`, sobre
`development-fixes-limitaciones`. El compilador incremental aislado fue
`A:/nsc-fixes-evidencia/build/bin/nsc.exe`, SHA-256
`31dc7b9fc7a26baf87b6770d6c69b96d5e8c29067362693f60f2a5af464d5b9b`.
El manifiesto, comandos, hashes de bibliotecas y logs están en
`A:/nsc-fixes-evidencia/sdk-integrado-dirigido/logs/1794521625194875300/`.

La compilación incremental del objetivo `nsc` pasó; también se emitieron Core
y Collections para O0 y O2. La prueba dirigida de vistas por ref/out con SDK
binario pasó con ambas configuraciones de bibliotecas. Esto no certifica todos
los contratos del SDK ni una compilación limpia.

El ejemplo original de `SortWith` y el control de inferencia con Collections
binaria fallaron para O0 y O2: el compilador no infiere el tipo de la llamada.
El control con tipo explícito compiló y ejecutó con salida 42 en ambos niveles.
El ejemplo original se conservó por bytes y sus hashes antes/después coinciden.
La falla sigue pendiente; los logs `consumer-sort-original-*.log` y
`consumer-sort-*.log` detallan los comandos.

Los ejecutores incorporados desde vistas se ejecutaron con la copia y runtime
aislados y los temporales en A. `operator_original_39_vistas.py` pasó análisis
y emisión O0/O2 de ambos ejemplos originales, sin ejecutar los ejecutables.
`operator_original_70_vistas.py` pasó análisis, emisión y ejecución O0/O2 con
copias idénticas. `operator_computed_receiver_vistas.py` falló: los cuatro
casos positivos en ambos niveles recibieron diagnóstico de incremento o
decremento de usuario no implementado; dos negativos no alcanzaron el
diagnóstico de propiedad esperado. Sus logs llevan el nombre del ejecutor.
Esos negativos no se ejecutaron. El experimento FnOnce del commit `88978ca12`
no se incorporó como solución; su estado está en
`vistas-checkpoint-pausa-fnonce.md`.

Estos son controles dirigidos de integración, no una batería completa ni
evidencia de que todas las limitaciones hayan quedado resueltas. No se integró
la rama de interoperabilidad C/C++, no se hizo push ni despliegue.

## Repetición tras las correcciones

La revisión `14a65343f9f2e77bb4394cdfdece9ea37e2c6049` contiene las
correcciones de inferencia y de receptor calculado. La compilación incremental
de `nsc` y la emisión aislada de Core y Collections O0/O2 terminaron con
salida 0. Los seis consumidores dirigidos de vistas ref/out, ejemplo original
de SortWith y semántica de SortWith pasaron para ambos pares de bibliotecas.
El manifiesto registra revisión, comandos y hashes en
`A:/nsc-fixes-evidencia/sdk-integrado-dirigido/logs/1794523813914838700/manifest.json`.

Con ese compilador pasaron también `operator-computed.log`,
`operator-throw.log`, `operator-original39.log` y `operator-original70.log`
en la misma carpeta; el control 39 comprueba análisis y emisión, no ejecución.
La semántica de cada prueba y los límites de las correcciones se detallan en
`sortwith-identidad-binaria.md` y `operador-receptor-calculado-identidad.md`.
Esta repetición valida esos controles dirigidos, no el conjunto completo de
limitaciones, ni el experimento FnOnce pendiente.
