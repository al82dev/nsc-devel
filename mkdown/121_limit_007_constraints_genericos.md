# 121. NSC-LIMIT-007 -- constraints genéricos `where T : class, IComponente`

Entrega del 2026-09-04. Cierra el segundo bloque del plan: parsear, guardar, validar, y que el
cuerpo genérico resuelva miembros por las interfaces declaradas.

---

## 1. Lo primero, porque cambia cómo se lee todo lo demás

**Un constraint en este compilador no habilita nada.**

NativeSharp monomorfiza. Cuando el cuerpo de `Caja<T>` se comprueba, `T` ya es un tipo concreto y
sus miembros se resuelven solos: `pieza.Peso()` compilaría igual sin la cláusula. Es la misma
situación de los templates de C++, que funcionan sin `concepts`.

Lo que cambia es **dónde falla el error**. Sin constraint, un argumento inválido revienta *dentro*
del cuerpo sustituido, apuntando a las tripas de la biblioteca. Con constraint, revienta en el
sitio de instanciación, nombrando el argumento y lo que le falta.

Eso, y que la fuente C# con `where` **deja de ser un muro**: antes ni siquiera parseaba.

---

## 2. Lo que fallaba

Cinco sitios del parser rechazaban `where` con «generic constraints are deferred beyond Stage 44».
No era una limitación del modelo: no había nada escrito.

Y **cuatro de los cinco estaban en la posición gramatical equivocada**, delante de lo que en el
lenguaje va antes de la cláusula:

| declaración | dónde estaba el rechazo | dónde va la cláusula |
|---|---|---|
| clase | antes de `: Base, IFoo` | **después** de la lista de bases |
| delegado | antes de `(...)` | **después** de la lista de parámetros |
| método | antes de `(...)` | **después** de la lista de parámetros |
| función | antes de `(...)` | **después** de la lista de parámetros |
| struct | antes de `{` | correcto: un struct no declara bases aquí |

Con el rechazo delante de la lista de bases, `class C<T> : Base where T : class` no llegaba
siquiera a leerse como una cláusula: el parser terminaba la lista de bases y encontraba `where`
sin nadie que lo esperara.

**No hay llamada equivalente en `parseInterfaceDeclaration`, y no es un olvido:** una interfaz de
este lenguaje no declara parámetros genéricos —no llama a `parseGenericParameterList`— así que una
cláusula ahí no podría nombrar nada. Se comprobó midiendo: `interface I<T>` da «expected '{', found
'<'». Poner la llamada de todas formas habría sido código muerto aparentando cobertura.

---

## 3. El AST

`ast::GenericConstraintClause`: nombre del parámetro y su rango, `requiresClass`, los tipos escritos
como `TypeSyntax*`, y el rango de la cláusula entera.

**Se guarda sin resolver, y esa es la decisión.** Cuando el parser corre no existe el name lookup
—se construye tres fases más tarde— así que resolver ahí exigiría una copia privada de la
resolución de nombres. Quien valida es una fase semántica posterior.

`class` viaja como bandera y no dentro de `typeConstraints` porque es el único constraint que no es
un tipo. Cuál de los tipos escritos es la base y cuál la interfaz **no se decide en el parser**: eso
necesita saber qué declara cada nombre.

---

## 4. Lo que el parser sí comprueba, y por qué ahí

Todo lo que se puede saber **sin resolver tipos**:

- el parámetro existe en la declaración;
- no hay dos cláusulas para el mismo parámetro;
- no se repite un constraint escrito;
- `class` no viene en segunda posición.

Son errores de la **declaración**. Dejarlos para la validación semántica los convertiría en errores
del sitio de instanciación —lejos de la causa— y sonarían una vez por instanciación en vez de una
vez por declaración.

`struct`, `new()`, `unmanaged` y `notnull` se **rechazan con diagnóstico propio**, no con un parseo
parcial que los ignore. Un constraint aceptado y no verificado es peor que uno rechazado: promete
algo que nadie comprueba, y el error acaba saliendo dentro del cuerpo sustituido — que es
exactamente el muro que este ID existe para quitar.

---

## 5. El modelo, y el problema de resolución que costó rehacer el diseño

`GenericTemplateRecord` gana `constraints`, copiadas del AST al recolectar plantillas.
`GenericSpecializationRecord` gana `templateOrdinal`, `instantiationSyntax` e
`instantiationRange` — el **sitio** importa: sin él, un `Caja<int>` inválido se diagnosticaría
sobre la declaración de `Caja<T>`, que es correcta.

### 5.1 El problema

La validación necesita resolver dos cosas a tipos: **el constraint** (`IComponente`) y **el
argumento** (el `Valor` de `Caja<Valor>`). `canonicalTypeKey` resuelve mirando
`nameLookup.resolvedDeclaration(syntax)` — o sea, exige que el lookup haya **visitado** ese nodo.

Y no visitaba ninguno de los dos:

- los tipos de constraint viven en `AstNode::genericConstraints_`, que nada recorría;
- el argumento se **borra del árbol** al materializar (`clearTypeArguments()`), en la fase 2,
  antes de que exista el lookup.

Medido: la primera versión devolvía `unresolved:Valor` y `unresolved:IComponente` para todo.

### 5.2 La salida que NO se tomó

Resolver por nombre dentro de la fase de validación. Habría funcionado para los casos fáciles y
habría sido **una segunda resolución de nombres** — con su propio alcance, sus propios `using`, sus
propios alias, y su propia forma de discrepar con la de verdad. Es el defecto que este proyecto
lleva media docena de documentos persiguiendo.

### 5.3 La que sí

**Colgar el argumento de la cláusula clonada.** `GenericConstraintClause` gana
`substitutedArgument`, que `cloneNode` rellena desde el mapa de sustitución. La cláusula clonada
vive en la declaración especializada, que **sí está en el árbol**, así que:

