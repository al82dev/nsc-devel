# Caso 74: fase, identidad y alcance de una consulta de campos

## Estado de los límites observados

El archivo `TAREAS-CHAT-CAMPOS.md` exige una consulta tipada real, con identidad,
visibilidad, diagnósticos y regresiones; las pruebas dirigidas documentadas aquí
demuestran rutas concretas, no la cobertura general del caso ni una batería
completa. El ejemplo original usa nombres de campos explícitos y no prueba por
sí mismo enumeración arbitraria.

- **Cuerpo no instanciado:** el compilador valida formas sintácticas y saltos
  aun con selección exterior vacía, pero el ejemplo con receptor interior
  incompatible compila y ejecuta al no producirse ninguna instancia. La
  comprobación semántica de un receptor dependiente sin un campo concreto
  todavía no está demostrada; requiere definir qué errores se exigen en código
  no instanciado y cómo obtener tipos sin fabricar una selección. Evidencia:
  `A:/nsc-limitaciones-campos-evidencia/shadow-type-controls-20261112T192340817568Z/results.json`.
- **Clonado y caché:** la ruta anidada conserva el vínculo de accesos en las
  pruebas ejecutadas, pero no acredita restauración C3, todas las cadenas de
  clones ni concurrencia. La ausencia de esa evidencia impide generalizar los
  resultados a reutilización de caché o ejecución paralela.
- **Formas del cuerpo:** usos de `nameof` fuera del subconjunto acreditado y sombras por
  declaraciones ordinarias siguen siendo rechazados conservadoramente por el
  rewriter. La sombra por otro `static foreach` sí está implementada como se
  describe abajo. Los saltos sin etiqueta a la iteración estática se admiten en
  las formas probadas; los etiquetados exteriores se admiten cuando su destino
  léxico es inequívoco y el valor de `break` puede validarse. No se ha acreditado todo control de flujo
  ni todas las proyecciones de tipos en anidamientos.
- **Declaraciones y representación:** la selección falla cerrada cuando no
  puede acreditar jerarquía, visibilidad, tipo o layout. Las rutas aprobadas de
  campos compuestos y metadata se limitan a las formas efectivamente probadas;
  no constituyen soporte de todas las declaraciones, layouts o certificados.
  Metadata concordante tampoco acredita autenticidad externa.
- **Proyectos distintos:** el control ordinario de una base `internal` fuente
  rechaza visibilidad, mientras que la consulta probada falla antes por
  jerarquía; esa corrida no acredita la guarda de visibilidad de Fields. Una
  proyección `field.Type` fuera de ámbito se rechazó en el parser, no en un
  análisis semántico de escape. Referencias a sus resultados están en el
  registro `static-field-query-test-log.md`.

Estas son limitaciones o áreas no verificadas del caso 74, no afirmaciones de
que todas sean defectos reproducidos. La construcción genérica con Core
binario es el segundo trabajo del archivo de tareas y se evalúa por separado.

## Actualización de las primeras pruebas

Las hojas nominales fuente en una tupla se resuelven en el contexto del campo,
se contrastan por identidad y visibilidad desde el consumidor y se proyectan
en una copia estructural de la arena de la unidad. El runner pasó O0/O2 con
tipos públicos homónimos y un tipo privado dentro del dueño:
`A:/nsc-limitaciones-campos-evidencia/temp/nsc-source-fields-m4rf02z2/results.json`.
Fuera del dueño, el control ordinario rechaza privacidad y Fields se detiene
antes por declaración incompleta; no se atribuye ese rechazo a la guarda de
tipo. El unitario pasó 85 checks sin SKIP:
`A:/nsc-limitaciones-campos-evidencia/temp/nsc-binary-fields-8cym7fw0/results.json`.
La afirmación posterior de rechazo de tuplas nominales describe la tanda anterior.

Los campos tupla fuente con hojas builtin sin modificadores se seleccionan por
estructura, incluidos elementos nombrados; Name/Get/Type y receptor incorrecto
pasaron O0/O2:
`A:/nsc-limitaciones-campos-evidencia/temp/nsc-source-fields-ztzb_2jb/results.json`.
Tuplas con elementos nominales públicos o privados siguen rechazadas antes de
visibilidad; para proyectar `field.Type` debe conservarse la identidad de cada
hoja en el scope del consumidor, no reutilizar grafías potencialmente ambiguas.
Evidencia `A:/nsc-limitaciones-campos-evidencia/source-tuple-fields-20261112T205300431586Z/results.json`.

`nameof(field)`, `nameof(field.Name)` y `nameof(field.Get(...))` producen los
nombres léxicos `field`, `Name` y `Get`, no el nombre del campo seleccionado.
Como en el control ordinario de llamada, los argumentos de `nameof` no se
evalúan ni se comprueban por esa operación. El runner O0/O2 verificó cadenas,
sombra, ausencia de efecto lateral y rechazos de miembro inexistente/fuera de
ámbito: `A:/nsc-limitaciones-campos-evidencia/temp/nsc-source-fields-0_68q3wy/results.json`.
El unitario con certificados pasó 85 checks sin SKIP:
`A:/nsc-limitaciones-campos-evidencia/temp/nsc-binary-fields-dgnvk1k3/results.json`.
Las formas anidadas no acreditadas y `Get` genérico se conservan rechazados.

