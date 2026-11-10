# 73-run: llamadas durante compilación

## SCOPE LOCK

Objetivo: corregir llamadas a funciones desde #run con contrato explícito de
valores, resolución, efectos, recursos, diagnósticos y caché. Base
62af870fc2b2fb9a2fbfb1013c651df8f67dffd5; rama development-fixes-73-run.
Conservar el comportamiento de programas que no usan estas llamadas.

Modificar únicamente el evaluador, sus conexiones indispensables, pruebas nuevas
y esta nota. Punteros compartidos, préstamos y SDK binario pertenecen a la
instancia de escritorio: evitar sus archivos salvo necesidad indispensable y
documentada. Excluir herramientas externas, LLVM/Clang, interoperabilidad
C/C++, Daikon, NativeSharp.Text y nuevas versiones de producto o NSMETA. No
modificar ni desplegar en C:/native-sharp; no hacer merge, rebase, push ni
deploy. Usar exclusivamente CMD, builds incrementales y pruebas dirigidas;
builds, temporales, cachés y resultados independientes en A:. Conservar las
pruebas históricas y el ejemplo original.

## Evidencia inicial

docs/fixes/decisiones-pendientes.md identifica cases/73-run/build.log, pero
A:/nsc-fixes-evidencia no está disponible en esta máquina. El original no se
ha localizado; no se declara reconstruido. Se creó una muestra independiente
en A:/temp/nsc-73-run con FortyTwo() y #run(FortyTwo()).

cmake --build A:/temp/nsc-fixes-laptop-build --target nsc -j 4 terminó con 0.
El compilador aislado, invocado con --dump-project-comptime sobre esa copia,
devolvió 1: comptime sandbox forbids identifiers, calls, FFI, and host access.

## Análisis

ComptimeEngine procesa cada unidad antes de lexer, parser y resolución semántica.
IntegerExpression::parsePrimary rechaza identificadores. La caché depende de la
expresión resuelta y entradas declaradas, no de cuerpos de funciones: debe
incluir sus dependencias para no devolver valores obsoletos. El evaluador actual
usa enteros de 64 bits y rechaza overflow; las operaciones tipadas ordinarias
tienen anchos y reglas propias, por lo que la sustitución textual no demuestra
equivalencia.

Límites actuales: 100000 pasos, profundidad 128, 1024 directivas, 64 MiB de
fuente y 128 MiB de memoria. Cancelación, ciclos, efectos y dependencias forman
parte de la corrección.

## Estado al iniciar

Al registrar la línea base no había capacidad nueva implementada ni verificada.
El estado posterior y sus límites constan en las secciones siguientes.

## Contrato propuesto para el primer alcance

El camino de llamadas debe ser determinista y sin efectos externos. Se
admitirán funciones de fuente cuya resolución sea inequívoca y cuyos cuerpos
puedan evaluarse de forma segura; los casos cuya resolución, tipo o efecto no
pueda demostrarse deben rechazarse con diagnóstico. Las evaluaciones deben
usar el mismo ancho y semántica numérica que la función compilada, evaluar los
argumentos una sola vez en orden, conservar los límites acumulativos y la
cancelación, impedir ejecución nativa y contabilizar llamadas anidadas y ciclos.
La caché deberá depender del contenido que afecta al resultado.

El alcance exacto de tipos, expresiones y resolución se concreta en el contrato
implementado de abajo. Los casos no admitidos no se presentan como funcionales.
La semántica histórica de las expresiones #run sin llamadas queda protegida.

## Validación preparada

tests/integration/comptime_calls.py contiene casos nuevos de llamada sin
argumentos, llamada con argumentos y programa sin #run. La primera ejecución
en el compilador aislado falló en FreeCall/O0 con el rechazo esperado, antes
de ejecutar los casos posteriores. Se comprobó además que una llamada ordinaria
Add(19, 23), sin #run, supera --check-project-method-bodies y produce un
ejecutable O0. Esa comprobación no valida todavía el ejecutable ni #run.

## Decisión y contrato implementado

El usuario autorizó expresamente omitir la comparación con el ejemplo original
ausente. La muestra independiente anterior y las regresiones nuevas no se
presentan como dicho original.