- `ProjectNameLookup::bindNode` recorre `genericConstraints()` y ata los tipos de constraint **y**
  el argumento por el camino ordinario — mismo alcance, mismos `using`, mismos alias;
- la validación lee las cláusulas **del nodo especializado**, no las de la plantilla, y los dos
  lados ya están resueltos.

Cero resoluciones nuevas. El argumento vuelve al árbol por la única puerta que había.

---

## 6. La fase de validación

`ProjectGenericConstraintModel`, invocada en `ProjectCompilation::compileProjectMethodBodies`,
**después** de construir interfaces y **antes** del modelo de cuerpos.

Es el único punto posible de toda la cadena:

```
syntax -> generics -> names -> members -> inheritance -> interfaces -> methodBodies -> ...
             ^                                                ^
             |                                                |
      materializa aquí                                 se valida aquí
   (sin herencia ni interfaces                  (con las dos ya construidas, y antes
    todavía: no puede validar)                   de que ningún cuerpo se analice)
```

Antes no hay con qué responder «este tipo implementa esa interfaz». Después, los cuerpos ya se
habrían analizado sobre una especialización inválida.

**Se sigue adelante aunque haya violaciones.** Ya se reportaron como errores; parar aquí dejaría
sin analizar el resto del programa, y un solo `Caja<int>` mal escrito ocultaría todos los demás
diagnósticos del fichero.

### 6.1 Las reglas

- **`class`**: el argumento tiene que ser clase o interfaz. El calificador de propiedad del uso no
  entra y no se toca: `where T : class` habla de **qué es** `T`, no de quién lo posee.
- **Interfaz**: una clase la implementa (`classImplementsInterface`) o una interfaz la extiende
  (`interfaceDerivesFrom` con identidad). Los dos caminos hacen falta.
- **Clase base**: `isDerivedFrom` **con identidad** — `where T : Base` lo satisface `Base` misma.
- **Un constraint que no es clase ni interfaz** es un error de la **declaración**, y su diagnóstico
  va sobre la cláusula: quien escribió `where T : MiEnum` se equivocó allí, y el sitio de
  instanciación no tiene nada que corregir.

### 6.2 La entrada de driver

`--check-project-generic-constraints`, con su texto de ayuda. No es decoración: hay 14 flags
`--check-project-*`, una por fase, y los tests de ctest que invocan al compilador cuelgan de ellas.
Una fase sin flag sólo se ejercita por unit test.

---

## 7. Medido

### 7.1 Positivo

Fixture `tests/cc1/c44_constraints_genericos`, código 42. Las cuatro posiciones que se admiten,
`class` + interfaz, clase base con identidad, interfaz satisfecha por otra que la extiende, y el
cuerpo genérico llamando a un miembro de la interfaz del constraint (códigos 11 y 15).

Ese último caso es el que responde al cuarto punto del encargo —«que el cuerpo resuelva miembros
mediante las interfaces declaradas»— y la respuesta medida es: **ya lo hacía**. Con monomorfización
`T` es concreto cuando el cuerpo se comprueba. La cláusula no añade capacidad; la fixture lo fija
para que nadie tenga que volver a averiguarlo.

### 7.2 Negativos

Diez rechazos, cada uno comprobado contra su mensaje y no sólo contra el hecho de fallar.

Los del **argumento**, que suenan en el sitio de instanciación:

```
generic parameter 'T' of 'Caja' requires a reference type ('where T : class'),
    but the argument 'Valor' is not a class or interface
generic parameter 'T' of 'Caja' requires an implementation of 'IComponente',
    but the argument 'Suelta' does not implement it
generic parameter 'T' of 'Derivada' requires a type derived from 'Base',
    but the argument 'Suelta' does not derive from it
```

`Caja<Suelta>` reporta **sólo** la violación de interfaz: `Suelta` es una clase, así que `class` se
cumple. La regla discrimina, no dispara en bloque.

Los de la **declaración**, que suenan donde hay que corregir:

```
'Color' cannot constrain 'T': a generic constraint names a base class or an interface
'U' is not a generic type parameter of this declaration
duplicate 'where' clause for generic type parameter 'T'
'class' must be the first constraint of its 'where' clause
duplicate constraint 'IComponente' for 'T'
the 'struct' generic constraint is not supported yet; this compiler implements
    'class', a base class and interfaces
```

### 7.3 En el contenedor

- las siete unidades tocadas compilan **sin avisos** con `-Wall -Wextra`; el driver también;
- barrido completo: **189 OK, 5 FAIL** en `--stage=llvm` y `--stage=bodies`. La línea base era 188;
  la que sube es c44. Los cinco fallos son los negativos intencionados de siempre.

**No verificado aquí:** el código de salida 42 en ejecución — el contenedor no enlaza ni ejecuta
PE — ni los unit tests de ctest, que necesitan el build completo.

---

## 8. Dos tropiezos que costaron tiempo y conviene no repetir

**ODR por rebuild parcial, dos veces.** Añadir un miembro a `AstNode` y a `ProjectCompilation`
cambia su tamaño; recompilar sólo los `.cpp` que edité dejó la mitad de los objetos viendo un
struct de un tamaño y la otra mitad de otro. Se manifestó como `malloc(): invalid size` en los 193
proyectos, y luego como «ProjectCompilation instances are single-use» en el primero. Ninguno de los
dos mensajes menciona layouts.

**Regla, desde ahora: tocar una cabecera obliga a reconstruir todo.** Es lo que el propio
`rebuild.sh` del arnés ya advierte en su comentario.

---

## Ficheros tocados

