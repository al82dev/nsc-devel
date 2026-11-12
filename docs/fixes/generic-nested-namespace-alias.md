# Alias de namespace en argumentos genéricos anidados

La reproducción separada `Lookup<Pick.Own<1>>` se rechazaba con `Pick.Own`
no visible, aunque `Pick.Own<1>` directamente y `Lookup<Wrong.Own<1>>`
compilaban. El problema aparecía incluso al declarar el tipo sin usar el
indexador. La evidencia previa está en
`A:/nsc-fixes-agente-2-evidencia/indexer-nominal-argument/alias-check-20261111.json`
y `alias-declaration-new-check-20261111.json`.

Tras resolver de forma única la plantilla en el ámbito de origen, el scheduler
conserva su nombre calificado de módulo cuando la escritura usa un alias de
namespace. Esto permite que una especialización envolvente lo clone sin perder
la identidad al analizarse en el ámbito de la plantilla.

El prototipo compilado incrementalmente pasó la reproducción y controles de
alias directo y tipos homónimos en O0/O2: ocho compilaciones y ejecuciones con
los resultados esperados (`A:/nsc-fixes-evidencia/nested-alias-prototype-check-20261111/results.json`).
La prueba CMake permanente pasó O0/O2 (`nested-alias-cmake-20261111.cmd` y
`.log` en la misma carpeta); `nsc.language.generic-aliases` pasó 1/1.

Antes y después, el módulo referenciado fue aceptado y se rechazaron tanto el
tipo no referenciado como el tipo internal de otro proyecto por nombre directo
y por alias. Comandos y diagnósticos están en
`nested-alias-safety-baseline-20261111.json` y
`nested-alias-safety-candidate-revised-20261111.json`. Tres ensayos de tipos
genéricos privados anidados son inconclusos: el parser rechaza esa declaración
antes de comprobar visibilidad. La selección comprobada no certifica todos los
casos de reexportación, módulos homónimos o consumo binario. Se usaron el
compilador aislado con otros cambios locales, runtime de A y `nslink.exe` por
nombre; no se certifica una revisión limpia.

La regresión permanente `generic_alias_visibility.py` repitió los cuatro
controles representables y pasó con el compilador aislado. Comando y salida:
`A:/nsc-fixes-evidencia/nested-alias-visibility-20261111.cmd` y `.log`.
