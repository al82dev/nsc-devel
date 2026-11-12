# Vinculación propuesta para `new T()` en fábrica binaria

## Estado y frontera

Diseño por lectura, no implementado ni validado. La reproducción, controles,
diagnóstico y límites están en `generic-new-constraint-access.md` en la copia
principal y en `implicit-factory-access-investigation.md` de esta copia.
No se cambia la accesibilidad general ni se usa el primer sitio de instancia
como autorización.

## Nodos y recorrido observados

* `GenericCloner.cpp`, `cloneNodeGeneric` y `cloneNodeChildrenIterative`, clonan
  nodos y sustituyen sus tipos. `cloneGenericConstraints` conserva en cada
  cláusula el nombre, `requiresDefaultConstructor` y `substitutedArgument`.
* La reproducción de Core usa `PlacementConstructionExpression` para
  `new ((T*)storage) T()`; el marcador
  `kGenericRawNewAuxiliary` es para `NewExpression` crudo y no acredita esa
  construcción ni debe reutilizarse como autorización.
  `PostfixIterationMachine.cpp:1468–1471` guarda el tipo que se construye en
  `typeSyntax` del nodo placement. En las fuentes del SDK, la ruta de clase es
  `SharedPointers.MakeRc<T>` o `MakeArc<T>` → `SharedLocalFactory.Create<T>`
  o `SharedAtomicFactory.Create<T>` → `SharedControlStorage.Construct<T>`,
  que declara `where T : class, new()` y contiene el placement; las fábricas
  que lanzan alcanzan `ConstructThrowing<T>`. No se ha demostrado todavía la
  identidad efectiva ni el certificado positivo en cada llamada anidada.
* `SpecializationExecutor.cpp`, `specializeMethod` y las rutas de tipos,
  reutilizan especializaciones por clave. Hay clonación adicional de miembro
  y de dueño cerrado: un único rango de fuente no identifica una instancia.
* `ConstructionBinding.cpp`, `bindImplicitConstruction`, recibe el nodo
  `expression` exacto. La autorización propuesta se usaría únicamente al
  resolver su constructor implícito, preservando la comprobación de cadena
  base, aridad y todas las demás rutas.

## Diseño propuesto

En el clonado de la construcción placement que originalmente referencia directamente
un parámetro, registrar una relación estructurada entre el nodo clonado, la
declaración y el ordinal del parámetro original. La identidad debe distinguir
parámetros de ámbitos anidados con el mismo nombre. La relación debe sobrevivir
clonaciones encadenadas y restauración de plantillas o, si no puede recuperarse
de forma inequívoca, conservar el diagnóstico actual. No asumir que un texto,
rango, tipo concreto, `auxiliaryText` ni nombre de parámetro es identidad.

En `ProjectGenericConstraintModel`, registrar evidencia positiva explícita de
que la cláusula `new()` de ese parámetro concreto se evaluó para el argumento
resuelto de esa especialización, separada de las candidatas rechazadas. En
construcción, aceptar la excepción limitada solo cuando coincidan esa relación,
la especialización efectiva, el argumento canónico y el destino resuelto; la
ausencia o ambigüedad conserva el rechazo. `specializationSatisfiesConstraints`
por sí sola no constituye esa evidencia: retorna verdadero para nodos no
registrados. El llamador debe poder nombrar el tipo en cada uso, aun cuando se
reutilice una especialización; falta verificar esa propiedad en cache hits.

El almacenamiento y API exactos del mapa están pendientes: hay que determinar
si se puede mantener dentro de `ProjectGenericModel` y consultar desde
`ProjectConstructionModel` sin crear ciclos ni cambiar formatos de caché. La
restauración y los dueños cerrados son Unknown; no se debe implementar un
atajo que dependa de que casualmente coincidan rangos o nombres. Este es un
diseño candidato, no prueba de que la conexión esté disponible en cada fase.

### Descarte y caché