```
include/nativesharp/AST/AstNode.h                     GenericConstraintClause + accessors
src/AST/AstNode.cpp                                   accessors
src/AST/AstDumper.cpp                                 volcado de las clausulas
include/nativesharp/Parser/Parser.h                   parseGenericConstraintClauses
src/Parser/Parser.cpp                                 el helper, las 4 posiciones admitidas,
                                                      los 5 rechazos eliminados, y los 3
                                                      rechazos nuevos de la 2a vuelta
include/nativesharp/Frontend/ProjectGenericModel.h    constraints, templateOrdinal, sitio
src/Frontend/ProjectGenericModel.cpp                  recoleccion, clonado con sustitucion,
                                                      substitutedArgument, sitio
src/Frontend/ProjectNameLookup.cpp                    ata los tipos de las clausulas
include/nativesharp/Frontend/ProjectGenericConstraintModel.h   nueva
src/Frontend/ProjectGenericConstraintModel.cpp                 nueva
include/nativesharp/Frontend/ProjectCompilation.h     el modelo nuevo y su accessor
src/Frontend/ProjectCompilation.cpp                   la fase, entre interfaces y cuerpos
src/Driver/Driver.cpp                                 --check-project-generic-constraints
CMakeLists.txt                                        compila el fichero nuevo, y la prueba
                                                      ctest de la fase de constraints
tests/cc1/c44_constraints_genericos/                  nueva
tests/unit/Cc1CorrectnessTests.cpp                    c44 + 13 rechazos + 1 aceptacion
mkdown/121_limit_007_constraints_genericos.md         este documento
```


---

## 9. Segunda vuelta: siete defectos de la revisión, y qué se cambió por cada uno

La primera entrega prometía más de lo que comprobaba. La revisión los aisló uno a uno; esto es lo
que cambió, con la razón, no sólo el parche.

### 9.1 Una función global genérica aceptaba `where` y nadie lo comprobaba

`isGenericMethodTemplateNode` reconoce `MethodDeclaration` y **nada más**, así que una `F<T>()` de
namespace nunca entra en el modelo genérico y nunca produce una especialización. La cláusula
quedaba escrita y sin comprobar.

Había dos salidas. Meter `FunctionDeclaration` en el modelo genérico es una función entera —una
carencia previa a este ID, no algo que él introdujo— y hacerla a medias dentro de esta entrega
habría cambiado la materialización de todo el compilador sin prueba que la sostenga. La otra es
**rechazar la cláusula ahí**, que es lo que se hizo, con un mensaje que dice qué hacer:
declararla como método estático de una clase.

Un constraint que no se comprueba es **peor** que no tener constraints: el autor lo lee como una
garantía. El documento decía «las cinco posiciones»; son cuatro admitidas y una rechazada, y la
fixture y este documento ya lo dicen así.

### 9.2 Una sobrecarga descartada producía una violación falsa

Cuando dos plantillas comparten nombre y aridad, la fase genérica **las materializa todas**,
porque elegir entre ellas necesita los tipos de los argumentos y esa fase no los tiene: a partir de
ahí son sobrecargas ordinarias que resuelve el modelo semántico. El validador, en cambio, recorría
todas las especializaciones por igual, de modo que la candidata que la llamada **descarta** podía
rechazar el programa.

`GenericSpecializationRecord` gana `overloadGroup`: cero si la especialización es la única
respuesta a su sitio, y distinto de cero si salió de uno de esos conjuntos. El validador acumula
los diagnósticos del grupo sin emitirlos y **sólo los emite si fallan todas** las candidatas. Una
candidata sin cláusulas salva al grupo, porque no hay nada que exigirle.

### 9.3 Los constraints mal escritos de una plantilla sin usar nunca se veían

`where T : MiEnum` está mal **escrito**: no depende de con qué se cierre `T` y el sitio a corregir
es la declaración. Comprobarlo sobre la especialización lo ataba a que alguien instanciara: una
biblioteca con la cláusula mal escrita y sin un solo uso salía verde.

Ahora hay dos pasadas. La primera recorre `genericModel.templates()` y denuncia lo que está mal
escrito —constraint que no nombra clase ni interfaz, y el mismo tipo dos veces— **una sola vez y
sobre la plantilla**. La segunda es la de siempre, sobre especializaciones, y **se salta** los
constraints que la primera ya denunció, para no repetir el mismo error de declaración por cada
`Caja<X>` del programa.

Para que la primera pasada pueda resolver algo, `bindNode` dejó de devolver en seco ante un nodo
genérico: ahora ata **los tipos de constraint** de la plantilla antes de volver. El cuerpo sigue
sin visitarse —habla de `T`, que no es un tipo—, pero `IComponente` en un `where` existe con
independencia de que `T` se cierre.

Eso abrió un caso que antes no podía llegar: `where T : U`, con `U` otro parámetro de la misma
declaración, habría acabado en «unknown type name 'U'». Se rechaza en el parser, con su mensaje.

### 9.4 `where T : class` no significaba «tipo referencia»

Aceptaba clase e interfaz y nada más, que es **más estrecho que la palabra que usa**. Un delegado
es una referencia —tiene su propia `NamedTypeDeclarationKind`—, `string` lo es, y un arreglo lo es.

`isReferenceTypeArgument` mira ahora el modificador **más externo** del tipo escrito: arreglo o
arreglo dinámico son referencia; puntero no, y no por descuido — `Pieza*` no participa del modelo
de objetos (no tiene descriptor, no admite `is`, no lo recorre la destrucción) y aceptarlo bajo
`class` prometería justamente eso. Sin modificadores: `string`, o una declaración de clase,
interfaz o delegado.

La fixture mide los tres positivos (códigos 18, 19, 20) y la suite de rechazos mide el puntero,
que es el control: sin él, «se acepta casi todo» sería indistinguible de «se acepta todo».

