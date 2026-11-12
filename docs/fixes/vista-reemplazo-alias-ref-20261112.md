# Vista activa y reemplazo a través de un alias ref

## Estado

**Reproducido en análisis semántico; corrección pendiente.** No se conoce un
ejecutable anterior verificado con el que atribuir el hallazgo a un commit.
No se han ejecutado los programas potencialmente inseguros.

## Reproducción

El compilador acepta reemplazar mediante un local `ref Owner` el dueño de una
vista viva. La asignación directa equivalente al mismo dueño se rechaza con un
diagnóstico de préstamo activo. El resultado se observó con origen directo de
la vista y con `owner.Borrow()`, con y sin un local homónimo en un scope
interior, y con una cadena de tres représtamos.

El volcado MIR de `negative_known_replace_alias_no_shadow` registra un
`reference-local` inicializado desde `local-address owner`, seguido de
`store Owner` hacia el puntero del alias. El volcado de cuerpos de la cadena
de tres représtamos conserva `frame-local:0`, `borrow-place=L0` y la
cadena de fuentes. La evidencia está en
`A:/nsc-fixes-evidencia/vistas-shadow-riesgos-20261112/resultados-comparacion/known-alias.mir.txt`
y `resultados-reborrow/reborrow-bodies.txt` bajo la misma carpeta.

El nsc de la primera comparación tuvo SHA-256
`4aedad5ddd323493d549492da5a649ec23bacae39096fc2e922c01731780211d`;
el de la tanda ampliada tuvo
`d6f7c8e534461a4df3394ae98d09a6ba9e26ebc24a3dc7de72a53efba88b38d1`.
Cada JSON conserva hashes de las fuentes, comandos, cwd, stdout, stderr y
hashes del ejecutable antes/después. Las dos tandas usaron ejecutables
diferentes; no constituyen comparación antes/después de una corrección.

`resultados-reborrow/results.json` registra siete controles positivos
aceptados, seis reemplazos por alias/représtamo aceptados inesperadamente,
dos reemplazos directos rechazados y cuatro escapes por `ref/out` rechazados.
Los positivos incluyen ausencia de vista, vista fuera de scope, dueño homónimo
distinto, campo no relacionado y représtamo sin vista.

## Límites y siguiente paso

El volcado semántico resuelve `Borrow` sobre el local `owner`; por
inspección, la recolección de fuentes del método de instancia registra esa raíz
como `Unknown`. El volcado no imprime directamente el vector, y la aceptación
por alias también se observa con origen directo. No debe atribuirse el hallazgo
a `Unknown` ni al shadowing sin una prueba adicional.

`AssignmentAnalyzer.cpp` identifica el local LHS y consulta la protección de
dueño para ese ordinal; `referenceEscapeOrigin` y `referencePlaceKey` del
alias ofrecen una posible ruta hacia el almacenamiento real. La propuesta
estática, sus controles de représtamo y el peligro de contar el propio alias
como conflicto están en
`A:/nsc-fixes-evidencia/vistas-shadow-riesgos-20261112/DISENO.md`. Quedan por
resolver explícitamente procedencia desconocida, proyecciones, parámetros,
uniones y binarios. No hay solución productiva aplicada ni prueba general de
seguridad.

## Prototipo conservador y regresión

Se preparó un guard WIP para una asignación a través de referencia mutable con
origen local y ubicación canónica `L<ordinal>`. Una primera versión trata
`Unknown`, `*` o procedencia heredada sin identidad como posible conflicto
en lugar de inferir seguridad por el nombre. La compilación incremental pasó.
El compilador utilizado en
`A:/nsc-fixes-evidencia/view-ref-owner-guard-20261112.log` tuvo SHA-256
`9c005763098c2409668d215430495015f4dd71848a9622816e0ac5b9bbeaf017`.
Los negativos de alias/représtamo de la selección se rechazaron, pero el
control válido `distinct_shadow_owner` también se rechazó: la prueba dirigida
en su conjunto **no pasó**. Los otros controles de esa selección pasaron.
Ningún programa negativo se ejecutó.

`collectViewSourceFields` añade actualmente `*` incluso al registrar
argumentos `ref` con identidad local conocida de una construcción de vista.
El modelo disponible no distingue el origen y momento de ese marcador, ni
ofrece en este punto una prueba completa de que la vista no fue reasignada a
través de una llamada u otra ruta. Ignorar el marcador simplemente porque
existe una raíz conocida distinta no demuestra que no pueda haber otra raíz.
La versión WIP no cumple simultáneamente conservación de este positivo y
tratamiento conservador del origen desconocido; no debe registrarse como
corrección terminada. Parámetros y proyecciones siguen fuera del guard.

### Orígenes inciertos acotados: propuesta no implementada

Una vía por investigar es conservar, para cada productor incierto de vista,
el callable, scope y límite de ordinales de locales existentes en ese momento.
Las copias trasladarían el límite original; una asignación posterior o una
llamada `ref/out` capaz de escribir la vista agregaría un origen incierto con
su propio límite. Los orígenes heredados sin certificado conservarían un límite
desconocido, no una garantía de seguridad.