Los saltos a una etiqueta exterior verificada se dejan al análisis ordinario
para conservar destino y limpieza. Una pila léxica por scope transporta la
identidad de cada loop y se reinicia en funciones y lambdas; la cola diferida
conserva ese contexto. El runner pasó O0/O2 con limpieza, anidamiento, expansión
diferida y negativos de etiqueta inexistente o lambda:
`A:/nsc-limitaciones-campos-evidencia/temp/nsc-source-fields-7wjqa2vv/results.json`.
El unitario pasó 85 checks sin SKIP:
`A:/nsc-limitaciones-campos-evidencia/temp/nsc-binary-fields-w547xewm/results.json`.
Con selección vacía se rechaza conservadoramente un `break` exterior con valor,
pues no se acredita su tipo semántico. La afirmación histórica posterior de
que todas las etiquetas externas se rechazan describe una tanda anterior.

El contrato de flujo adoptado para `static foreach` es `break` para salir de
la iteración y `continue` para pasar a la siguiente copia. Se preservan los
destinos de loops y switch interiores; las pruebas O0/O2 incluyen `defer`,
`finally` y destructores y terminaron 0:
`A:/nsc-limitaciones-campos-evidencia/temp/nsc-source-fields-lngbuw5p/results.json`.
El unitario de flujo y certificados pasó 82 checks sin SKIP:
`A:/nsc-limitaciones-campos-evidencia/temp/nsc-binary-fields-z402vjw8/results.json`.
Saltos etiquetados hacia el exterior continúan rechazados y queda sin probar
colisión de etiquetas para clones de un mismo origen simultáneamente anidados.
El análisis de cuerpos ordinario puede resolver etiquetas y limpieza tras la
expansión, pero delegar sin verificación previa perdería el diagnóstico de una
etiqueta inexistente cuando la selección es vacía. El alcance genérico actual
no transporta etiquetas activas; una posible solución requiere propagación
léxica por ruta, límites de función/lambda y conservación de ese contexto en
expansiones diferidas. Queda por especificar la comprobación de valores de
`break` en cuerpos no instanciados. Las etiquetas existentes e inexistentes,
incluida selección vacía, se rechazaron con la guarda actual:
`A:/nsc-limitaciones-campos-evidencia/outer-label-controls-20261112T201957551497Z/results.json`.

Las lambdas genéricas con parámetros exclusivamente de tipo y sin constraints
se admiten en el subconjunto medido. El runner dirigido pasó O0/O2 con
`field.Name` e `identity<field.Type>` sobre int/long y controles negativos de
receptor, sombra y descriptor aislado:
`A:/nsc-limitaciones-campos-evidencia/temp/nsc-source-fields-owgfd0eb/results.json`.
Una lambda genérica que proyecta `field.Get` para U=A y su control ordinario
`value.X` rechazan inferencia de retorno aun con `-> int`; esta ruta positiva
no está acreditada ni se atribuye exclusivamente a Fields. Evidencia
`A:/nsc-limitaciones-campos-evidencia/generic-field-lambda-20261112T194615983862Z/results.json`.
Constraints y parámetros const/pack no se habilitaron.

Las lambdas no genéricas en el cuerpo de Fields se admiten en las formas
probadas: sin descriptor, con `field.Get` sobre parámetro y con captura explícita
del receptor. El runner fuente pasó O0/O2 y rechazó usar el descriptor aislado:
`A:/nsc-limitaciones-campos-evidencia/temp/nsc-source-fields-5bd5c2vi/results.json`.
Los saltos `break`/`continue` dentro de lambda siguen rechazados para no usar
destinos de un ámbito exterior. Un control ordinario y otro con Fields que
transfieren una closure fuera del bloque terminaron con `0xC0000374` O0/O2;
`A:/nsc-limitaciones-campos-evidencia/field-lambda-20261112T193510098130Z/results.json`.
Ese hallazgo no se atribuye exclusivamente a Fields y no acredita lifetime de
closures escapadas. Las referencias históricas al rechazo de lambdas describen
tandas anteriores.

Decisión de contrato para sombra: un descriptor declarado por una iteración
interior oculta al homónimo exterior solo dentro de su cuerpo; el tipo de la
cabecera se resuelve en el ámbito exterior, que se restaura después. El runner
fuente pasó O0/O2 con tipos diferentes, cabecera `Fields(field.Type)`,
restauración y rechazo de receptor incompatible:
`A:/nsc-limitaciones-campos-evidencia/temp/nsc-source-fields-cq6t6iwi/results.json`.
Una proyección fuera de ámbito se rechazó en parser y no acredita comprobación
semántica de escape. Con exterior vacío un receptor incompatible del interior
no instanciado compiló y ejecutó; la validación sintáctica no prueba validación
semántica de ese receptor. Evidencia
`A:/nsc-limitaciones-campos-evidencia/shadow-type-controls-20261112T192340817568Z/results.json`.
Las afirmaciones históricas de rechazo de sombra más abajo corresponden a
tandas anteriores.

Iteraciones de Fields anidadas con descriptores diferentes pasaron en dos y
tres niveles, incluido un campo heredado oculto cuyo valor distingue su
identidad. El cuerpo interno se valida incluso si la selección exterior es
vacía, y los accesos seleccionados conservan procedencia durante el clonado.
El runner dirigido terminó 0 en O0/O2:
`A:/nsc-limitaciones-campos-evidencia/temp/nsc-source-fields-c23ntggy/results.json`.
El shadowing del descriptor sigue rechazado; C3, todas las formas de tipos y
todos los destinos de control de flujo anidados no están acreditados.