### 9.5 La entrada del driver no era una fase

`--check-project-generic-constraints` se sumaba a `methodBodyAction` y acababa ejecutando
`compileProjectMethodBodies()`, con la validación incrustada dentro. Pedir «comprueba los
constraints» analizaba además **todos los cuerpos** del proyecto y podía devolver rojo por un
error que no tenía nada que ver con un `where`.

Ahora `compileProjectGenericConstraints` es una fase propia entre las interfaces y los cuerpos, y
el driver tiene su propio escalón `genericConstraintAction`, por debajo de `methodBodyAction` —toda
fase posterior lo arrastra— y por encima de `interfaceAction`, que es lo último que necesita.

### 9.6 Los constraints duplicados por alias no se detectaban

El parser compara `baseName()`, que es lo único que puede hacer sin resolver nombres, y dos
escrituras distintas del mismo tipo se le escapan. La primera pasada del validador compara ahora
**claves canónicas** dentro de cada cláusula y denuncia la repetición nombrando las dos escrituras.

### 9.7 Cobertura

La fixture gana el delegado (17), los tres tipos referencia que no son clase ni interfaz (18-20) y
el conjunto de sobrecargas (21). La suite de negativos gana el `where` en función global, el
constraint que es un parámetro genérico, el duplicado por alias y el puntero bajo `class`, más una
aceptación explícita para la sobrecarga descartada. Y `--check-project-generic-constraints` tiene
por fin su entrada de ctest, `nsc.cc1.generic-constraints`, sobre la propia c44.

### 9.8 Lo que NO está medido en esta vuelta

Los cambios de esta sección **compilan sin avisos con `-Wall -Wextra`, y nada más**. No hay barrido
del contenedor ni corrida de `nscbuilder.py` posteriores: se pidió expresamente no ejecutar nada en
esta pasada. Los cuatro casos nuevos de la fixture —delegado genérico con `where`, `class`
satisfecho por delegado, por `string` y por arreglo, y el conjunto de sobrecargas— **están
escritos, no comprobados**, y son los primeros que hay que mirar en la próxima corrida.


---

## 10. Tercera vuelta: lo que la segunda dejó abierto

### 10.1 Una sobrecarga no aplicable salvaba al grupo

`collectMethodTemplates` agrupa por nombre y **aridad genérica**, sin mirar cuántos argumentos de
valor toma cada candidata. La regla de grupo de la segunda vuelta —«sólo falla si fallan todas»—
daba entonces por buena esta llamada:

```
F<T>(T x) where T : class
F<T>(T x, int extra)          // sin constraint

F<int>(1);
```

La primera es la única aplicable y viola `class`; la segunda no acepta un argumento, pero salvaba
al grupo y tapaba el error.

`GenericSpecializationRecord` gana `overloadApplicable`, calculado en el sitio comparando la cuenta
de parámetros de valor con la de la llamada. Una candidata no aplicable **no dice nada en ninguna
dirección**: ni falla el programa —la llamada nunca resolvería a ella— ni lo salva. Se ignora
entera, y el grupo falla si ninguna de las **aplicables** cumple. Un grupo sin candidatas aplicables
queda vacío y calla: ese error lo reporta la resolución de llamadas, que es su sitio.

La respuesta se guarda con la especialización, que se cachea por clave; si la misma especialización
se alcanzara después desde una llamada con otra cuenta, conserva la primera. El sesgo es hacia el
silencio, nunca hacia un error falso.

### 10.2 Faltaba entera la regla de constraint primario

El parser admite cualquier combinación —no sabe cuál de los nombres es una clase— y la validación
sólo miraba que cada uno fuera clase o interfaz. No se rechazaba ninguna de estas tres:

```
where T : class, Base
where T : Base1, Base2
where T : IComponente, Base
```

Una cláusula dice **dos cosas de distinta naturaleza**: qué *es* el parámetro —a lo sumo una:
`class`, o una clase base concreta— y qué *implementa*, que pueden ser varias. La primera es única
porque no hay herencia múltiple: dos clases base no describen ningún tipo posible, y `class` más
una clase base es redundante en el mejor caso y contradictorio en cuanto la base sea un struct.
Aceptarlas dejaba escribir cláusulas que no restringen nada y que el lector interpreta como si
restringieran. Y va primera por la misma razón que `class`: la lista se lee de izquierda a derecha
como «esto es un X que implementa Y y Z», y con la base en medio deja de leerse.

Las tres se comprueban sobre la **plantilla**, en `validateTemplateClauses`, porque son errores de
declaración: no dependen de con qué se cierre `T` ni de que alguien instancie.

### 10.3 Sigue pendiente, y se dice sin adornos

Los constraints en **funciones globales genéricas** no están implementados: se rechazan. Meter
`FunctionDeclaration` en el modelo genérico es una carencia anterior a este ID y una entrega por sí
misma. El rechazo es seguro —no queda ningún `where` sin comprobar— pero no es la funcionalidad.


---

## 11. Cuarta vuelta: la aplicabilidad no se cuenta, se resuelve

La tercera vuelta decidió qué candidata de un grupo de sobrecargas podía salvar al grupo **contando
parámetros**. Eso separa `F<T>(T x)` de `F<T>(T x, int)` y **no separa** `F<T>(T x)` de
`F<T>(T[] x)`, que es el caso normal:

```
F<T>(T x) where T : class
F<T>(T[] x)

F<int>(1);      // la única aplicable viola `class`; la otra la salvaba
```

El error de fondo era de sitio, no de fórmula: **ninguna cuenta de parámetros equivale a la
resolución de sobrecargas**, y contar mejor sólo habría movido el fallo a un caso más raro.

