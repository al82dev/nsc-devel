# Baseline de campos y construcción genérica

Rama `development-fixes-limitaciones-campos`, revisión inicial
`3f51a03aacd729a55321868ef47f9b7ae817f635`. La base compartida
`d00b3a8698d7750c80887d029d64fda481d67211` contiene trabajo previo sin
certificación. La compilación inicial de los objetivos `nsc`,
`nsc_ns3_t11_f01_typed_introspection_tests` y
`nsc_project_construction_model_tests` terminó con código 0; la configuración
`A:/nsc-limitaciones-campos-evidencia/build/CMakeCache.txt` registra esta copia
como fuente. Las compilaciones posteriores serán incrementales.

El compilador producido tiene SHA-256
`90ab8c405c868574c272faad8b955d3613b186ebde6d546a5383dd9a7b47e251`.
Core binario, construido con él desde `lib/NativeSharp.Core` de esta copia,
tiene SHA-256 `565bf2177c85f88d94057ebe8616f2caeb71e01b7e39610bf815c15b685221c6`;
su metadata tiene SHA-256
`8bd9b7c2bd23b8235c00fbc2df0471907a5979d0791861f5ae0eaca33acefa16`.
Comando y resultado: `A:/nsc-limitaciones-campos-evidencia/build-core.cmd` y
`build-core.log`. Esta identidad no certifica otras configuraciones.

La prueba dirigida `nsc.ns3.t11.f01.typed-introspection` pasó. La prueba
`nsc.project-construction-model` falló en 4 de 71 comprobaciones de diagnósticos.
Las reproducciones originales independientes fallan antes por pérdida de
ownership; copias separadas con retorno `owned` alcanzan los diagnósticos
esperados de constructor. Evidencia:
`A:/nsc-limitaciones-campos-evidencia/construction-owned-controls/results.json`.
Esto explica el bloqueo observado por CLI, pero no certifica los ID internos
del unitario. El fixture original se conserva intacto. No se ha ejecutado una
batería completa.

El ejemplo original 74, con las dos fuentes idénticas a las originales y
proyecto aislado que referencia el Core anterior, compiló y ejecutó O2 con
salida 0. Su salida declara expresamente que enumera los campos a mano; no
demuestra una API general. Comandos, proyecto, imports y resultados están en
`A:/nsc-limitaciones-campos-evidencia/case-74` y en los scripts adyacentes.

En el consumidor de Core binario, `Rc` y `Arc` con un tipo interno cuyo
constructor es implícito superan semántica y fallan en emisión O0 y O2 con
`implicit parameterless constructor is not accessible from this module`.
Los controles de construcción directa y fábrica genérica local emitieron y
ejecutaron correctamente. Nueve positivos emitidos ejecutaron con código 0 tras
inspeccionar sus imports. Negativos de acceso privado, protegido, anidado y
módulo sin referencia se rechazaron. Una expectativa inicial de texto para el
constructor base era incorrecta y se corrigió después de conservar el resultado;
no se cuenta como defecto del compilador. Comandos, hashes, fuentes y salidas:
`A:/nsc-limitaciones-campos-evidencia/generic-baseline.log`,
`generic-execute.log` y `temp/nsc-precompiled-new-jf9b30dn/`.
La matriz inicial falló por las cuatro emisiones y esa expectativa previa.

Un control adicional mostró que el tipo privado anidado usado como argumento
genérico se rechaza incluso desde su propio tipo contenedor, tanto mediante
fábrica local como binaria; el equivalente público pasa. La sonda dirigida
con bibliotecas recompiladas observó que el argumento escrito desde `Outer`
se enlaza con el contenedor léxico `Factory`, lo que causa el rechazo en ese
control. Evidencia: `generic-private-context-discard/results.json` y
`generic-private-origin-context.log` bajo el directorio de evidencia de campos.
Los hits agregados de caché no identifican el sitio que los produjo.