`Box : Alpha.Base<int>` con base pública fuente en otro proyecto referenciado
se selecciona tras la materialización Ready, cotejando la identidad de la
plantilla y su visibilidad desde el proyecto de Box. El runner fuente terminó
0 en O0/O2 con homónimos, ocultación y receptores incompatibles:
`A:/nsc-limitaciones-campos-evidencia/temp/nsc-source-fields-cib0tcmj/results.json`.
El control de base internal rechaza acceso ordinario; Fields falla antes por
jerarquía, por lo que no acredita el mismo diagnóstico de visibilidad. Esta
prueba no cubre jerarquías múltiples ni otras formas de base no verificadas.

`Box<T> : Base` con ambas declaraciones fuente en el mismo proyecto se acepta
ahora tras acreditar el contexto y la identidad de Base y aplicar la selección
recursiva ordinaria. El runner fuente versionado pasó O0/O2 con ocultación,
homónimos y rechazos de receptor y acceso privado; evidencia
`A:/nsc-limitaciones-campos-evidencia/temp/nsc-source-fields-jkc1t86_/results.json`.
La reproducción anterior que rechazaba la identidad está en
`A:/nsc-limitaciones-campos-evidencia/local-nominal-base-20261112T184526211256Z/results.json`.
Este resultado no demuestra bases fuente de otro proyecto.

La ruta acotada `Box<T> : Base<T>` con ambas declaraciones fuente en el mismo
proyecto pasó O0/O2 en el runner fuente, incluyendo ocultación, homónimos en
ambos órdenes y rechazo de receptores incompatibles. Antes se rechazaba por la
guarda de base genérica; los comandos y resultados anteriores y posteriores
están en `local-generic-inheritance-20261112T184005345516Z/results.json` y
`temp/nsc-source-fields-m9yy19wj/results.json`, bajo
`A:/nsc-limitaciones-campos-evidencia/`. Se exige resolución única y visible,
plantilla fuente del mismo proyecto y especialización Ready de clase con clave
y sintaxis verificadas. La base nominal local y bases fuente de otro proyecto
no quedaron cubiertas por esa tanda.

Para una base nominal binaria de otro módulo, una cadena de certificados y
dependencias por identidad permite los casos medidos, incluidos homónimos y
receptores incompatibles; el runner `temp/nsc-binary-fields-uhs678me/results.json`
terminó 0. Una dependencia duplicada en copia sintética se normalizó por el
codec y no probó la guarda de duplicados; un certificado duplicado en memoria sí
se rechazó (`nominal-external-certificates-20261112T183339018828Z/results.json`).
Estas pruebas no acreditan autenticidad de metadata externa. Las descripciones
de estado anteriores en los párrafos históricos siguientes se interpretan en el
momento de cada tanda.

`Box<T> : Base<T>` con Base en otro módulo binario pasó O0/O2 con y sin
sidecars, incluso con una Base homónima de otro módulo. La plantilla se
selecciona por resolución inequívoca y visibilidad del módulo productor, y se
exigen identidad Ready y certificado; la sonda artificial de cinco mutaciones
rechazó los casos esperados. El runner ampliado terminó 0:
`temp/nsc-binary-fields-sk0f4v_t/results.json`. Los controles de base internal
o sin referencia no llegaron a Fields porque el productor falló al parsear;
esto no demuestra esa guarda. La herencia binaria no genérica entre módulos
requiere otra ruta de certificado y sigue pendiente de reproducción.

La ruta acotada `Box<T> : Base<T>` del mismo módulo binario pasó los controles
O0/O2 con/sin sidecar, ocultación, miembros privados filtrados y módulos
homónimos en ambos órdenes. Receptores incompatibles y acceso privado ordinario
se rechazaron. El runner versionable ampliado terminó 0:
`temp/nsc-binary-fields-86r7wedn/results.json`; la sonda artificial de
certificados está en `generic-base-certificates-20261112T180216404533Z/results.json`.
La pista de nombre usada por el parser independiente solo desambigua sintaxis:
la plantilla, la clave Ready y el certificado se comprueban por identidad.
Esto no valida metadata externa adulterada.

La primera ruta de herencia genérica binaria admite `Box<T> : Base` del mismo
módulo, con base no genérica certificada. La comparación comprueba base en la
fuente de metadata, AST transportado y clon, y selecciona primero los campos
de Base. Tres variantes, incluida la ocultación homónima con valores 3 y 7,
pasaron O0/O2 con y sin sidecar; dos receptores incompatibles se rechazaron.
Evidencias `generic-inherited-baseline-20261112T172602667562Z/results.json` y
`generic-inherited-negative-20261112T172602659470Z/results.json`. Una sonda
artificial rechazó certificado Base ausente, duplicado o con identidad alterada
(`inherited-certificate-probe-20261112T172834310730Z/results.json`); una
repetición también rechazó alteración artificial del template
(`inherited-certificate-probe-20261112T173132537647Z/results.json`). No son pruebas
de metadata externa maliciosa. Bases genéricas, interfaces, base de otro módulo
y jerarquías sin evidencia suficiente siguen pendientes.