`SpecializationExecutor.cpp:452–465` y `:579–580` revierten la clave y extraen
el registro si falla una especialización de tipo; los métodos tienen caminos
equivalentes en `reportMethodSpecializationError` (`:728–729`) y ante dueño
cerrado no localizado (`:1015–1016`). El clonado de tipos puede ocurrir en un
producto con arena propia. En `commitTypeSpecializationProduct` (`:230–259`)
se validan primero errores y clon, luego se absorbe la arena y se publica. Un
mapa nuevo tendría que permanecer provisional por producto y publicarse junto
al clon aceptado, sin escrituras globales desde trabajadores. Falta demostrar
el manejo correcto de clonaciones de método y dueños, incluida su reversión.

Hay dos rutas de restauración C3 en `SpecializationExecutor.cpp:411–441` y
`restoreTypeSpecializationFromCache` (`:477–506`): deserializan el AST ya
especializado, lo publican y ejecutan `processNode` sin repetir la clonación.
El payload actual (`:262–278`) no conserva el mapa externo propuesto. Una
autorización en hit requeriría reconstruir una relación estructural verificable
o serializarla con validación; mientras no se demuestre, debe permanecer
desconocida y no autorizada. La restauración C1 de registro
`TemplateCacheBridge.cpp:93–176` reasocia el registro a la sintaxis recorrida;
no es la misma ruta que la restauración de clon C3.
En `ensureMethodSpecialization` (`:827–870`) el planner reutiliza una clave
existente o reserva una especialización de método y prosigue con su clonado;
por lectura de esa función no se observó una restauración C3 equivalente a la
de tipos. Esto no demuestra que todo el recorrido de métodos carezca de otras
cachés, y las especializaciones anidadas siguen necesitando trazabilidad.

## Prueba mínima propuesta, aún sin ejecutar

Usar el productor Core binario y el mismo tipo interno de consumidor con
constructor implícito. Emitir sin ejecutar `MakeRc<Payload>` y
`MakeArc<Payload>`; contrastar `new Payload()` y una fábrica local equivalente.
Repetir el mismo tipo con constructor explícito privado para comprobar el
rechazo de `new()`. Probar dos parámetros `T` y `U` del mismo tipo concreto,
solo `T` con `new()`: una construcción cuyo origen sea `U` nunca debe obtener
el certificado de `T`; si el lenguaje rechaza esa plantilla antes de la
emisión, conservarlo como barrera previa y probar la relación en una prueba
unitaria de clonación, sin atribuirla al nuevo binding. Añadir una construcción
de tipo fijo y parámetros con igual nombre en ámbitos diferentes donde la
sintaxis lo permita; sin usar identidad por texto.

Ejecutar dos ordenaciones de sitios que reutilicen la misma especialización,
uno autorizado y otro que no pueda nombrar el tipo, siempre que el sistema de
proyectos permita expresarlas. Verificar por separado resolución de nombre por
sitio y reutilización real de clave; si la barrera de nombres impide alcanzar
la caché, registrar esa barrera y añadir una prueba unitaria del mismo registro
en vez de afirmar que el control dinámico ejercitó el caso. Mantener negativos
sin ejecución y exigir diagnósticos concretos, sin contar fallos anteriores como
éxito del cambio.

### Observaciones dirigidas

En `A:/nsc-fixes-agente-2-evidencia/provenance-fixtures-20261112/run-20261112T061528882844Z/results.json`,
`local-implicit` pasó semántica y rechazó emisión con el diagnóstico de
acceso a constructor implícito en `new ((T*)storage) T()` de Core binario.
`same-argument` pasó semántica y su primera emisión quedó bloqueada por un
runtime configurado fuera del entorno aislado; esa primera emisión no prueba
el resultado de vinculación.

Al repetir únicamente `same-argument` con runtime explícito en A, tanto
semántica como emisión O0 pasaron, sin ejecutar el consumidor:
`A:/nsc-fixes-agente-2-evidencia/provenance-fixtures-20261112/run-20261112T061641524659Z/results.json`.
La prueba construye mediante `new U()` ordinario en una fábrica local; no
demuestra que una futura regla de placement distinga ambos parámetros ni que
deba rechazarse ese programa. No se ejecutó ningún consumidor. Los registros
conservan comandos y hashes antes/después de compilador, Core y metadata.