La hipótesis es que una vista anterior no puede haber obtenido por vías seguras
la dirección de un struct local por valor que aún no existía. El límite podría
ayudar a probar que la vista exterior del control válido no presta del dueño
interior posterior. Esta conclusión no debe extenderse a referencias de clase,
objetos compartidos, alias, punteros ni memoria externa: un local posterior
podría referirse a un objeto ya existente.

La hipótesis exige demostrar que no se pierde ningún productor en una
reasignación, llamada o rama; que loops y reutilización de slots no vuelven a
poner viva una referencia inválida; y que `unsafe`, aliases externos y
captures no se interpretan como prueba de disyunción. Las uniones deben ser
conservadoras. En análisis por callable, el límite y sus identidades requieren
remapeo con `localBase` y verificación de la identidad del callable; la
serialización y la identidad de cualquier caché que consuma esos datos también
deben investigarse. La alternativa afectaría al modelo de locales y a varias
rutas de producción de vistas, no solamente a `AssignmentAnalyzer.cpp`.

No se ha implementado ni probado este diseño, ni se afirma que el límite de
ordinales baste para todos los controles. La política estricta falló
`distinct_shadow_owner`; su diff se conservó en evidencia y se retiró del
producto. No se debe ignorar `*` para hacer pasar ese control.

### Infraestructura informativa en preparación

La primera tanda conserva por local de vista observaciones de origen
desconocido: scope de producción y cantidad de locales observada. **No son
límites de vida ni pruebas de que dos almacenamientos sean disjuntos**. La
incertidumbre no localizada usa ambos campos vacíos. Las observaciones de una
copia o asignación se unen conservadoramente, y una salida `ref/out` añade una
observación de llamada; si el destino es una referencia cuyo local no se
conoce, marca las vistas visibles del callable. El análisis por ramas conserva
la unión existente, no demuestra un estado exacto por camino.
Un vector sin observaciones significa que no se registró evidencia: no
significa que el valor carezca de raíces desconocidas ni autoriza disyunción.

Cada local mantiene como máximo ocho observaciones distintas. Ocho es una
decisión de presupuesto para limitar memoria y búsquedas, no un valor medido ni
un límite semántico del lenguaje. Si llega una novena observación o cualquier
origen no localizado, toda la historia se sustituye por un único Unknown no
localizado que subsume las observaciones, y no puede volver a estrecharse por
una copia o una unión. La búsqueda recorre como máximo ocho entradas. El vector
vacío ocupa espacio en cada `MethodLocalSymbol` (su tamaño depende de la
implementación de la STL; no se ha medido el impacto total). Si una llamada
tiene varios destinos inciertos se recorre el conjunto de locales visibles una
vez, no una vez por argumento.

El fusionador remapea scope y conteo observado con las bases del shard; el
Unknown no localizado queda vacío al remapear. La compilación incremental
conjunta pasó. La prueba dirigida de modelo pasó sus 25 comprobaciones de
copias, asignación, salidas `ref/out`, referencia alias, dos callables,
deduplicación y colapso. La ejecución directa desde la raíz del repositorio
quedó en `A:/nsc-fixes-evidencia/view-unknown-model-repo-cwd-20261112.log`;
el ejecutable de prueba tuvo SHA-256
`3db5c6feb1a52b4be35b6f59d9baea309696a53b4a35db0b4c7c3bdc399a911b`.
La prueba CTest también pasó y su resumen está en
`A:/nsc-fixes-evidencia/view-unknown-origin-model-20261112.log`.

Con el compilador de SHA-256
`70569bd4b18d2b563cfdecc907f525bb251b670e95b896aef82b9fbb2166ff78`,
la prueba de identidad con shadowing conservó sus controles: cuatro programas
válidos ejecutaron con resultado 42 en O0 y O2, y tres escapes se rechazaron.
El registro es `A:/nsc-fixes-evidencia/view-shadow-origin-20261112.log`.
La selección semántica ampliada aceptó los nueve controles válidos y rechazó
los seis controles directos o de escape; aceptó inesperadamente los siete
reemplazos por alias que deberían rechazarse. Su registro completo es
`A:/nsc-fixes-evidencia/view-ref-owner-observation-20261112.log`. Esos siete
programas no se ejecutaron. La observación confirma que esta tanda informativa
no corrige la brecha, sin atribuirla a los commits de vistas.

No se ha
demostrado persistencia entre procesos de este modelo, consumo por MIR ni una
clave de caché para estas observaciones; tampoco se ha activado ningún guard o
consulta que las use para cambiar diagnósticos o codegen. Las salidas que no
se puedan vincular a locales y los efectos interprocedurales no constituyen un
contrato completo de procedencia.

Esta infraestructura no demuestra la propuesta de disyunción por ordinal, no
resuelve el reemplazo por alias y no convierte el marcador `*` en una prueba
de seguridad.