Una directiva puede llamar directamente una función libre de la misma unidad
fuente y namespace, o un método estático del tipo que contiene la directiva.
El método del mismo tipo puede escribirse sin calificar o con el nombre de ese
tipo. Todas las declaraciones y llamadas deben tener identidad inequívoca. Se
rechazan sobrecargas ambiguas, nombres que pueden estar ocultos y resolución de
receptores ajenos, instancias, herencia, genéricos, imports y tipos parciales.

Las funciones admitidas reciben hasta 16 parámetros por valor de tipo int y
devuelven int. El cuerpo debe tener un único return; la expresión admite
literales decimales int, parámetros, llamadas anidadas admitidas y operadores
unarios +, - y ~ y binarios +, -, *, /, %, &, | y ^. Cada argumento se evalúa
una vez de izquierda a derecha. Las operaciones int usan 32 bits con wrapping;
división por cero y MIN / -1 se rechazan. No se ejecuta código nativo del
programa ni se permiten lectura de estado global, asignaciones, bucles, I/O,
FFI, async, throws o llamadas no resueltas. Las operaciones con capabilities
de archivo o entorno ya admitidas se resuelven por la ruta declarativa previa;
no se amplían sus permisos.

Los métodos estáticos se rechazan si el tipo declara campos, propiedades,
eventos o constructores: así no se omite una posible inicialización del tipo.
Una directiva aún pendiente dentro del cuerpo llamado se rechaza; una que ya
se procesó puede usarse. Los imports, algunos scopes complejos y el texto
`partial` o delimitadores de raw string en la unidad se rechazan de forma
conservadora. La comparación de esos textos puede rechazar también comentarios
o literales inocuos. Se conserva el requisito previo de que el resultado
impreso quepa en la longitud original de la directiva.

La implementación usa el lexer y parser existentes sobre copias, inventaría
declaraciones y ejecuta el subconjunto con una pila explícita de continuaciones.
No introduce recursión de producto. El camino histórico sin llamadas mantiene
su evaluador y caché anteriores.

## Recursos y caché

Se aplican los límites existentes por unidad: 1024 directivas, 100000 pasos,
profundidad 128, fuente general de 64 MiB y presupuesto contable de 128 MiB.
El perfil de llamadas añade un máximo de 4096 bytes para la fuente y expresión,
y 16 parámetros. El análisis preliminar carga pasos por bytes y nodos; la
ejecución carga pasos por continuación y comprueba cancelación y profundidad.
La búsqueda de nombres es lineal en el inventario acotado por esos 4096 bytes.

Para las copias y el parser se debitan conservadoramente 2048 bytes por byte
de fuente y expresión, más 32 unidades de margen, al presupuesto contable
acumulativo. Es una reserva estimada, no un límite duro medido del heap ni del
RSS del parser: esa propiedad no está verificada. Un presupuesto insuficiente
produce diagnóstico antes de analizar la fuente.

Los resultados de llamadas no usan la caché de valores: la entrada histórica
no contabiliza la memoria del parser. El digest de dependencia incorpora la
fuente completa y las dependencias externas declaradas. Así se evita reutilizar
valores al cambiar un cuerpo; repetir una llamada vuelve a contabilizar trabajo.
Una futura caché deberá conservar memoria, pasos y dependencias sin cambiar
este contrato.

## Archivos y coordinación

El cambio funcional está en `ComptimeEngine.cpp`, su cabecera y
`src/Frontend/Internal/ComptimeFunctionEvaluator.*`, con tipos internos
separados en `ComptimeDeclaration.h`, `ComptimeTask.h` y
`ComptimeWalkItem.h`. Las regresiones nuevas son
`tests/integration/comptime_calls.py` y
`tests/unit/ComptimeFunctionCallsTests.cpp`.

`CMakeLists.txt` registra el módulo y su prueba; es archivo compartido con
la revisión local conocida de escritorio `a43435d6e` y requiere revisar el
diff al integrar. `tests/compiler-suite/CMakeLists.txt` registra el módulo
para el objetivo que compila el frontend de forma independiente. Ese objetivo
no se construyó ni se ejecutó aquí. No se modificaron fuentes de punteros,
préstamos ni SDK binario.