Los runners dirigidos de fuente y binario terminaron 0 con el compilador actual:
`temp/nsc-source-fields-5hygarx4/results.json` y
`temp/nsc-binary-fields-ae1uho33/results.json`. El runner binario comprueba
explícitamente fallback por poda para protected directo y snapshot cargado
para el transitivo. La corrida previa, con dos expectativas ya obsoletas, se
conserva en `temp/nsc-binary-fields-zssijphy/results.json`; el caso binario
ajeno se ejecutó también de forma independiente en O0/O2.

Fields(Base) desde derivadas binarias directa y transitiva pasó en O0/O2
cuando cada campo no sintético de la cadena tenía acceso acreditado. Get con
receptor base incompatible fue rechazado; un campo privado sin miembro metadata
conservó el rechazo conservador (`binary-protected-full/results.json` y
`binary-hierarchy-private-final/results.json`). La sonda adversarial específica
de `accessKnown`/protected sigue pendiente; los controles funcionales no la
sustituyen. Herencia genérica binaria y metadata de acceso insuficiente siguen
pendientes.

La consulta de una clase binaria sin bases desde función libre funciona para
campos `int`/`long` con layout natural certificado. Con snapshot instalado y
sin sidecar, el control ejecutó O0/O2 y excluyó tanto un campo privado como un
backing sintético; evidencia `binary-class-next/results.json`. La prueba previa
había reproducido el rechazo aun cuando el control ordinario ejecutaba 0
(`binary-class-free/results.json`). En esa primera tanda se rechazaban las
consultas desde métodos y derivadas; clases con bases/interfaces, otros tipos
de campo y layouts no naturales siguen rechazadas: el metadata de una superficie externa no basta por sí solo para
distinguir todas las reglas de protected. Falta la sonda adversarial de esta
extensión; no debe confundirse con soporte general de clases binarias.

Posteriormente se preservó el dueño AST exacto al analizar métodos genéricos.
La consulta desde `Inspector.Fold<T>` sin bases ni interfaces compiló y ejecutó
O0/O2 con snapshot; sus controles rechazaron acceso directo al campo privado.
Dueños con bases o interfaces siguen rechazados conservadoramente aunque sus
controles ordinarios pasan (`binary-class-scope-current/results.json`). La ruta
desde función libre se volvió a ejecutar con y sin sidecar O0/O2
(`binary-class-free-owner/execution-results.json`).

Una ampliación posterior recorre bases de fuente por identidad AST, con límite
y rechazo ante parcial, ambigüedad, base binaria/genérica o ciclo. Dueños con
bases fuente ajenas directa/transitiva y homónimos calificados pasaron O0/O2;
dueños derivados de Record directa/transitivamente, base binaria ajena y
partial siguen rechazados con controles ordinarios exitosos. El control
ordinario del ciclo ya falla por herencia cíclica, por lo que no demuestra la
guarda Fields. Evidencia `class-owner-boundaries-results/results.json`.

Una sonda adversarial en memoria alteró el offset del certificado de Record y
el consumidor lo aceptó: el offset no se expresaba en la sintaxis reconstruida.
Se añadió un segundo cálculo acotado de offsets, tamaños y alineación natural
en el consumidor, con rechazo de overflow; el build incremental terminó 0.
La regresión de ejecución sigue pendiente. La mutación era del certificado
interno, no una prueba de metadata externa alterada.

La repetición artificial rechazó offset, tamaño de campo, tamaño y alineación
de clase modificados, así como `align` alterado específicamente en First;
el control aceptó. Evidencia `class-adversarial-next-results.json` y
`class-sizes-results.json`. Falta repetir la ejecución de la clase con el
compilador definitivo; la prueba no demuestra metadata externa maliciosa.

Una primera consulta Box<int> transportada desde biblioteca compiló y ejecutó
en O0/O2 (`binary-generic-certified/results.json`), con imports inspeccionados.
El certificado se asocia a la clave de plantilla, compara la metadata completa
con la superficie retenida y reparsea perezosamente `sourceText`; contrasta
parámetros y campos simples del AST transportado antes de comprobar el clon
Ready. Clases/structs sin owner, herencia ni constraints y campos básicos o
parámetros sin modificadores son el subconjunto pretendido. Metadata
concordante no acredita autenticidad externa. Faltan controles adversariales,
identidades homónimas entre módulos, rutas de snapshot/poda, C3 y otras formas.

Una sonda artificial posterior rechazó certificado genérico ausente/duplicado,
clave o fuente alterada y cambios de orden, visibilidad o tipo en el AST;
`generic-adversarial-results.json`. Box sin sidecar ejecutó O0/O2
(`generic-nosidecar-next/results.json`). No se ha ejercitado poda natural ni
metadata maliciosa real. La herencia y el acceso protected en especializaciones
genéricas continúan pendientes: se añadió un rechazo explícito fuera del propio
tipo para evitar omitir miembros protegidos; aún falta validar esa guarda.

Para un campo público de tipo interno procedente de un binario, Name y Get
inferido compilaron y ejecutaron en O0/O2, al igual que el control ordinario
con `var`. La proyección explícita `field.Type` se rechaza en el sitio de la
consulta con `static field type is not accessible in this context`; el nombre
explícito del tipo también se rechaza sin Fields. Evidencia:
`A:/nsc-limitaciones-campos-evidencia/private-binary-modes-diagnostic/results.json`.

La primera ruta de structs y clases genéricas locales `Box<int>` pasó en O0/O2
tras diferir la consulta hasta una especialización Ready; una sonda confirmó
vínculos inmediatos de sus dos campos. Aún faltan cadena de clones,
concurrencia, C3 y genéricos transportados desde biblioteca.