La generación existente sigue imprimiendo etiquetas históricas de versión que
el usuario no considera versiones reales. Es un hallazgo para el integrador;
no se cambió el versionado en este frente.

## Primera ruta en desarrollo

La primera compilación incremental con la ruta provisional de campos terminó
con código 0 y tres warnings de inicialización del cloner, pendientes de
corrección. Con ese compilador la iteración sobre un struct vacío compiló y
ejecutó en O0/O2; el caso heterogéneo falló en parsing de `field.Type item`.
La prueba unitaria ejecutó 20 comprobaciones y falló una: el fixture binario
no alcanzó la especialización. No hubo SKIP en esa corrida. La causa de ese
fallo está en investigación. El compilador usado tiene SHA-256
`0ec454bd163bb2eeec645ac3061588a3b5ca2a5d3cb7030d554fe0045c2e73c9` y el
Core de la prueba era el binario del baseline anterior, no un Core regenerado
con ese compilador. Evidencia: `static-field-first-route/results.json` en el
directorio de evidencia. No se considera cerrada la consulta.

La segunda compilación incremental terminó con código 0. Con su compilador,
SHA-256 registrado en `static-field-second-route/results.json`, los controles
de struct heterogéneo, struct vacío, ámbitos hermanos, receptor y tipo homónimo
compilaron y ejecutaron con código 0 en O0 y O2. El unitario de introspección y
procedencia aprobó 32 comprobaciones sin SKIP; para la ruta binaria utilizó el
Core del baseline anterior. La herencia se rechazó explícitamente con
`cannot verify this type's complete field declarations`; su implementación y
las restantes variantes binarias siguen pendientes. El runner conjunto terminó
con código 1 porque incluye formas aún no implementadas y casos negativos;
no se presenta como una batería aprobada.

Cinco controles dirigidos de flujo pasaron en O0: los saltos hacia la
iteración estática o un bucle exterior se rechazaron, y los saltos de bucles
internos ejecutaron correctamente. Esta guarda es provisional; etiquetas,
anidamientos y la semántica completa de estos saltos siguen pendientes.
Evidencia: `static-field-control-flow/results.json` en el directorio de
evidencia.

En una tanda posterior, el compilador con SHA-256
`13253993e9680d9b8a22986d5c23646919ff7b56b5ecd287b1885aee0508acf5`
compiló y ejecutó en O0/O2 consultas de clase simple, campos homónimos heredados
y los controles privado interior/exterior. `protected`, campos compuestos y
struct genérico seguían rechazados; `internal` está en investigación separada.
El unitario volvió a pasar 32 comprobaciones sin SKIP con el Core del baseline.
Evidencia: `static-field-class-matrix/results.json` en el directorio de
evidencia. Esta matriz no certifica toda la superficie de la consulta.

Con el mismo compilador, el tipo privado anidado fue aceptado dentro de su
contenedor y rechazado fuera, con fábrica local y binaria; los controles
públicos pasaron. Evidencia: `generic-private-context-next/results.json` y
`generic-private-cache-next/results.json`. Las métricas de hit agregadas no
demuestran un hit de la clave concreta, y la inferencia sigue pendiente. La
regla del constructor aún no está conectada.

En una tanda posterior se conectó el certificado únicamente para construcción
placement sin argumentos. Con nsc SHA-256 registrado en `generic-after-fix.log`,
la matriz con Core binario del baseline aprobó emisión y rechazos esperados;
13 positivos, incluidos `MakeRc` y `MakeArc` con constructor implícito en O0 y
O2, ejecutaron con código 0 tras inspeccionar imports. Una segunda matriz
separada con Core regenerado por el mismo nsc también pasó los mismos 13
positivos y negativos. Evidencia: `generic-after-fix.log`,
`generic-after-fix-execute.log`, `generic-sdk-current.log` y
`generic-sdk-current-execute.log` bajo el directorio de evidencia. Aún faltan
los controles de parámetros T/U con el mismo tipo, inferencia y reutilización
del planificador; C3 continúa sin validación en el flujo real.

