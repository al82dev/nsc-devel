# Resto i128 con signo

La reproducción dirigida de resto i128 no enlazaba en O0/O2: `NSL1020` informaba que el objeto generado refería `__modti3` sin definición. El enlace no atribuye por sí solo el defecto a NSLink. Log basal `A:/nsc-fixes-agent1-temp/i128-divrem-baseline-20261112.log`.

Tras la guarda de divisor cero, la ruta específica de resto i128 calcula el cociente mediante el algoritmo restaurador ya usado en división y obtiene `a − q*b` con operaciones i128 modulares. Esto cubre `MinValue % -1` sin emitir `srem`; los demás anchos y la división conservan sus rutas.

La compilación incremental y diez combinaciones dirigidas pasaron en O0/O2: resto con signos y extremos, divisor cero (código 129), dos regresiones de división y controles int/long. La prueba de anchos pequeños se ejecutó desde la copia aislada del agente. Log `A:/nsc-fixes-agent1-temp/i128-remainder-integrated-20261112.log`; compilador SHA-256 `dc62a5487d83f13eddf9689ef3efc1d73c4a36616b8aeadd68d8f733cffe8d99`, hashes de runtime e import libraries estables. El árbol contiene WIP y no es una validación limpia.

Ambos ejecutables de resto enlazaron y no importan `__modti3` dinámicamente según los logs `A:/nsc-fixes-agent1-temp/i128-rem-imports-O0-20261112.log` y `A:/nsc-fixes-agent1-temp/i128-rem-imports-O2-20261112.log`. Eso no descarta una definición estática sin inspeccionar los objetos. No se verifica con esta prueba el resto de otras anchuras no incluidas ni todas las rutas de otros backends.

El runner CMake exacto se ejecutó desde la copia principal y terminó 0: resto,
divisor cero y controles int/long en O0/O2. Los seis ejecutables están en
`A:/nsc-fixes-agent1-temp/i128-remainder-cmake-20261112`; el log queda vacío
por diseño cuando pasa. No se ejecutó CTest.