La primera ruta certificada de struct binario con campos básicos compiló y
ejecutó O0/O2 tanto con snapshot efectivamente instalado como sin sidecar.
Se cotejan metadata completa y superficie retenida, campos por orden,
visibilidad, flags, tipo y layout reconstruido. El backing de propiedad fue
observado en metadata con flag 1024 y reconstruido como campo privado con otro
nombre, sin marca `generated` en el modelo: no se infiere su condición por el
nombre. Evidencias `expanded-fields-final-route/results.json`,
`field-snapshot-probe.log` y `field-ast-nosidecar-current.log`.

Una sonda artificial en memoria rechazó certificados ausentes/duplicados y
alteraciones de offset/alineación, pero reveló aceptación de un flag desconocido
introducido directamente en el certificado. Se añadió una guarda para rechazarlo;
falta volver a compilar y repetir la sonda. Esto no demuestra aceptación de
metadata maliciosa real. La consulta normal observada no podó la superficie;
está pendiente la prueba artificial de poda y su distinción del flujo normal.
Clases binarias, campos binarios compuestos y plantillas transportadas siguen
fuera de este subconjunto y pendientes del caso.

La proyección de campos nominales sencillos usa ahora identidad calificada
verificada. Los controles de struct compuesto, alias y tipos homónimos
compilaron y ejecutaron O0/O2 (`compound-fields-next/results.json`). También
compiló y ejecutó una especialización invocada con nombre simple, calificado y
`field.Type` (`compound-identity-next/results.json`); no se midió reutilización
de caché. Un tipo privado anidado aceptado sin Fields sigue rechazado por la
consulta (`compound-identity-ordinary/results.json`) y permanece pendiente.

Los controles de `this` heredado ordinario y `field.Get(this)` protected
pasaron O0/O2 tras diferir la comprobación temprana cuando existen bases. Se
verificaron también miembro desconocido, ocultamiento y accesos prohibidos:
`inherited-this-controls/results.json` y
`protected-fields-inherited-fix/results.json`. Genéricos de tipo, metadata
binaria y contextos todavía no demostrados siguen pendientes.

El fallo ordinario de acceso `internal` desde funciones libres se reprodujo y
se corrigió propagando el proyecto del símbolo de función. Los controles tras
el cambio aceptan el acceso ordinario en el mismo proyecto desde función libre
y método (class/struct), rechazan private/protected y rechazan internal de otra
biblioteca. La consulta `Fields` sobre internal del mismo proyecto compiló y
ejecutó O0/O2; sus controles de clase, herencia y private también pasaron.
Evidencia: `A:/nsc-limitaciones-campos-evidencia/ordinary-field-access-next`,
`internal-fields-next` e `internal-fields-binary-next`. El binario enumerado
sigue rechazado por procedencia no verificada.

Se añadió después una comprobación temprana de herencia del tipo léxico para
selección `protected`: no está compilada ni probada todavía. El acceso
generado debe comprobar de nuevo la regla del receptor en semántica.

Una tercera tanda compiló la extensión de jerarquía. Los controles dirigidos
de clase simple, campos homónimos ocultos en herencia y visibilidad privada
dentro y fuera del dueño compilaron y ejecutaron en O0 y O2. El acceso
`protected` exterior, los campos compuestos y el struct genérico siguen
rechazados expresamente; `internal` requiere un control independiente. El
unitario pasó 32 comprobaciones. Evidencia:
`A:/nsc-limitaciones-campos-evidencia/static-field-class-matrix/results.json`.
Esta tanda reemplaza las afirmaciones anteriores de que esa extensión no fue
compilada; no representa el cierre de la consulta de campos.

Segunda tanda: el unitario pasó 32 comprobaciones, incluida la preparación de
metadata binaria real, sin omisiones; el Core de la prueba procede del build
baseline anterior. Los programas de campos heterogéneos, selección vacía,
ámbitos hermanos, receptor y tipo homónimo compilaron y ejecutaron en O0 y O2.
El descriptor inválido en una selección vacía fue rechazado. Evidencia y
comandos: `A:/nsc-limitaciones-campos-evidencia/static-field-second-route/results.json`.
Cinco controles de flujo en O0 pasaron; faltan etiquetas y anidamientos:
`A:/nsc-limitaciones-campos-evidencia/static-field-control-flow/results.json`.
El consumidor de biblioteca binaria sigue rechazado en O0 y O2 por el límite
explícito de procedencia (`static-field-binary-route/results.json`).

Después de esas pruebas se agregó código para jerarquías de clases de fuente
no genéricas y campos básicos; todavía no se ha compilado ni probado. Rechaza
partial, bases no verificables y acceso `protected` que requiera comprobar
herencia. La ruta binaria y el resto de variantes siguen pendientes.

Se compiló incrementalmente el compilador con el prototipo restringido. La
prueba dirigida de un struct vacío compiló y ejecutó en O0 y O2; la prueba
heterogénea se detuvo en el parser al declarar `field.Type item`. Los resultados
están en `A:/nsc-limitaciones-campos-evidencia/static-field-first-route/results.json`.
Se ajustó posteriormente el reconocimiento temporal del nombre calificado
`field.Type` en ambos caminos del parser; ese ajuste todavía no se ha compilado
ni probado. Las secciones siguientes conservan el razonamiento anterior: sus
afirmaciones de ausencia total de build o pruebas describen aquel momento.