### 11.1 Dónde vive ahora la decisión

El modelo de constraints **deja de decidir**. Para una especialización que forma grupo, guarda sus
violaciones en `deferredBySpecialization_` y no emite nada; para una que no forma grupo —la única
respuesta posible a su sitio— sigue emitiendo al construir, porque no hay nada que esperar.

`ProjectMethodBodyModel` recibe el modelo y lo consulta **dentro** de su bucle de candidatas,
después de tipos y de propiedad y sólo si la candidata sigue viable:

- una candidata que no acepta estos argumentos no incumple nada —nadie va a llamarla— y acusarla
  llenaría la salida de errores sobre sobrecargas que el programa descarta;
- una candidata viable tiene su `where` como parte del contrato, igual que los tipos de sus
  parámetros.

Si al final no queda ninguna viable y alguna cayó **sólo** por su cláusula, se emiten sus
diagnósticos guardados: el mensaje del constraint en vez de un «ninguna sobrecarga acepta estos
argumentos» que no explica nada.

`overloadApplicable` se eliminó. Era el sucedáneo, y dejarlo junto a la solución real habría dejado
dos mecanismos a medias respondiendo la misma pregunta.

### 11.2 Medido

```
F<T>(T x) where T : class / F<T>(T[] x),  F<int>(1)        -> rechazado
                                          F<int>(values)   -> aceptado, elige T[]
F<T>(T x) where T : class / F<T>(T x,int), F<int>(1)       -> rechazado (caso de la 3a vuelta)
Elegir<T>(T) where T:IComponente / Elegir<T>(T,T) where T:IPesado
                                          Elegir<Pieza>(p) -> aceptado
```


---

## 12. Quinta vuelta: qué plantillas alcanza una llamada

Dos P1 con la misma causa de fondo. El conjunto de plantillas que una llamada podía estar nombrando
se formaba con el nombre del método, su aridad genérica y el nombre **corto** del contenedor. Nada
más.

### 12.1 Faltaban dos filtros

**Cantidad de argumentos de valor.** Una sobrecarga de dos parámetros se materializaba para una
llamada de uno, y el clon sobrante se analizaba: su cuerpo, sustituido con un `T` que a él no le
corresponde, rechazaba el programa entero. Medido, y **es independiente de los constraints** —el
mismo par sin `where` falla igual—, así que eran dos huecos, no uno.

**Identidad fuente del receptor.** `N1.C` y `N2.C` entraban en el mismo conjunto.

### 12.2 Y sobraba una interpretación

`receiverOwnerChain` convertía el receptor escrito a la forma canónica: `N1.C` pasaba a `N1+C`,
afirmando que `N1` era un tipo contenedor. **Esta fase corre antes de la resolución de nombres**: no
puede saber cuál de los dos segmentos es un namespace, así que no debe decidirlo.

El efecto medido era peor que un falso positivo. `N1+C` no coincide con **ningún** registro —el de
`N1` tiene dueño `C`—, de modo que el filtro descartaba las dos plantillas y sólo el fallback
rescataba la llamada. Con dos `F<T>` visibles de la misma aridad ese fallback no es ambiguo a veces:
lo es siempre, y **dos llamadas completamente calificadas no compilaban**.

La comparación es ahora textual contra la escritura fuente completa de cada plantilla, por **sufijo
de segmentos** —exacto, o precedido por un punto—, nunca a media palabra: un `ends_with` haría que
`XN1.C` nombrara a `N1.C`.

### 12.3 Y la especialización descartada tampoco se analiza

Filtrar por argumentos evita materializarla, pero no basta como única defensa: una candidata puede
ser aplicable por aridad y aun así incumplir su `where`. `ProjectMethodBodyModel` y
`ProjectOopHirModel` preguntan ahora los dos por `specializationSatisfiesConstraints` antes de
analizar o bajar un miembro. **Hacen falta los dos**: HIR vuelve a recorrer todos los miembros por su
cuenta, así que saltarla sólo en el modelo de cuerpos dejaría un miembro cuyo cuerpo HIR pide sin
que nadie lo haya analizado.

El registro **no se borra**: la fase de constraints lo necesita para poder explicar, si al final
ninguna candidata sirve, por qué ninguna servía.

### 12.4 Medido

```
N1.C.F<int>(1) y N2.C.F<int>(1), calificadas    -> compilan, dos cuerpos distintos
F<int>(1) con sobrecarga F<T>(T,int) where ...  -> compila
el mismo par SIN where                          -> compila
Par<int>(0,0) elige la de dos parametros        -> compila
los diez negativos de las rondas anteriores     -> siguen rechazando, mismos mensajes
barrido bodies y llvm                           -> OK=191 FAIL=5   (c46 es la nueva)
```

Fixture `tests/cc1/c46_sobrecargas_genericas`, código 42, con los cinco casos.

### 12.5 El sitio del diagnóstico, medido por offset

La prueba del rango de llamada compara el **offset** de la diagnosis contra el de la segunda
llamada en el propio texto, y exige además que NO apunte a la primera. Por texto no se distingue
nada: el mensaje sería idéntico en las dos.

## 13. Sexta vuelta: de «¿corregí el caso?» a «¿qué propiedad se cumple?»

Las cinco vueltas anteriores tienen un patrón que conviene nombrar, porque explica por qué cada una
dejó residuo para la siguiente: cada corrección atacaba **el caso reportado**. La revisión traía un
programa, el programa se ponía verde, y el defecto sobrevivía en el caso vecino que nadie había
escrito todavía. Los tres falsos verdes de §13.5 son el mismo error mirado desde el otro lado: una
prueba que mide el caso reportado y no la propiedad.

Esta vuelta se hizo al revés. Primero las propiedades, después el código:

