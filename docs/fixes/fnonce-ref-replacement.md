# Reemplazo conocido de un delegado FnOnce por ref/out

Estado: corrección parcial integrada y validada. En el compilador WIP de referencia,
invocar un local FnOnce, reemplazarlo por otra closure mediante `ref`/`out` y llamar el nuevo
valor producía NS3020 porque el flag del valor anterior permanecía en el local. A la inversa,
si la closure de reemplazo ya se había invocado, el compilador podía aceptar una llamada
posterior a través del local reemplazado. Los casos inseguros solo se analizaron.

El candidato usa el método seleccionado semánticamente y reconoce solo un método estático
fuente, no virtual, no precompilado, sin async/generador ni excepción declarada, con dos
parámetros y cuerpo completo `destino = fuente`. El destino debe ser `ref`/`out`, la fuente
por valor, los tipos deben coincidir y ambos argumentos deben ser locales conocidos. Si la
fuente tiene una procedencia FnOnce única, copia esa procedencia al local destino y borra su
flag anterior, sin borrar el consumo del entorno viejo ni del nuevo. El mapeo utiliza los
ordinales de parámetros seleccionados, incluidos argumentos nombrados.

No se reconocen métodos con otras sentencias, ramas dentro del callee, efectos de excepción,
funciones externas, métodos virtuales, campos, fuentes sin procedencia única ni delegados Fn
sin procedencia FnOnce conocida. En esas rutas se conserva el comportamiento previo; no es un
resumen general de efectos por `ref`/`out`. La identidad de cierre hereda los límites ya
documentados por sitio de lambda.

Verificación integrada, 2026-11-11: la compilación incremental pasó y CTest
ejecutó cinco regresiones de FnOnce y dos de préstamos: 5/5 y 2/2 sin
omisiones. Los logs `fnonce-ref-replacement-integrated-20261111.log` y
`fnonce-ref-replacement-borrow-20261111.log` están en `A:/nsc-fixes-evidencia`.
El compilador contenía otros cambios locales y no certifica una revisión limpia.

La referencia WIP SHA-256
`7e030382f7dbb0fc5517102276979cd8b24719cd0bb471c7690512837948508c` rechazó cuatro controles
válidos de reemplazo y aceptó una reproducción de reemplazo por cierre ya consumido. El
candidato WIP SHA-256
`afe5ebf70611cce6da6ffee56670714bd92a8d76207587ebcd6d73ae367ad148` acepta y ejecuta los
controles válidos en O0/O2 y rechaza los negativos de valor conservado, auto-reemplazo, alias
antiguo consumido, fuente ya consumida y rama que conserva el valor. Pasaron también las
regresiones de aliases, parámetros directos y préstamos. Logs:
`A:\nsc-fixes-agent1-temp\fnonce-conditional-tests\ref-before-final.log`, `ref-after-final.log`,
`ref-local.log`, `ref-direct.log` y `A:\nsc-fixes-agent1-temp\fnonce-borrow-ref-replacement`.

El código de `e7184e823~1` ya mantenía el flag de invocación por local y no lo invalidaba en
`invalidateCallArgumentFlow`; esto sustenta que el falso rechazo precede a la protección de
aliases. No se compiló limpiamente esa revisión histórica. El candidato se enlazó con objetos
y bibliotecas WIP congelados, no representa un commit limpio.

Parche separado: `A:\nsc-fixes-agent1-temp\fnonce-ref-replacement.patch`. Solo modifica
`src/Frontend/Bodies/CallAnalyzer.cpp` y se verificó sobre su base WIP SHA-256
`a4460795482130b81f7171a1e2a6975329cb7d85d697b2aa5c30f8f215be8165`. Prueba portable:
`tests/integration/fnonce_ref_replacement.py`.
