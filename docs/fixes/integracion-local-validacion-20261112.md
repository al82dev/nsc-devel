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