1. Una llamada genérica **nunca** materializa una plantilla que su receptor no puede nombrar.
2. Una especialización que incumple su cláusula **nunca** analiza ni baja su cuerpo —método o tipo,
   agrupada o no—.
3. Una especialización cacheada reporta **siempre** el rango de la llamada actual.
4. Toda prueba nueva tiene que ponerse **roja** contra la implementación anterior, y recorrer
   exactamente la función que dice verificar.
5. El nombre C depende **sólo** de la identidad del tipo, nunca del orden de declaración. (Esta vive
   en el documento 122.)

### 13.1 Invariante 1 -- el receptor abreviado

`collectMethodTemplates` comparaba por sufijo de segmentos y aceptaba **cualquier** coincidencia. Con
`Caja.Corto<int>(0)` escrito dentro de `c46uno`, tanto `c46uno.Caja` como `c46dos.Caja` terminan en
`.Caja`: las dos entraban, se materializaban las dos, y el cuerpo de la ajena rechazaba el programa.

La comparación pasa a tener **escalones**, y sólo sobrevive el mejor:

```
0  la escritura es la del registro, exacta            Caja        ==  Caja
1  la escritura vacia, o relativa al namespace        Caja        ->  c46uno.Caja   desde c46uno
2  sufijo de segmentos                                Caja        ->  c46dos.Caja
3  no coincide
```

Dos pasadas: se busca el escalón mínimo, y después se recogen **sólo** las de ese escalón. Y encima
de eso, una regla de **dueño único**: si el mejor escalón abarca dos escrituras de dueño distintas,
la llamada es ambigua y se devuelve vacío, en vez de materializar plantillas de dos tipos homónimos.

`resolveMethodTemplate` **hace lo mismo**, y ese fue el tropiezo de esta vuelta: se aplicó el
escalón a `collectMethodTemplates` y se olvidó su gemela. `C.F<int>()` con un `C` en cada namespace
pasó a declararse ambigua, porque el camino sin sobrecargas seguía aceptando «coincide en cualquier
escalón». Es exactamente el fallo que el encuadre por invariantes existe para atrapar: **una
propiedad aplicada en una de las dos funciones que la necesitan**.

### 13.2 Invariante 2 -- invalidez y aplazamiento son dos preguntas

`specializationSatisfiesConstraints` consultaba `deferredBySpecialization_`, que sólo tiene las
especializaciones **agrupadas**. Una especialización sin grupo emite su error de inmediato y no
entra en ese mapa, de modo que la función respondía `true` sobre ella y su cuerpo se analizaba
igual, añadiendo un segundo error sobre código que el programa no usa.

Ahora hay un conjunto aparte, `invalidSpecializations_`, en el que entra **toda** especialización
con violaciones pendientes, antes de decidir si su diagnóstico se aplaza. Diferir el **diagnóstico**
y emitir el **código** son dos decisiones distintas.

Y se preguntan **dos nodos**, no uno. Para un método genérico la invalidez está en su propio nodo;
para un tipo genérico —`Caja<Valor>` con `where T : class`— está en el nodo del **tipo**, y sus
métodos son hijos suyos, así que preguntar sólo por `member.syntax` no la veía nunca.

```
Solo<int>(1) con where T : IComponente
  antes:  el constraint  +  «member access requires a class/interface reference ... 'builtin:int'»
  ahora:  el constraint

Caja<Valor> con where T : class, metodo Medir(T v) { v.MetodoInexistente(); }
  antes:  el constraint  +  «... has no compatible member named 'MetodoInexistente'»
  ahora:  el constraint
```

Los dos programas se pasaron por el binario anterior para obtener esa primera columna.

### 13.3 Invariante 3 -- el rango cacheado, medido por mutación

La prueba del rango existía y estaba verde, pero **no probaba lo que decía** (véase §13.5). La
versión nueva usa dos llamadas que cierran el **mismo** `<Valor>` y dos sobrecargas de **un**
argumento, de modo que forman grupo, se cachean, y la segunda llamada reutiliza la especialización
que creó la primera.

Que este programa pasa de verdad por ahí no se afirma, se midió: sustituyendo `callRange` por el
rango guardado en `reportSpecializationConstraintViolations`, el único diagnóstico salta del
desplazamiento **367** —la segunda llamada— al **311** —la primera—. La prueba anterior, con esa
misma sustitución, se habría quedado verde.

### 13.4 El hueco que nadie cubría: la cláusula aplazada

Buscando qué fixture recorría el aplazamiento de una cláusula agrupada, resultó que **ninguna**:

* `c35` tiene el par de sobrecargas de la misma cuenta y tipos distintos, pero **sin** `where`.
* `c44` tiene el par con `where`, pero de cuentas **distintas**, que el filtro separa antes.

Medido mutando el compilador para que la rama agrupada emitiera como la no agrupada: `c46` se puso
en rojo y `c35` y `c44` siguieron en verde. Ésa es la medida de que el hueco existía.

El caso nuevo es `c46` código 16, y está marcado en la fixture como lo que es: **no es una
regresión, es cobertura**. Ya funcionaba.

### 13.5 Los tres falsos verdes que se sustituyeron

Los tres eran míos, los tres estaban en verde, y ninguno de los tres tocaba la función que decía
verificar. Se dejan escritos porque el patrón es más útil que los casos:

| prueba | lo que decía medir | por qué no lo medía |
|---|---|---|
| el rango de la llamada | que una especialización cacheada reporta el sitio actual | usaba `F<Pieza>` y `F<Valor>` —especializaciones **distintas**, cada una con su propio rango— y sobrecargas de uno y dos parámetros, que el filtro de cuenta separa antes de formar grupo |
| la inyectividad del nombre C | que dos identidades distintas no colapsan | usaba `Ns.Inner` contra `Ns_Inner`, que se distinguen por el **nombre simple**: nunca llegaba al codificador |
| `c46` | qué plantillas alcanza una llamada | todos los receptores eran completamente calificados, y el defecto vivía en el receptor **abreviado** |