Se reprodujo por separado el mismo diagnóstico en una fábrica binaria propia
que usa `new T()` ordinario: el consumidor con tipo interno fallaba en O0/O2,
mientras la fábrica local y la construcción directa ejecutaban correctamente.
La corrección se extendió a esa expresión y la matriz dirigida posterior
aprobó emisión, negativos y ejecución de 15 positivos con el nsc cuyo SHA-256
es `f32da00ad292bfc16986313e6fd8086fc202a5cd5e9283e68f7cfcde7c5aab8d`.
Evidencia: `ordinary-generic-factory/results.json`, `generic-ordinary-fix.log`
y `generic-ordinary-fix-execute.log`. Esa primera corrida utilizó Core de la
compilación anterior. Se repitió con Core regenerado por el nsc de la misma
tanda: pasaron emisión, negativos y ejecución de 15 positivos. La metadata
regenerada tiene SHA-256
`785ef1889ee44670bccc46253fe757c7877347b5c46e760a9b4ead82e246be60`.
Evidencia adicional: `generic-ordinary-sdk-current.log` y
`generic-ordinary-sdk-current-execute.log`.

La sonda directa de parámetros con el mismo tipo concreto observó que la
construcción originada en T con `new()` tiene procedencia y certificado,
mientras la originada en U sin esa cláusula tiene procedencia pero no
certificado; ambas especializaciones registraban el mismo tipo canónico.
Evidencia: `certificate-tu-good.log` y `certificate-tu-bad.log`. La sonda
conservó hashes de sus 769 entradas propias; las bibliotecas del sistema
enlazadas por nombre no quedaron identificadas individualmente.

Para una llamada escrita dentro de una plantilla abierta, el nsc identificado
en `generic-private-open-next/results.json` aceptó el tipo privado dentro de su
contenedor y rechazó el uso exterior; el caso válido emitió y ejecutó en O0/O2
(`generic-open-emission/results.json`). Una llamada `MakeRc<Box<T>>` se rechaza
mientras los controles de tipo directo y fábrica local pasan semántica; su
causa está en investigación y no se ha atribuido a una revisión anterior.

La resolución de `MakeRc<Box<T>>` se corrigió para conservar la plantilla
resuelta en el ámbito original. Los casos con import, alias y homónimos en dos
módulos binarios emitieron y ejecutaron O0/O2 con runtime aislado; el caso de
Box local todavía falló durante la construcción implícita. Evidencia:
`generic-sites-emission-20261112T141729405637Z/results.json`. Una primera
corrida con runtime mal configurado se conserva como evidencia de infraestructura
y no se atribuye al compilador. La causa del caso local sigue en investigación.

Con el nsc de SHA-256
`4cab52f637e6fb250816e11ccb7e9e978f92f13890c698f53d45776c27339ab9`,
los controles de campo compuesto simple, alias y tipos homónimos compilaron
y ejecutaron en O0/O2. Evidencia: `compound-fields-next/results.json`.
Estas pruebas no cierran todavía genéricos de tipo ni metadata binaria.

La primera ruta de `Fields(Box<int>)` de fuente compiló y ejecutó en O0/O2
para struct y clase; la sonda `field-clone-probe.log` observó el vínculo
inmediato de ambos campos clonados con sus originales. El unitario pasó 46
comprobaciones sin SKIP. Evidencia: `generic-fields-next/results.json`. No se
afirma cobertura de concurrencia, cadenas de clones, C3 ni Box binario.

Una lambda no genérica de una fábrica binaria fue invocada y construyó el
valor esperado; el consumidor terminó 0 tras verificar el efecto del
constructor. Evidencia: `generic-scope-binary-20261112T145928369466Z/results.json`.
Las variantes con lambda genérica se detuvieron en generación de metadata
antes del certificado y siguen pendientes; no se cuentan como negativas
aprobadas.