Incidencia: la primera compilación incremental tras cambiar la cabecera
compartida recompiló objetos de infraestructura de interoperabilidad,
incluido `src/Interop/CHeaderGenerator.cpp`, pese a la exclusión del usuario.
No se modificaron esas fuentes ni se ejecutaron pruebas de interoperabilidad.
El build estaba aislado en A:. Esta compilación no acredita comportamiento de
interoperabilidad; el incidente se conserva explícitamente.

## Validación de esta tanda

El build incremental de `nsc` y de cinco objetivos unitarios terminó con
código 0. Se usó el compilador
`A:/temp/nsc-fixes-laptop-build/bin/nsc.exe`, identidad
`3.0.0+build.20261109T233411Z.44b7f8d8f85a3c19`, SHA-256
`bdc53602f366f93f840b5d837b75d2d54a4013a553b0ccb7fa54950709a3329f`.

`tests/integration/comptime_calls.py` terminó con código 0; registro
`A:/temp/nsc-73-run-directed-9.log`. Fueron 41 casos y 82 compilaciones:
17 casos válidos se compilaron y ejecutaron en O0 y O2 (34 ejecuciones con
salida 42), y 24 casos inválidos se rechazaron con el diagnóstico esperado en
ambos niveles (48 rechazos). Se cubren funciones libres, namespace, parámetros,
llamadas anidadas, métodos estáticos y calificados del mismo tipo, comparación
con ejecución ordinaria de wrapping y división con signo, dos directivas,
marcador comptime, control sin feature, nombres ocultos, sobrecargas, receptores,
efectos, división inválida, recursión, límites exactos y memoria acumulada.

La prueba unitaria `nsc.comptime-function-calls` pasó 1/1 y comprueba además
pasos, memoria contable, profundidad, cancelación inicial y durante ejecución,
cambios del cuerpo y ausencia de cache hits de llamadas. Las pruebas históricas
`nsc.item385.comptime-core`, `nsc.item386.comptime-sandbox`,
`nsc.item387.comptime-cache` y `nsc.item388.comptime-reproducibility`
pasaron 4/4 sin modificar sus fuentes. Registros:
`A:/temp/nsc-73-run-unit-actual.log` y
`A:/temp/nsc-73-run-historical-actual.log`.

La reproducción independiente inicial también pasó
`--dump-project-comptime A:/temp/nsc-73-run/Comptime.nsproj`: código 0 y una
evaluación. El comando de proyecto contiene dos fuentes porque se añadió un
control independiente; no es el ejemplo original omitido por autorización.

Se ejecutó `clang-tidy -p A:/temp/nsc-fixes-laptop-build
--checks=-*,clang-analyzer-*` sobre el evaluador, `ComptimeEngine.cpp` y la
prueba unitaria. Los tres terminaron con código 0. El reporte del evaluador
conserva un aviso de padding en `include/nativesharp/AST/TypeSyntax.h`, archivo
preexistente no modificado; el aviso ya aparecía en la pasada anterior. Los
reportes se conservaron bajo `A:/temp/nsc-73-run-tidy-actual*`. El análisis
no demuestra ausencia general de defectos ni medición del heap.

No se ejecutaron batería completa, baseline, pruebas de interoperabilidad,
consumo del SDK binario ni el objetivo independiente de compiler-suite. La
configuración existente de CTest muestra una ruta antigua a
`C:/native-sharp/bin/nsc.exe`; las pruebas de esta tanda usaron el compilador
aislado indicado y no esa ruta. No se hizo merge, rebase, push ni deploy.

## Límites pendientes

No están implementadas las llamadas entre unidades, a otro tipo, mediante
receptores de instancia o imports, las sobrecargas resueltas semánticamente,
otros tipos numéricos, control de flujo, variables locales ni efectos dentro
del cuerpo. Los rechazos deliberados son parte del perfil implementado; no
demuestran soporte general de evaluación semántica de NativeSharp. La reserva
de memoria es contable y aún falta verificar una cota dura del heap real.
Al integrar, cotejar el archivo CMake compartido y repetir las pruebas
dirigidas sobre la rama de integración.
