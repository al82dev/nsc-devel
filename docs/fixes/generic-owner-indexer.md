# Receptores genéricos devueltos por indexadores

## Reproducción y causa

`Lookup.this[int]` devuelve `Own<1>` y el consumidor llama a
`lookup[0].At<20, 42>()`. Con el ensayo de métodos genéricos previo, O0 y O2
rechazaban `packat(Ns, N)` porque no se identificaba el dueño cerrado. La
reproducción y comandos están en
`A:/nsc-fixes-agente-2-evidencia/generic-indexer-receiver/results.json`.

La inferencia de llamadas trataba explícitamente un elemento de arreglo, pero
no el tipo devuelto por un indexador definido por el usuario. El análisis de
la llamada ahora identifica la declaración del contenedor, obtiene el tipo
de su getter y conserva el ámbito de declaración. Para un contenedor genérico
sustituye sus argumentos antes de resolver el dueño del método. Se registra
también el ámbito léxico de estructuras; antes se guardaba solo el de clases.

Cuando existen varios indexadores con igual cantidad de argumentos, se usa
únicamente una coincidencia inequívoca de tipos primitivos exactos. Si no se
puede identificar así, no se adivina la sobrecarga y el análisis posterior
conserva su responsabilidad. La búsqueda recorre bases de clase identificadas
de manera única, manteniendo sus sustituciones y deteniéndose ante ciclos o
bases ambiguas. No se cambió el análisis de préstamos.

## Evidencia y límites

Con la primera variante, las clases pasaban y un indexador de estructura
fallaba en diez cruces; conservar el ámbito de estructuras resolvió esa forma.
Un `GenericLookup<N>` seguía fallando en diez cruces antes de sustituir sus
argumentos (`A:/nsc-fixes-agente-2-evidencia/generic-indexer-generic-before.log`).
Las pruebas diagnósticas de sobrecarga `int`/`long` y de indexador heredado
fallaban antes y ejecutaron el resultado esperado después; sus fuentes y
resultados están en `generic-indexer-overload`, `generic-indexer-overload-after`,
`generic-indexer-inherited` y `generic-indexer-inherited-after` en esa carpeta.

`tests/integration/generic_owner_indexer.py` comprueba 13 consumidores en O0 y
O2 desde fuente y contra cuatro productores estáticos/DLL en O0/O2: 130
compilaciones dirigidas, incluidas 20 negativas que requieren el diagnóstico
de índice fuera del pack. Los positivos ejecutaron 42. Se incluyen clases y
estructuras, contenedores genéricos y con pack, alias léxicos opuestos,
sobrecargas primitivas exactas, herencia ordinaria y genérica y dos dueños
distintos. Log: `generic-indexer-inherited-fix/generic_owner_indexer.log`.

También pasaron los seis controles existentes de método y las pruebas de
matriz, clausura binaria, restricciones e imports binarios; sus logs y
resúmenes están en `generic-indexer-inherited-fix/`. El compilador se enlazó
con tres objetos recompilados y copias de bibliotecas de campaña. Su comando y
hashes están en `generic-indexer-inherited-fix/build.log`; es un ejecutable
híbrido, no una compilación limpia. Falta repetir la selección con el
compilador integrado; las pruebas se ejecutaron directamente y no mediante el
CTest recién registrado.

No se demuestra la selección entre sobrecargas que requieren conversiones,
tipos nominales o inferencia desconocida, ni todas las formas de herencia o
metadata histórica. En esos casos la selección nueva no inventa una
coincidencia. Los ejemplos originales del informe no se modificaron.

Después de integrar el cambio en `27af4997c`, la compilación incremental del
compilador terminó con código 0 y la regresión registrada de indexadores pasó
sin omisión. Comando y salida:
`A:/nsc-fixes-evidencia/generic-indexer-integrated-20261111.cmd` y `.log`.
El compilador contiene otros cambios locales y no acredita una revisión
limpia. Las variantes con conversión y tipos nominales siguen en investigación.
