# Acceso al constructor implícito en especializaciones genéricas

## Evidencia observada

La matriz `A:\nsc-fixes-agente-2-evidencia\implicit-factory-access-20261112\run-20261112T054850795200Z\results.json` registra que una clase `Payload` interna del consumidor se construye directamente y que `MakeRc<Payload>` y `MakeArc<Payload>` superan el análisis semántico pero fallan en emisión con `implicit parameterless constructor is not accessible from this module`, dentro de la unidad de plantilla de Core. El contraste de fábrica local está en `A:\nsc-fixes-agente-2-evidencia\implicit-factory-context-owned-20261112\run-20261112T055507558913Z\results.json`. Son resultados de ese compilador y Core, no de todas las configuraciones.

Por lectura en la copia aislada `A:\nsc-devel-fixes-agent-1-lambda`, `src/Frontend/ConstructionModel/ConstructionBinding.cpp:213-214` asigna al candidato implícito el resultado de `implicitDefaultAccessible`. En `src/Frontend/ConstructionModel/ConstructionAccessibility.cpp:132-166`, esa comprobación depende, entre otras condiciones, de si el tipo es público o pertenece al proyecto del contexto de la construcción. La selección de constructores explícitos públicos utiliza una comprobación diferente en ese mismo archivo. La ruta explica la diferencia observada como hipótesis causal de código; requiere comparación dirigida del cambio para demostrar causalidad.

## Restricciones y datos disponibles

`src/Frontend/ProjectGenericConstraintModel.cpp:783-794` comprueba `clause.requiresDefaultConstructor` mediante `hasPublicDefaultConstructor`. La consulta `specializationSatisfiesConstraints` en el mismo archivo, líneas 1391-1395, responde verdadero para `nullptr` y para cualquier nodo ausente del conjunto de inválidos. Por sí sola, esa respuesta no demuestra que una restricción `new()` exista, haya sido evaluada positivamente para el parámetro correcto, ni que la expresión de construcción corresponda a ese parámetro.

`include/nativesharp/Frontend/GenericSpecializationRecord.h` conserva, entre otros campos, `templateSyntax`, `specializedSyntax`, `templateOrdinal`, `typeArguments`, `instantiationSyntax`, `instantiationRange`, `overloadGroup` e `instantiationContext`. El comentario de `include/nativesharp/Frontend/ProjectGenericConstraintModel.h:95-98` explica que una especialización se guarda por clave y retiene el primer sitio de instanciación. Ese sitio no debe tratarse como evidencia de acceso para invocaciones posteriores. En la revisión efectuada no se identificó un mapa explícito de cada nodo original a su clon especializado; su ausencia no queda demostrada exhaustivamente.

## Propuesta y límites

No se propone cambiar la visibilidad general de tipos ni convertir constructores privados o protegidos en públicos. La opción acotada por investigar es vincular semánticamente la construcción original `new T()` al parámetro concreto, su cláusula `new()`, el argumento resuelto y una evaluación positiva verificable para la especialización efectivamente seleccionada. La ausencia de un vínculo o de información suficiente debe conservar el rechazo. No se dispone todavía de un parche que demuestre esa cadena.

Una comprobación basada únicamente en `isPrecompiled` tampoco es suficiente para identificar todo origen binario: `src/Frontend/ProjectCompilation.cpp:1488-1496` materializa unidades de plantillas publicadas con ese indicador en `false`, y `src/Frontend/NameLookup/TypePredeclarations.cpp:123` lo obtiene de la unidad. No se ha demostrado por esta lectura que todas las especializaciones afectadas alcancen la comprobación de constructor con ese valor.

## Pruebas dirigidas necesarias

- Tipo interno del consumidor con constructor implícito y con constructor explícito público, por fábrica binaria y local; construcción directa como control.
- Constructores explícitos privados y protegidos; tipo interno de un productor externo no visible; tipos genéricos binarios con constructores válidos e inaccesibles, y metadata antigua sin evidencia suficiente.
- Plantilla con `new T()` y otra construcción de un tipo fijo; parámetros `T` y `U` con solo uno sujeto a `new()`; plantilla sin esa restricción y especialización inválida.
- Sobrecargas con candidatos válidos e inválidos, llamadas repetidas con la misma clave y sitios de acceso distintos; verificar que no se herede autoridad del primer sitio.
- Prueba diferencial antes/después de la modificación propuesta, con emisión dirigida y sin ejecutar programas potencialmente inseguros; preservación de los rechazos y controles válidos.

Esta nota documenta una investigación estática. No acredita un arreglo, una garantía general de visibilidad ni compatibilidad con metadata histórica.