El prototipo sigue limitado a structs no parciales de fuente con campos de
tipos básicos. El paso por O0/O2 del caso vacío no valida acceso ni tipos
heterogéneos. Por inspección falta además comprobar que un cuerpo inválido se
diagnostique incluso cuando la selección está vacía; se pidió una prueba
dirigida. Clases, herencia, binarios y las demás variantes siguen pendientes.

Estado: investigación estática y parser en desarrollo. El target incremental
`nsc_frontend` terminó con código 0 mediante
`A:/nsc-limitaciones-campos-evidencia/build-frontend-check.cmd`; la salida quedó
en la sesión de herramienta, no en un log completo en disco. Esto no demuestra
una compilación ejecutable ni una prueba de la construcción. Desde entonces se
activó una expansión provisional restringida, sin compilar ni probar. El caso
74 sigue pendiente.

Después de ese build se añadieron cambios aún no compilados: procedencia
explícita de unidades, rewriter sobre clones, productor limitado a structs de
fuente no parciales con campos simples, y comprobaciones de selección completa
y binding por identidad. La ruta no está compilada ni probada; clases,
binarios, genéricos de tipo, campos complejos, `nameof` del
descriptor, anidamientos y shadowing siguen pendientes. Una expansión posee un
presupuesto de nodos para evitar crecimiento descontrolado; el límite y sus
diagnósticos deben verificarse con pruebas dirigidas.

## Objetivo y límites

El programa original nombra los campos y no demuestra enumeración. Se necesita
una operación estática que permita consultar campos de tipos reales y emitir
accesos tipados sin un registro de offsets en runtime. La evaluación debe usar
la identidad del tipo seleccionado, respetar visibilidad y diagnosticar
información insuficiente. La reproducción original debe permanecer intacta.

La reproducción de control en O2 compiló y ejecutó con salida 0 usando el
compilador y Core binario de esta copia. Las dos fuentes `.ns` se compararon
byte a byte con los originales; el proyecto copiado usa una `LibraryReference`
binaria. El comando está en `A:/nsc-limitaciones-campos-evidencia/case-74-build.cmd`
y los registros de build, imports y ejecución en la carpeta `case-74`. SHA-256:
compilador `90ab8c405c868574c272faad8b955d3613b186ebde6d546a5383dd9a7b47e251`,
Core `565bf2177c85f88d94057ebe8616f2caeb71e01b7e39610bf815c15b685221c6`,
metadata `8bd9b7c2bd23b8235c00fbc2df0471907a5979d0791861f5ae0eaca33acefa16`.
La salida indica que los campos están enumerados manualmente; este control no
demuestra la operación general solicitada.

Este frente no modifica interoperabilidad C/C++, Daikon, fuentes LLVM, la
instalación funcional ni los archivos asignados al integrador. La validación
autorizada consiste en compilaciones incrementales y pruebas dirigidas.

## Propuesta de superficie, todavía no implementada

La construcción contextual en desarrollo es `static foreach (field in Fields(T))`.
Dentro de su cuerpo, `field.Name` representaría el nombre constante;
`field.Type`, el tipo exacto; y `field.Get(receiver)`, un acceso tipado al
miembro. El parser fue modificado para reconocer esa forma, pero las operaciones
del descriptor siguen siendo una propuesta sin implementar ni validar.
La elección debe comprobar parsing, enlace y especialización antes de adoptarse.
Cada uso de `Get` evaluaría su receptor una vez y conservaría sus restricciones
de escritura, préstamo y vida. Cada campo necesita una copia de cuerpo con
identidad y ámbito propios. El `return` ordinario conserva su significado; los
destinos de `break` y `continue` requieren especificación y diagnóstico explícitos.

La consulta seleccionaría por identidad y contexto de acceso, no por nombres
generados ni índices físicos. Los campos sintéticos no serían tratados como
declaraciones de usuario sin una política específica verificable. Herencia,
ocultamiento, tipos genéricos y dependencias binarias requieren los controles
siguientes antes de garantizarse; no se cierran por implementar una variante
limitada a un struct de fuente.

## Orden de fases y dependencias

En `src/Frontend/ProjectCompilation.cpp`, `compileProjectGenerics` ejecuta
`genericModel_->build` antes de `compileProjectNames`; posteriormente se crean
el modelo de miembros, herencia y constraints, y se analizan cuerpos. El
catálogo `ProjectMetaprogrammingModel` se construye después del layout, que a
su vez depende de los cuerpos. `#run` se evalúa antes del lexing. Por tanto:

1. El catálogo actual no puede consultarse directamente desde `#run` sin
   reordenar dependencias. `ProjectMemberModel` ofrece información anterior
   al análisis de cuerpos, pero aún no resuelve por sí solo la expansión.
2. Una expansión que introduzca `Format<field.Type>()` después de finalizar
   genéricos puede exigir una especialización que no existía al descubrir las
   demandas. Se necesita demostrar un punto fijo de demandas y reconstrucción
   de índices, o definir otra representación que no pierda especializaciones.
   Aún no se ha demostrado ninguna de las dos soluciones.
3. `ProjectNameLookup` se construye antes de cuerpos. Una proyección como
   `field.Type` necesita binding diferido dentro del ámbito de la iteración y
   un enlace posterior a la identidad real, sin aceptar nombres no resueltos
   en otros ámbitos.