El denominador común: las tres se escribieron mirando el mecanismo que se acababa de tocar, en vez
de construir la entrada que **obliga** a atravesarlo. La comprobación es barata y hay que hacerla
siempre: **ponerla contra la implementación anterior y ver el rojo**. Cuando la propiedad ya
funcionaba —y por tanto no hay rojo posible—, la comprobación equivalente es **mutar** la línea que
la implementa y ver el rojo entonces.

### 13.6 Dos comentarios falsos

Ninguno de los dos cambia el comportamiento, y por eso mismo son peores: quien los lea se los cree.

* `tests/cc1/c35_sobrecarga_generica/Main.ns` decía que la identidad de una plantilla incluye la
  cuenta de parámetros de valor. **No la incluye.** Esa fue la primera versión del arreglo y está
  descartada, porque la clave sale del compilador por el volcado de HIR, el de MIR y los metadatos
  de biblioteca. El desempate real es el **índice de declaración**, puesto sólo donde dos plantillas
  comparten `nombre``aridad`.
* `tests/cc1/c44_constraints_genericos/Main.ns` decía que sus dos `Elegir` se materializan las dos.
  **No se materializan.** El volcado de genéricos de ese programa tiene **una** especialización de
  `Elegir`: la llamada trae un argumento y la que incumple toma dos, así que el filtro de cuenta la
  deja fuera y su cláusula ni se comprueba.

### 13.7 Medido

```
barrido bodies                                  -> OK=191  FAIL=5   (los 5 negativos de siempre)
barrido llvm                                    -> OK=191  FAIL=5
CompilationModelTests                           -> PASS 67 checks
ClassMemberSyntaxTests                          -> PASS 58 checks
ProjectNameLookupTests                          -> PASS 64 checks
ProjectMemberModelTests                         -> PASS 64 checks
c46 codigo 15 (receptor abreviado)              -> ROJO contra el binario anterior, verde ahora
c46 codigo 16 (clausula aplazada)               -> verde en los dos; ROJO bajo mutacion
cc1-limit007-invalida-sin-grupo                 -> ROJO contra el binario anterior
cc1-limit007-invalida-de-tipo                   -> ROJO contra el binario anterior
cc1-limit007-sitio-cacheado                     -> ROJO bajo mutacion del rango (367 -> 311)
IR de c46: definiciones de Aplazar              -> 1, la de (int, string)
```

## 14. Séptima vuelta: la escala de nombres, y la tercera forma de invalidez

Los dos defectos de esta vuelta son **míos, y de la vuelta anterior**. No son residuo antiguo: los
introdujo, o los dejó abiertos, el arreglo del §13.

### 14.1 El invariante 1 estaba aplicado con reglas inventadas

§13.1 escribió una escala propia de tres peldaños —exacto, relativo al namespace del que llama,
sufijo de segmentos— para decidir qué tipo nombra el receptor de una llamada genérica. El problema
no es que sea aproximada: es que **ya había una definición de «nombrar» en este compilador**, en
`ProjectNameLookup::resolveTypeCandidates`, y escribir una segunda es el mismo defecto que las seis
vueltas anteriores repitieron.

Lo que la escala real hace, y la inventada no:

```
1  tipos CONTENEDORES del sitio, de dentro afuera   Helper dentro de Outer -> Outer.Helper
2  alias de namespace                               using LD = a.b;  ->  LD.C es a.b.C
3  la cadena LEXICA de namespaces, de dentro afuera A.B.C, luego A.C
4  namespaces importados, TODOS en un solo escalon  dos candidatos ahi son ambiguos
5  el global del modulo propio
6  los modulos referenciados, por anidamiento
```

De los seis peldaños, la versión inventada tenía uno y medio. El caso que lo destapa:

```csharp
namespace A {
    class C { public static int F<T>(T x) { return 42; } }
    namespace B { class P { public static int Main() { return C.F<int>(0); } } }
}
namespace Z { class C { public static int F<T>(T x) { return x.Inexistente(); } } }
```

Para el modelo de nombres, `C` dentro de `A.B` es `A.C` sin dudarlo: el peldaño 3 acierta en `A` y
ya no se sigue bajando. Para la escala inventada, `A.C` y `Z.C` son los **dos** sufijos de `.C`,
empatan en el mismo peldaño, y la regla de dueño único —correcta en sí— declaraba ambigua una
llamada perfectamente legal. Medido: `generic call 'F<...>' does not name a unique visible Stage 44
generic method template`.

Y el peldaño 1 faltaba entero: dentro de `Outer`, `Helper.F<int>(0)` podía irse a `A.Helper` en vez
de a `Outer.Helper`.

**Qué se hizo.** `qualifiedNameLadder`, en `Frontend/QualifiedNameLadder.{h,cpp}`: dado el sitio en
forma **textual** —namespace léxico, cadena de tipos contenedores, alias, imports— produce las
escrituras candidatas **en orden de prioridad**, sin tablas y sin decidir nada.

Lo que se comparte es **el orden, no la búsqueda**, y la distinción importa:

* `ProjectNameLookup` recorre la escala y sondea su tabla de tipos, con la regla de visibilidad de
  cada peldaño. Sus dos copias del orden —`resolveTypeCandidates` y
  `resolveQualifiedTypeCandidates`— **se borraron**; ahora hay un solo recorrido y un `switch` por
  peldaño que conserva exactamente el sondeo que cada uno tenía.
