# Identidad pre-LLVM para decisiones de yield

Estado: diseño y sonda exploratoria ejecutada; sin modificación del producto.
Mientras LLVM no consume estos hechos, no incluirlos
en la identidad no demuestra un defecto vigente. La inclusión se vuelve necesaria
antes de que una decisión basada en ellos afecte el objeto emitido.

## Frontera actual

`ProjectOopMirCacheIdentity.cpp` escribe en la definición de cada función su
identidad, firma, locales, regiones de cleanup, flujo de excepción y cuerpo MIR;
actualmente `writeFunction` no escribe `asyncYieldFacts`. En
`ProjectOopObjectGenerator.cpp`, `composeModuleCacheKeys` combina digests por
función en la identidad de unidad y `restorePreLlvmObject` busca el objeto antes
de ejecutar el lowering LLVM. La clave final también incorpora el manifiesto de
fuentes leído por el lowering. Un cambio de archivo puede invalidar el objeto,
pero eso no prueba que un cambio exclusivo de binding, reconstruido o importado,
entre en la identidad MIR antes de la consulta.

Los hechos se reconstruyen después de restaurar MIR, pero esa reconstrucción no
vuelve distinta una clave de objeto si sus campos no forman parte de su material.
La otra ruta de caché basada en IR se consulta después del lowering y no prueba
la corrección de la consulta pre-LLVM. El comportamiento ThinLTO y la versión
global de codegen tienen sus propias identidades; no confundirlas con esta prueba.

## Serialización propuesta

Antes de habilitar el consumidor de descarte, extender la **definición**, no la
mera declaración, con una sección versionada y delimitada por dominio, por
ejemplo `NativeSharp.AsyncYieldDecisionInputs.v1`. Escribir cantidad de hechos
y, en su orden preorder determinista actual, para cada uno: `yieldPreorder`,
`nodePreorder`, presencia y valor de `parentFact`, `childIndex`, `nodeKind`,
`itemMode`, `resultKind`, `branchEvidence`, `hasBinding`, longitud y bytes de
`typeKey`, `valueCategory`, `ownership`, `ownershipTransfer`,
`ownershipFreshOwner`, `objectLifetimeOwnership` y `storageOwnership`. La
presencia del binding debe distinguirse de un binding existente cuyos valores
coincidan con los predeterminados. La forma y el orden de los wrappers, sus
regiones y campos MIR que afectan la emisión ya pertenecen al cuerpo/cleanup;
verificar que la cobertura existente sea completa al implementar el consumidor.

Usar `IdentityWriter` y sus campos tipados/con longitud, sin concatenaciones
ambiguas ni dependencias de punteros, direcciones, timestamps, rutas temporales,
orden de mapas, hilos o disposición de memoria. Los enums necesitan un mapeo
estable o una revisión explícita de versión cuando cambien sus identidades. La
identidad debe reflejar el plan derivado **o todas sus entradas y la versión de
la derivación**; no basta hashear solo la categoría final si condición, rama,
efectos, propiedad o cleanup también influyen. Mantener `Unknown` y binding ausente
distintos de un caso conocido. Si la derivación consulta datos externos al hecho
o al MIR ya serializado, incluir su identidad estable o invalidar la unidad de
manera conservadora. No calcular ni persistir un plan duplicado exclusivamente
para el hash si una serialización canónica de entradas es suficiente.

La sección debe calcularse antes de consultar la caché pre-LLVM y representar
la misma instantánea de hechos que verá la emisión. El versionado debe impedir
reutilizar objetos creados por un algoritmo de descarte anterior, aun cuando
los bytes de los hechos sean iguales. Revisar la versión de identidad global de
codegen y la identidad de la sección de forma coordinada; no asumir que una
invalida necesariamente todas las rutas de caché. Mantener entradas existentes
de plataforma, configuración, dependencias y fuentes.

## Sonda preparada y límites

El modo explícito `identity-probe` de
`tests/unit/AsyncYieldCrossProcessCacheTests.cpp` obtiene identidades de una
definición y de su declaración para polls de clase y namespace, muta un hecho
por campo sin cambiar MIR ni fuentes, informa si cada identidad cambió y restaura
el hecho. No exige que las mutaciones invaliden la identidad actual: eso sería
un gate prematuro. El runner independiente
`tests/integration/async_yield_object_identity_probe.py` prepara observaciones
fría, cálida y con cuatro trabajadores en A, exige restauración MIR cálida y
compara registros; no compara todavía objetos LLVM ni ejecuta programas. Está
fuera del runner ordinario.

La sonda dirigida compilada terminó con salida 0 en frío, cálido y paralelo;
produjo registros idénticos y restauró 0, 17 y 0 polls respectivamente. En
ambas funciones, las 16 mutaciones individuales dejaron sin cambio las
identidades de definición y declaración y restaurar el hecho conservó la
identidad original. Esto concuerda con que `writeFunction` no incluye esos
campos y **es esperable hoy**, cuando LLVM no los consume; no es un defecto
vigente demostrado. Las mutaciones son sintéticas y no equivalen a cambiar un
binding de un programa válido ni a demostrar hit de objeto. Evidencia:
`A:/nsc-fixes-evidencia/async-yield-object-identity-probe-20261112/`; SHA-256
del ejecutable de prueba:
`f5cb72b272f682210288567e9e4904cd3201109d4154b9202f8e3182ae55a543`.

Al implementar la serialización, convertir en pruebas de aceptación dirigidas:

1. Misma entrada fría/cálida y un/cuatro trabajadores: igual material canónico,
   mismo digest y objeto/conducta equivalentes; la fase cálida debe demostrar
   hit real de objeto, no solo restauración MIR o compilación de proyecto.
2. Sin modificar árbol MIR ni fuentes, mutar cada campo que consume la decisión
   (incluido `Unknown`, modo, binding, propiedad y relaciones) y demostrar cambio
   de identidad de definición, estabilidad de la declaración y restauración del
   digest original. Los estados sintéticos inválidos prueban sensibilidad, no
   demuestran que existan programas fuente válidos con ellos.
3. En un fixture fuente válido, cambiar binding o dependencia semántica que
   cambie la decisión: comprobar miss de la unidad afectada y resultado runtime;
   volver al estado anterior y comprobar recuperación coherente. Registrar hash
   de fuente y dependencia: un miss causado solo por fuente no prueba el punto 2.
4. Variar versión de algoritmo, ramas owned/alias, wrappers, Unknown y condición
   SIMD; probar separación de particiones y que unidades no afectadas no se
   invaliden si la arquitectura admite granularidad por función. No imponer esa
   granularidad si la unidad común incorpora dependencias compartidas legítimas.
5. Hacer pruebas frías/cálidas también con excepciones y cleanup cuando exista
   emisión, comparando efectos, objeto destruido y salida, y conservar logs,
   hashes de compilador/SDK, configuración y rutas. No ejecutar el código
   sintético con tipos incompatibles como programa runtime válido.

Esta propuesta no activa descarte, no considera seguro liberar un alias y no
certifica la conducta de ninguna caché de objetos actual.
