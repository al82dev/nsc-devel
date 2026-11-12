# Sobrecargas de indexador con métodos genéricos

## Reproducciones

Con el compilador de la campaña anterior, seleccionar un indexador, guardar
el resultado `Own<0>` y llamar después a `At` devolvía 42 en O0/O2. La llamada
directa sobre `items[index]` fallaba antes de cerrar el dueño en dos casos:
un argumento de clase `Key` y la conversión numérica `int` a `long` frente a
otra sobrecarga `double`. Comandos y diagnósticos:
`A:/nsc-fixes-agente-2-evidencia/generic-indexer-conversions-before/results.json`.

La fase genérica se ejecuta antes del modelo semántico que resuelve el
indexador. La corrección reconoce coincidencias nominales solo por identidad
de declaración resuelta en el ámbito de cada lado. Para candidatos numéricos
conocidos utiliza `ProjectTypeConversion::classifyImplicit` y compara rangos y
conversiones entre destinos. Para literales enteros evalúa el valor mediante
`ProjectIntegerEvaluator` antes de clasificar la conversión. Si faltan datos o
varios candidatos permanecen indistinguibles, no elige un dueño.

## Pruebas y evidencia

Las reproducciones nominal y numérica pasan junto con sus controles en O0/O2.
Los controles con literal también distinguen `1`, que selecciona el índice
`byte`, de `256`, que selecciona `long`. Los comandos y resultados están en
`generic-indexer-conversions-indexer-constant-fix/results.json` bajo la carpeta
de evidencia indicada arriba.

Las sobrecargas `short`/`ushort` para un `byte` son ambiguas: el acceso
ordinario y la llamada directa se rechazan en O0/O2. El acceso ordinario
diagnostica ambigüedad; la llamada directa todavía se detiene antes con un
diagnóstico de parámetro genérico. La corrección no escoge silenciosamente un
indexador, pero la calidad de ese diagnóstico queda pendiente.

La regresión `generic_owner_indexer.py` cubre fuentes y cuatro cruces de
productor estático/DLL O0/O2, con consumidor O0/O2. Incluye `Key` homónimos en
dos namespaces con aliases opuestos, conversión numérica, límites de literal,
ambigüedades de uno y dos argumentos y todos los casos anteriores de
indexadores. Pasó junto con diez regresiones dirigidas de métodos, matriz,
clausura, restricciones e imports. Logs en
`A:/nsc-fixes-agente-2-evidencia/generic-indexer-constant-fix/`.

La validación usó un compilador híbrido aislado: tres objetos recompilados y
bibliotecas copiadas del build de campaña; sus comandos y hashes están en
`generic-indexer-constant-fix/build.log`. Falta repetirla con el compilador
integrado y con la prueba registrada en CTest.

No se afirma selección completa para conversiones definidas por usuario,
herencia nominal entre argumentos, expresiones constantes no literales o
sobrecargas con parámetros estructurados y por referencia. Ante información
insuficiente la fase genérica conserva la ambigüedad; algunos programas
válidos pueden seguir rechazándose antes del análisis semántico definitivo.
En la primera prueba, un `const int` local con indexadores `byte`/`long` usaba
`long` en el control ordinario, pero la llamada directa no cerraba el dueño.
La corrección posterior y su evidencia se describen en
`generic-owner-indexer-local.md`. Los ejemplos originales no cambiaron.