4. `GenericCloner.cpp` clona nodos y tipos para especializaciones. Cualquier
   nuevo nodo y sus bindings deben sobrevivir clonación y restauración. Este
   archivo también pertenece al frente de construcción genérica y se debe
   coordinar antes de editarlo.
5. `ProjectMethodBodyModel` y HIR indexan información por punteros a sintaxis.
   Reutilizar una misma copia para campos heterogéneos confundirá bindings;
   las copias deben permanecer vivas durante análisis y lowering. El análisis
   de cuerpos usa workers: no se debe compartir una arena mutable sin coordinar.

## Acceso y herencia

`ProjectMemberAccessibility::isAccessible` comprueba `private` por tipo
declarante, `internal` por proyecto y `protected` por herencia y tipo del
receptor. `ProjectMetaprogrammingModel::visibleMembers`, en cambio, incluye
todo miembro del mismo proyecto y usa un buffer mutable reutilizado. Ese método
no representa el contrato de acceso requerido ni proporciona una vista
estable para consultas concurrentes.

`ProjectMethodBodyModel::memberCandidatesForType` busca desde el tipo receptor
hacia la base y se detiene en la primera clase con el nombre. Reescribir un
descriptor heredado como una mera expresión textual `receiver.Nombre` puede
seleccionar un campo ocultador distinto. Debe preservarse la identidad del
miembro seleccionado y comprobarse accesibilidad con el receptor real antes
de generar su acceso. El orden de enumeración entre bases y derivadas debe
especificarse y probarse con campos homónimos, clases sin campos y acceso
protegido permitido y rechazado.

La selección propuesta evalúa accesibilidad léxica al enumerar; el control de
`protected` que depende del tipo de un receptor concreto se repite en cada
`Get` con ese receptor. En particular, conocer el nombre de un campo protegido
no autoriza acceder mediante un objeto cuya clase estática no cumple la regla.
La identidad incluye el tipo declarante, por lo que dos campos homónimos de
base y derivada no son el mismo descriptor. Este contrato es diseño pendiente.

El análisis de acceso ordinario en `MemberAccessAnalyzer.cpp:766–849` toma el
ordinal elegido, aplica las reglas de movimiento, escritura, `readonly` y
préstamo, y registra ese ordinal en `MethodNameBinding`. En el lowering
`HirNameExpressionLowering.cpp:215–243` se consume ese ordinal y se baja el
receptor una vez. Una ruta nueva podría validar la selección tipada y entregar
ese único ordinal a la misma lógica. Esto es una propuesta de integración
estática. `MirNodeMachine.cpp:1595–1628` vuelve a consultar el layout mediante
ese ordinal para obtener offset y alineación, sin elegir por nombre. Todavía
deben seguirse backend y verificarse la ejecución, incluido un campo heredado
oculto, antes de afirmar que funciona.

## Información binaria

`LibraryMetadataField` conserva nombre, clave de tipo, flags y layout, pero no
un campo explícito con la identidad del tipo declarante; el registro que lo
contiene también aporta contexto. La construcción de metadata copia esa
información desde el layout de structs y clases. El layout ordinario de una
clase parte del tamaño de su base y recoge `membersOfType` para el tipo propio;
la colección ordinaria añade cada miembro a su tipo. Esto favorece recuperar el
propietario desde el registro, pero todavía debe verificarse para snapshots y
las demás rutas. En la
reconstrucción de fuente, `PublicSurfaceBuilder.cpp:861–875` consulta el
miembro por propietario y nombre, renombra algunos backing fields y asigna
`private` cuando no encuentra el contrato. Esto no demuestra por sí solo que
se preserve la identidad de campos heredados, ocultos o sintéticos.

Las unidades de plantillas reconstruidas desde binarios pueden tener
`isPrecompiled == false`; esa bandera y la existencia de sintaxis no prueban
origen en fuente original. La implementación necesita conservar o reconstruir
procedencia e identidad con evidencia suficiente, y rechazar con diagnóstico
la información que no pueda demostrar. Se requieren comparaciones entre fuente
y biblioteca, incluidos miembros homónimos y visibilidades. Esto es una
inspección estática; no afirma que exista un fallo binario reproducido.

## Pruebas dirigidas requeridas

- Programa original intacto, con comando, procedencia del compilador y salida.
- Enumeración desde un programa NativeSharp real, con tipos heterogéneos,
  vacíos, estáticos, sintéticos, no accesibles y orden comprobado.
- Instanciación genérica que produzca llamadas dependientes del tipo del campo,
  con especialización reutilizada y sin depender del primer sitio de uso.
- Herencia con ocultamiento homónimo; contextos `private`, `internal` y
  `protected` y recepción compatible e incompatible.
- Fuente y Core/biblioteca binaria con los mismos casos, incluyendo metadata
  incompleta y diagnósticos. Comprobar que no se acepta una lista parcial.
- Una evaluación del receptor por uso; préstamos, readonly, escapes y
  control de flujo. Verificar resultados observables, no sólo el catálogo C++.

Estas pruebas están pendientes. La presente nota no representa validación de
compilación, ejecución, seguridad ni cierre del caso.

## Investigación del ciclo de especialización

Las olas de `MethodBodyBuildCoordinator` descubren qué ordinales de métodos ya
materializados necesitan análisis; no constituyen un materializador tardío de
nuevas especializaciones. `ProjectGenericModel::build` termina con `built_`, y
`materializePendingDemandsForBuild` vacía las demandas antes de crear los
modelos de nombres y miembros. No se debe confundir cierre de alcanzabilidad de
cuerpos con cierre de especializaciones nuevas.