* `ProjectGenericModel` recorre **la misma escala** y sondea sus plantillas. `sourceSpellingRank`,
  `kNoMatch` y `endsWithSegment` desaparecieron.

La fase genérica corre **antes** del modelo de nombres y no puede consultar su tabla —ése era el
motivo original de tener reglas propias—. Por eso el sitio es puramente textual: la fase genérica
lo construye del AST que ya recorre.

Dos cosas más que hacían falta y no estaban:

* **Los `using` del ámbito.** La fase genérica no los miraba, de modo que un receptor traído por
  `using` o por alias no encontraba dueño y la llamada caía al camino **sin dueño**, que materializa
  cualquier plantilla del mismo nombre. Se recogen del mismo AST y en el mismo sitio que
  `bindScope`. Con una diferencia declarada: allí el namespace importado se resuelve contra los que
  lo encierran y aquí no hay con qué; se guardan las dos formas y el peldaño prueba ambas. Un
  import que no acierte no elige mal —no acierta y se baja al siguiente—.
* **El camino sin dueño deja de deshacer el filtro.** Antes, `resolveMethodTemplate` terminaba con
  `unico(false, ...)`: si el filtro por dueño no daba una única plantilla, se quitaba el filtro. Eso
  anula el peldaño que se acaba de aplicar. Ahora ese camino se toma **sólo cuando la escala no
  encontró ningún dueño**, que es lo único que esta fase tiene para sospechar que el receptor
  escrito era un **valor** y no un tipo —`objeto.F<int>()` produce la escritura `objeto`, que no
  nombra ningún tipo—.

Y `collectMethodTemplates` y `resolveMethodTemplate` ya no tienen reglas propias: las dos llaman a
`templatesNamedBy`. Es la misma pregunta con y sin sobrecargas, y ahora no pueden divergir porque
es la misma función. El §13.1 documenta el tropiezo de aplicar una propiedad en una de dos
funciones gemelas; esto lo hace imposible.

### 14.2 La invalidez no eran dos preguntas, eran tres

§13.2 separó «hay que diferir este diagnóstico» de «esta especialización es inválida». Faltaba una:
**la plantilla misma está mal declarada.**

```csharp
class Mala<T> where T : int
{
    public int Medir(T x) { return x.MetodoInexistente(); }
}
```

`int` no puede ser un constraint. La primera pasada lo diagnostica sobre la declaración, como debe.
Pero la segunda pasada **salta** esa cláusula —no es clase ni interfaz, no hay nada que comprobar
contra el argumento—, así que `pending` queda vacío, la especialización nunca entra en
`invalidSpecializations_`, y su cuerpo se analiza:

```
'int' cannot constrain 'T': a generic constraint names a base class or an interface
member access requires a class/interface reference ... receiver has type 'builtin:string'
```

El segundo es ruido derivado de un cuerpo que nunca debió mirarse.

**Qué se hizo.** `invalidTemplateOrdinals_`, llenado **desde `violations_`** y no desde cada sitio
que reporta: toda violación de declaración lleva ya `isDeclarationViolation` y el ordinal de la
plantilla. Recolectarlo de ahí hace que una regla de declaración que se añada mañana entre sola.
La segunda pasada marca inválidas sus especializaciones y sale **sin emitir nada más**: el error ya
salió sobre el `where`, que es donde hay que corregirlo.

### 14.3 Medido

```
P1  C.F<int>(0) desde A.B, con Z.C ajena       antes: «does not name a unique visible template»
                                                ahora: OK (bodies)
P2  where T : int + x.MetodoInexistente()       antes: 2 errores    ahora: 1

c46 17  namespace exterior      -> 1 especializacion: c46hijo.C.F
c46 18  tipo contenedor         -> 1 especializacion: c46anidado.Marco+Helper.F
c46 19  using de namespace      -> 1 especializacion: c46lejos.dentro.Caja.F
c46 20  alias de namespace      -> 1 especializacion: c46lejos.dentro.Caja.G
        c46ajeno.C, c46anidado.Helper y c46intruso.Caja NUNCA se clonan, y sus cuerpos no
        compilan con `T = int`, asi que un clon se delataria por los dos lados.
c46 entero contra el binario anterior          -> ROJO

barrido bodies                                 -> OK=191  FAIL=5
barrido llvm                                   -> OK=191  FAIL=5
CompilationModelTests                          -> PASS 67
ClassMemberSyntaxTests                         -> PASS 58
ProjectNameLookupTests                         -> PASS 71   (64 antes; +7 de la escala)
ProjectMemberModelTests                        -> PASS 64
ProjectMethodBodyModelTests                    -> PASS 55
avisos -Wall -Wextra en los cuatro ficheros    -> 0

la prueba de la escala, bajo mutacion          -> 6 de 71 en ROJO
  (sin escalon de contenedores y con la cadena lexica de FUERA adentro)
```

La reescritura de `ProjectNameLookup` sobre la escala compartida **no cambia ningún resultado**:
191 proyectos y `ProjectNameLookupTests` con sus 64 comprobaciones anteriores intactas. Eso es lo
que la convierte en una extracción y no en un cambio de semántica.

### 14.4 Lo que sigue sin medirse aquí

`Cc1CorrectnessTests` y `ProjectCompilationTests` compilan limpio pero **no enlazan** en este
contenedor: arrastran `Driver::run`, y con él `LldLinker`, `WindowsExecutableBuilder` y
`LlvmOptimizer`, que necesitan la API C de LLVM 23. Sus casos nuevos se comprobaron reproduciendo
sus asertos con `ProjectCompilation` directamente —que es lo que las propias pruebas usan— y con el
volcado de genéricos. El enlace y la ejecución de las fixtures los cierra `nscbuilder.py` en la
máquina.