Una tanda posterior resolvió tipos privados anidados desde su contenedor:
el caso simple y el de sombra con `field.Type` compilaron y ejecutaron O0/O2.
El acceso exterior se rechazó con un diagnóstico general de selección no
verificable, no con uno específico de privacidad. Evidencia:
`private-fields-next/results.json` en el directorio de evidencia.

El struct binario sencillo compiló y ejecutó en O0/O2 tanto con snapshot
realmente cargado como sin sidecar. La prueba normal obtuvo superficie completa;
una poda forzada conservó el certificado y llegó a `compileProjectMembers`,
pero no demuestra emisión ni poda natural desde la CLI. En una sonda
adversarial, la guarda corregida rechazó certificado ausente o duplicado,
flags desconocidos y offset/alineación alterados; el control válido pasó.
Evidencia: `expanded-fields-final-route/results.json` y
`field-adversarial-next-results.json`. Tipos binarios complejos siguen abiertos.

La matriz reciente de construcción genérica ejecutó 17 positivos y conservó
los rechazos esperados. Los controles mixtos aceptaron y ejecutaron Alpha y
rechazaron Beta privada en ambos órdenes y O0/O2. Evidencia:
`generic-tu-latest.log`, `generic-tu-latest-execute.log` y
`generic-mixed-certificates-20261112T143302792169Z/results.json`.

El fixture permanente ahora incluye Box local y llamadas mixtas en ambos
órdenes; emisión, negativos e inspección/ejecución de positivos pasaron con
el compilador y Core identificados en `generic-box-permanent.log` y
`generic-box-permanent-execute.log`. La corrida usa Core de la tanda anterior;
se repetirá contra los artefactos del estado final.

La instrumentación temporal del planner registró un hit de la clave concreta
de `MakeRc<Outer.Hidden>` en ambas ordenaciones. El rechazo de la llamada
exterior se identificó mediante controles CLI separados; la sonda no incluye
ubicación en su propio diagnóstico. Evidencia: `planner-inside-first.log` y
`planner-outside-first.log`. La instrumentación se retiró después de la sonda;
falta recompilar la versión sin ella.

El acceso ordinario `this` a miembros heredados public/protected y el
ocultamiento compilaron y ejecutaron en O0/O2; miembro inexistente, private de
base y protected externo se rechazaron. También pasaron las pruebas Fields
`Get(this)`, receptor derivado y exclusión externa. Evidencia:
`inherited-this-controls/results.json` y `protected-fields-inherited-fix/results.json`.

Una tanda posterior con compilador SHA-256
`0c6fbd135402f5ff0b96049a62ffb939fb80271a9980236deedf61d09c71fb68`
aceptó el acceso ordinario `internal` desde funciones globales y métodos del
mismo proyecto, tanto en clase como en struct; mantuvo el rechazo de miembros
privados/protegidos en los controles y de `internal` desde otro módulo.
La consulta de un campo `internal` del mismo proyecto compiló y ejecutó en
O0/O2, y pasaron los controles previos de clases y herencia. El consumidor
de Fields de una biblioteca binaria sigue rechazado por insuficiencia de la
selección actual. Evidencia: `ordinary-field-access-next`,
`internal-fields-next` e `internal-fields-binary-next` en el directorio de
evidencia. La primera sonda de caché C3 no produjo entradas ni eventos y no
constituye evidencia de hit o miss; se investiga su configuración.

Una sonda posterior construyó el modelo genérico directamente con una raíz de
caché C3 aislada: generó entradas C1, pero no entradas ni eventos C3. La lectura
del código encontró que la ruta C3 entrega claves textuales a `TemplateParseCache`,
que requiere 64 caracteres hexadecimales. Además no se localizó una llamada del
flujo de `ProjectCompilation` que active la raíz C3 del modelo genérico. La
evidencia está en `c3-direct-20261112T132307338479Z` bajo el directorio de
evidencia. No se conectó ni reparó esa caché independiente; sus hits no están
verificados. La ausencia del vínculo de procedencia deja el nuevo certificado
sin autorizar, preservando el rechazo previo.
