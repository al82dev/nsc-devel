# FnOnce en alternativas condicionales con entornos distintos

Estado: corrección parcial integrada y validada. Depende del cambio de
destino condicional con entorno coincidente. Solo añade un diagnóstico cuando todas las
alternativas conocidas son locales `const`, tienen una única procedencia por local, y cada
entorno tiene una invocación definitiva acreditada por otro local `const`. La unión de flujo
marca la invocación como definitiva únicamente si está presente en ambas ramas; el experimento
no confunde una invocación posible con una definitiva.

La restricción `const` se incorporó después de demostrar que una versión para locales mutables
rechazaba código válido: `ref` y `out` pueden reemplazar el delegado, pero el análisis anterior
conserva la procedencia y el flag de invocación antiguos. Los controles válidos de reemplazo
indirecto pasan con la versión restringida. Se verificó por separado que el compilador rechaza
pasar los locales const por `ref` y `out`; no se auditó la modificación mediante punteros o
código inseguro. El modelo sigue heredando las limitaciones de identidad por sitio de lambda.

Verificación integrada, 2026-11-11: la compilación incremental pasó y se
ejecutaron cuatro pruebas dirigidas de FnOnce y dos de préstamos; pasaron 4/4
y 2/2 sin omisiones. Los logs `fnonce-distinct-integrated-20261111.log` y
`fnonce-distinct-borrow-controls-20261111.log` están en `A:/nsc-fixes-evidencia`.
El compilador contenía otros cambios locales; no certifica una revisión limpia.

Se rechazan tres reproducciones en las que todos los entornos const posibles ya se invocaron,
incluidos aliases y dos ramas. Los ejemplos potencialmente inseguros se analizaron sin
ejecutarlos. Los controles válidos de ramas correlacionadas, selección simple, reasignación
ordinaria y reemplazo por `ref`/`out` se compilaron y ejecutaron O0/O2. Se incluyen como
análisis únicamente ejemplos donde queda sin protección un posible consumo o locales mutables.
También pasaron las regresiones de entorno coincidente, aliases locales y préstamos.

La referencia WIP SHA-256
`bad9ec92fa3f0b66e9605bc0638c7fadb22a0d9d5974f77df6c3253ff1663ee0` aceptaba los tres
negativos; el candidato WIP SHA-256
`c38d4b06786314cdf11ac1658f9a9dd218ef954c461c47248dcd4adfbc8f0f19` los diagnostica.
Las bibliotecas y objetos son una instantánea WIP aislada, no un commit limpio. Comandos y
salidas: `A:\nsc-fixes-agent1-temp\fnonce-conditional-tests\const-before.log`, `const-after.log`,
`const-same.log`, `const-local.log`, y `A:\nsc-fixes-agent1-temp\fnonce-borrow-distinct-const`.
Los falsos positivos de la versión descartada se conservan en `distinct-refout-before.log` y
`distinct-refout-after.log`; las pruebas const de `ref`/`out` en `fnonce-const-probe`.

Parche separado: `A:\nsc-fixes-agent1-temp\fnonce-distinct-prototype.patch`. Modifica solo
`src/Frontend/Bodies/CallAnalyzer.cpp`, sobre el archivo previo SHA-256
`b65d86766ea969bd396437334f9d96a31990e1c988dab61d889eedfd3672ed0a`. Prueba portable:
`tests/integration/fnonce_conditional_distinct.py`. Los casos de locales mutables, consumo solo
posible, procedencias no únicas, parámetros, campos y otros escapes permanecen sin cubrir.
