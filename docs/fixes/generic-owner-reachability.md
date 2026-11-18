# Clausura de métodos genéricos en bibliotecas

## Reproducción y causa

Una biblioteca anterior de la campaña contiene `Fixture.Box`, `Box<N>` y
otros dueños con un método `Echo<U>`. El consumidor usa un campo de
`TypeOwner<Pair<20, 22>>` y llama a `Echo`; no usa `Box<N>`. La selección por
nombre conserva los registros de los métodos `Echo`, pero
`PrecompiledSurfaceReachability::includeTemplate` alcanzaba al dueño por
`ownerName`, sin aridad. Se descartaba `Box<N>` y sus métodos se reconstruían
como métodos de la clase ordinaria `Box`, causando rechazo por duplicación.

La inspección `generic-historical-trace-meta/results.json` registra los
registros realmente recibidos, la superficie generada y las claves
recolectadas. La hipótesis inicial de un fallo de caché se descartó: la traza
registró recolección fría. El cambio alcanza la plantilla propietaria si el
prefijo exacto de la clave del método identifica una plantilla del mismo
módulo y namespace cuyo nombre completo coincide con el dueño. De no existir
esa identidad exacta conserva la ruta anterior para el dueño ordinario.
No se modifica el formato de metadata ni se identifica un dueño por coincidencia
de nombre terminal.

## Evidencia

Se usaron los cuatro pares de biblioteca y metadata estática/DLL generados el
10/11/2026 en `A:/nsc-fixes-evidencia/temp/nsc-owner-field-nested-5gn8_nu6`.
Su creación por el compilador de un ensayo anterior y los comandos constan en
`A:/nsc-fixes-evidencia/generic-owner-field-nested-inspect.log`. No son una
publicación certificada ni representan todas las bibliotecas anteriores.

Con el compilador integrado de la campaña, cuyos hashes constan en
`A:/nsc-fixes-agente-2-evidencia/generic-historical-20261110/results.json`,
los ocho cruces de productor O0/O2, biblioteca estática/DLL y consumidor
O0/O2 fallaron con métodos `Echo` duplicados. El mismo resultado se obtuvo sin
el producto precalculado (`generic-historical-no-sidecar/results.json`).

Se recompiló únicamente `PrecompiledSurfaceReachability.cpp` en A y se enlazó
delante de copias de las bibliotecas y objetos preexistentes de la campaña.
La procedencia, comandos y hashes están en `generic-historical-fix/build.log`;
el ejecutable es híbrido y no certifica una revisión limpia. Con él los ocho
cruces devolvieron 42 tanto con el producto precalculado como sin él:
`generic-historical-historical-fix-sidecar/results.json` y
`generic-historical-historical-fix/results.json`.

La regresión `generic_owner_reachability.py` genera su propio productor con
dueños homónimos de cero, uno y dos parámetros. Comprueba una demanda que poda
superficies y otra que usa los tres dueños con resultados distintos. Cada
caso se ejecuta en los ocho cruces estático/DLL y O0/O2. Antes del ajuste el
fixture de dos dueños fallaba en los 16 consumidores; después los 16 pasan
con los tres dueños (`generic-owner-reachability-before.json` y
`generic-owner-reachability-after.json`).

También pasaron los seis controles de métodos genéricos y las pruebas
dirigidas de matriz, restricciones, imports binarios, alias, metadata interna
y conformidad de traits. Los logs están en `generic-historical-fix/` y los
resúmenes `test-results-*.json`. Se ejecutaron directamente, no mediante el
CTest recién registrado.

Queda repetir las pruebas con el compilador integrado. La clausura no inventa
un dueño para claves antiguas sin identidad inequívoca; esta evidencia no
certifica todos los artefactos históricos ni otros formatos de receptor.

## Verificación posterior en integración

Con el compilador principal construido incrementalmente, las tres pruebas
dirigidas de clausura, consumidor binario y alias binario anidado pasaron sin
omisiones: `A:/nsc-fixes-evidencia/generic-reachability-integrated-20261111.log`.
Se repitieron además los ocho cruces de los artefactos anteriores con y sin
archivo auxiliar; todos compilaron y devolvieron 42. Los comandos y hashes
están en `A:/nsc-fixes-evidencia/integrated-historical-owner.py` y los
`results.json` de `generic-historical-integrated-sidecar` y
`generic-historical-integrated-no-sidecar`. El compilador contiene otros
cambios locales; la evidencia no acredita un árbol limpio ni todas las formas
de metadata histórica.