Dos alternativas requieren comparación antes de elegir implementación:

1. Demandas semánticas tardías seguidas de nuevas olas de materialización y
   actualización consistente de índices, nombres, miembros, herencia y
   constraints, hasta alcanzar un punto fijo. Debe demostrarse estabilidad de
   ordinales, inexistencia de referencias colgantes, límites, cancelación,
   paralelismo y ausencia de diagnósticos duplicados. No existe evidencia aquí
   de que la API actual permita reabrir esos modelos de manera segura.
2. Una recolección temprana de candidatos de campo y demandas en la fase
   genérica, seguida de comprobación semántica exacta después de nombres,
   miembros y herencia. `GenericCallInference.cpp:1322–1440` ya identifica
   declaraciones y plantillas, recorre campos y bases, recupera contexto de
   imports y aliases y sustituye argumentos. Es una base de investigación,
   no una prueba de cobertura total ni de control de acceso. Debe resolverse
   cómo impedir que una demanda especulativa de campo inaccesible, incompleto o
   incorrecto genere diagnósticos falsos o código observable. También debe
   demostrarse que la comparación semántica detecta diferencias de identidad,
   partials, tipos binarios y miembros sintéticos sin dejar demandas perdidas.

Un posible punto de integración temprano es `processNodeOnce`, antes de
`scheduleProcessedNodeChildren` en `GenericNodeProcessor.cpp`. Las copias de
especializaciones vuelven a procesarse en `SpecializationExecutor.cpp`; si se
generaran allí los cuerpos por campo, el recorrido existente podría recoger sus
demandas. Esto todavía necesita un prototipo y los controles mencionados. El
planificador visita tanto `first`, `second` y `third` como `children`: conservar
en esos punteros el cuerpo abierto y a la vez agregar copias exigiría impedir
que se analice el original como si fuera otra iteración. También deben tratarse
la iteración vacía, el anidamiento y la serialización de la plantilla abierta.

La selección temprana puede ser especulativa frente a la semántica completa.
Una comprobación posterior de identidad no basta si las copias provisionales ya
provocaron errores, especializaciones o publicaciones de caché. Un motor
`DiagnosticEngine` local permite diferir y fusionar diagnósticos, pero no revierte
mutaciones al AST, planner, índices ni `specializations_`. La alternativa debe
aislar todo ese estado o demostrar que su selección temprana es exacta antes de
materializarla. Hasta entonces no se considera resuelto el ciclo de fases.

### Encadenamiento propuesto y criterios para un prototipo

1. Parser: reconocer una construcción contextual independiente de `foreach`
   ordinario en ambas máquinas de sentencias, conservar tipo consultado,
   identificador de descriptor y cuerpo abierto en un nodo propio. Agregar el
   nombre del nodo al volcado y preservar la identidad de nodos serializados.
   Los errores de cabecera y cierre se producen en la ubicación original.
2. Expansión: al procesar un cuerpo concreto en la fase genérica, obtener una
   secuencia provisional asociada a identidades completas de tipo y campo.
   Clonar un bloque independiente por elemento y sustituir nombre constante,
   proyección de tipo y acceso; las copias entran al worklist de genéricos.
   El árbol abierto no se procesa junto con las copias. No publicar resultados
   de una selección que no pueda demostrarse completa y única.
3. Verificación: antes de usar cada copia, cotejar el registro temprano con la
   declaración semántica exacta, incluyendo identidad de tipo, ordinal de
   miembro, accesibilidad y alcance. Una discordancia debe invalidar toda la
   expansión sin producir una lista parcial ni permitir una selección por
   nombre equivalente. Las consultas concurrentes no comparten buffer mutable.
4. Acceso: convertir el `Get` de esa copia en una expresión ordinaria con una
   selección interna verificable. Tras comprobar receptor y accesibilidad,
   llamar a `analyzeFieldMember` con el ordinal único y continuar por HIR/MIR.
   Ni metadata ni texto auxiliar escrito por sí solos conceden autoridad.
5. Diagnósticos y estado: resolver antes cómo aislar o evitar los diagnósticos,
   especializaciones, entradas de planner y mutaciones surgidos de candidatos
   descartados. Si no puede demostrarse una selección temprana completa, esta
   vía no es suficiente y no se debe presentar como implementación segura.

Prueba de discriminación imprescindible: dos campos, `int` y `long`, alimentan
dos llamadas especializadas diferentes; los valores y nombres observados deben
ser correctos. Otra prueba usa una derivada que oculta un campo de la base y
exige que cada descriptor acceda al miembro de su propia identidad, no al
primero que resuelva el nombre. Se agregan variantes de visibilidad y binario;
ninguna prueba se ha ejecutado todavía.

Experimentos concretos, aún pendientes de un compilador construido de esta
copia: (a) reproducir la materialización existente de dos llamadas explícitas
`Format<int>` y `Format<long>` y registrar las especializaciones observadas;
(b) inspeccionar cómo se comporta una llamada genérica que infiere el tipo de
un campo de clase genérica y de una base genérica; (c) repetir con clase de
una biblioteca binaria y con field oculto homónimo; (d) verificar que un campo
inaccesible o metadata incompleta no genera éxito silencioso. Estos controles
no hacen existir la sintaxis propuesta y no sustituyen la prueba de la
construcción completa una vez implementada.
