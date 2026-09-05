# 122. NSC-LIMIT-009 — tipos anidados en clases y structs

Entrega del 2026-09-05. Cierra el tercer bloque del plan: `class`, `struct`, `interface`, `enum` y
`delegate` dentro de una clase o un struct, con `Outer.Item` como nombre fuente, `Outer+Item` como
identidad canónica, y accesibilidad, metadata y mangling que las distinguen de verdad.

---

## 1. Lo que fallaba, que era poco y estaba muy repartido

`class Outer { struct Item { } }` daba **«expected an instance field or method declaration in class
body»**. El mensaje de una errata, para algo que no lo era.

La causa está en cuatro sitios que no se conocen entre sí:

- **`parseClassMember`** lee modificadores y después intenta parsear un tipo de miembro. `class`,
  `struct`, `interface`, `enum` y `delegate` no son comienzos de tipo ni identificadores, así que
  caían todos en la guarda final.
- **`collectTypeSeedsInScope`** —el escáner de tokens que siembra los nombres de tipo que el parser
  usa para desambiguar— registraba el tipo exterior y **saltaba su bloque entero**. Un tipo interno
  no existía para él.
- **`predeclareNode`** construía `TypeIdentity` con `containingTypes = {}` y **devolvía sin
  recorrer los hijos** del tipo. Aunque el parser lo hubiera colgado del árbol, ninguna fase
  volvía a mirarlo.
- **La resolución de nombres** buscaba por namespace, nunca por cadena de tipos contenedores.

Lo que **no** faltaba: `TypeIdentity` ya modelaba `containingTypes` y ya separaba los anidados con
`+` desde hace tiempo. Esa pieza no se tocó — se empezó a usar.

---

## 2. El parser

`parseClassMember` reconoce las cinco formas antes de su guarda final y delega en **los cinco
parsers que ya existen**. Ninguna gramática se reimplementa; lo que el contexto de miembro aporta
es la accesibilidad escrita y el rechazo de lo que no significa nada en un tipo.

Como `parseStructDeclaration` usa el mismo `parseClassMember`, los tipos anidados en structs
salieron con la misma entrada.

### 2.1 Lo que se rechaza, y por qué cada uno lleva su mensaje

`static`, `readonly`, `const`, `virtual`, `override`, `export extern C` y `at(N)` se rechazan **uno
a uno, por su nombre**. Quien escribe `static class Item` dentro de una clase viene de C#, donde
eso existe, y necesita saber que aquí no — no que su línea es ilegible.

`abstract` y `sealed` sí valen, pero sólo sobre una **clase** anidada; el parser lo sabe porque es
él quien ve cuál de las cinco formas se está abriendo.

`partial` no entra en el bucle de modificadores —es contextual, y allí sería indistinguible del
tipo de un campo llamado `partial`— así que se reconoce por lo que le sigue y se consume para poder
dar el mensaje correcto en vez de dejar que el error salga tres tokens más allá.

### 2.2 Un anidado genérico se rechaza, y eso es una decisión

Exigiría que la identidad canónica llevara la aridad de **cada** segmento de la cadena
(`Outer\`2+Item\`1`) y que la materialización genérica clonara un cuerpo que a su vez declara tipos.
Ninguna de las dos cosas está escrita, y una identidad con la aridad perdida no es un límite: es
una colisión silenciosa entre `Outer<T>.Item` y `Outer.Item`. Se rechaza.

Lo que **sí** funciona, y se midió: un tipo anidado **dentro de un contenedor genérico**. La
especialización ya codifica sus argumentos en el nombre, de modo que `Caja<int>.Item` y
`Caja<long>.Item` salen con identidades y símbolos distintos sin trabajo adicional.

---

## 3. Las semillas del parser

`collectTypeSeedsInScope` recibe ahora la cadena de tipos contenedores y **baja al cuerpo** en vez
de saltarlo. Dos cambios, no uno:

1. **Avanzar hasta el `{`.** La versión anterior sólo miraba si el token justo detrás del nombre
   era `{`, de modo que en `class C<T> : Base { ... }` no reconocía el cuerpo y seguía leyendo sus
   tokens en línea recta, como si fueran declaraciones del namespace. Entre el nombre y el cuerpo
   caben parámetros genéricos, lista de bases, el tipo subyacente de un enum y una cláusula
   `where`.
2. **Bajar a él.** Los cuerpos de método de dentro se saltan solos: los cierra la rama `LeftBrace`
   que ya existía.

Se siembran todas las escrituras posibles —`Item`, `Outer.Item`, `ns.Outer.Item`— porque aquí no
hay identidad: esto es la lista de nombres con la que el parser distingue `Foo x;` de una
expresión, y nada más.

---

## 4. Identidad y accesibilidad

`predeclareNode` transporta `std::vector<TypeNameSegment>` y, tras registrar un tipo, **baja a sus
hijos que son tipos** con la cadena extendida. La aridad de cada segmento se toma del nodo aunque
hoy sólo pueda valer cero: es lo que hará que la identidad salga completa el día que los anidados
genéricos se admitan.

**El valor por omisión depende del sitio**, y ésa es toda la regla: un tipo de namespace sin
modificador es `internal`; uno anidado sin modificador es `private`. Convertir el segundo en el
primero publicaría dentro del módulo cada tipo interno que alguien escribiera para uso propio.

`NamedTypeAccessibility` gana `Private` y `Protected`. El agregador de partes sólo entiende
internal/public —es lo único que un tipo de namespace puede ser— así que para un anidado se le deja
hacer su trabajo (duplicados, `partial`) y se ignora su veredicto de accesibilidad. Un anidado
nunca es partial: el parser lo rechaza, de modo que siempre hay exactamente una parte.

### 4.1 `protected`, y por qué no se decide donde se decide todo lo demás

`private` se resuelve al buscar el nombre: basta con mirar si el contenedor está entre los tipos
léxicos del sitio. `protected` pregunta otra cosa —**«el tipo que escribe esto deriva del
contenedor?»**— y esa respuesta la tiene el modelo de herencia, que se construye **después** de
resolver nombres.

Había dos mentiras posibles y una salida. Tratarlo como private rechaza el uso legítimo desde una
derivada; tratarlo como internal deja pasar el módulo entero. La salida es **partir la pregunta**:
la resolución de nombres acepta el nombre y guarda en el binding la cadena de tipos del sitio de
uso, y `ProjectInheritanceModel::checkProtectedNestedTypeUses` la responde en cuanto la herencia
existe.

Se recorren los **bindings** y no las declaraciones porque la pregunta es por uso: el mismo tipo
protegido puede ser legítimo en una línea e ilegítimo en la siguiente.

### 4.2 Lo que cruza el límite de proyecto son todos los eslabones o ninguno

`public class Outer { public struct Item }` exporta `Item`. `class Outer { public struct Item }` no
exporta nada, porque el contenedor no sale del módulo. `typeIsPubliclyVisible` recorre la cadena
entera; una comprobación de un solo nivel habría dejado pasar el segundo caso, que es el que un
`public` interior hace parecer correcto.

---

## 5. La búsqueda

Dos mapas, a propósito:

- `typesByModule_` — la clave **canónica**, con `+`. Es la identidad.
- `nestedSourceNamesByModule_` — el nombre **fuente**, con puntos. Es lo que el programador
  escribe.

Colapsarlos en uno haría indistinguible el anidado `ns.Outer.Item` del tipo `Item` del namespace
`ns.Outer`, que es precisamente la ambigüedad que hay que poder detectar. `findTypeInModule`
consulta primero el canónico y después el de nombres fuente, de modo que en el mismo escalón
**gana el del namespace** — que es lo que ya hacía antes de que existieran los anidados: ningún
programa ya escrito cambia de sentido.

El orden de búsqueda gana un escalón delante de todo: los **anidados de los contenedores léxicos,
de dentro hacia fuera**. Dentro de `Outer`, `Item` significa `Outer.Item`. Es el mismo sombreado
que un namespace interior hace sobre uno exterior, aplicado a la cadena de tipos.

Y se arregló de paso un hueco anterior a este ID: los `using` no se consultaban para un nombre
**con puntos**, así que `using XLib;` + `Publica.Visible` no resolvía. Con anidados ésa es la forma
que la gente escribe, porque el punto separa contenedor de anidado, no dos namespaces.

### 5.1 Lo que la búsqueda NO hace

El nombre **corto** sube por los contenedores léxicos, **no por la cadena de herencia**. Desde una
clase derivada hay que escribir `Base.Reservada`, no `Reservada`. Resolverlo a secas exigiría
recorrer las bases al resolver nombres, que es la fase anterior a la que las conoce. Es una
carencia real, está anotada en la fixture, y no impide que la accesibilidad se decida bien.

---

## 6. El mangling, que es donde estuvo el defecto de verdad

Las identidades **siempre** se distinguieron:

```
Modulo::N.Outer+Item     el tipo ANIDADO
Modulo::N.Outer.Item     el tipo del NAMESPACE N.Outer
```

Pero el nombre nativo no salía de la identidad: salía de un saneador que aplastaba a `_` todo lo
que no fuera alfanumérico. Y había **cuatro copias** de ese saneador —HIR, MIR, destrucción y el
lowerer—, las cuatro convirtiendo `+` y `.` en el mismo `_`. Los dos tipos producían el mismo
`__nsc_method_`, el mismo `__nsc_classdesc_`, el mismo `__nsc_destroy_` y la misma tabla de
dispatch. **Dos tipos, un cuerpo.**

Y no lo cazaba nada, porque las pruebas de tipos anidados llegaban hasta el modelo de nombres y el
de miembros, **donde las identidades ya se distinguen**. El defecto vivía una fase más abajo.

`symbolFragmentForIdentityKey` vive ahora en `TypeIdentity`, junto al `+` que codifica, y las tres
copias que sólo se llamaban sobre claves de identidad delegan en ella. Codifica `+` como `$`:

- un identificador de NativeSharp es `[A-Za-z_][A-Za-z0-9_]*`, así que **`$` no puede salir de
  código de usuario** y la codificación es inyectiva;
- `$` es un carácter ordinario en un símbolo COFF y en un identificador de LLVM, que es donde el
  nombre acaba;
- **ningún símbolo ya existente cambia**, porque `+` sólo aparece en una identidad anidada.

La cuarta copia, la del lowerer, se dejó como estaba: sólo se llama sobre nombres de campo, que no
pueden contener `+`.

El **metadata** salió gratis y correcto: la cadena de nombre de tipo en tiempo de ejecución es
`"WindowsSystemInfo+Detalle"`, producida por `TypeIdentity`, no concatenada a mano en ningún sitio.

---

## 7. Medido

### 7.1 Fixtures

`tests/cc1/c45_tipos_anidados`, código 42. Lleva **dos identidades potencialmente conflictivas**
—`c45.Caja+Pieza` y `c45.Caja.Pieza`, más el par de structs homónimos— hasta HIR, LLVM, enlace y
ejecución, y comprueba el valor que devuelve cada una. Si los símbolos vuelven a colapsar, uno de
los dos cuerpos gana y los códigos 10-13 lo dicen. Códigos 14 y 15: el anidado `protected` desde
una derivada y desde el propio contenedor.

La fixture prueba el **comportamiento**; la prueba de IR `cc1-limit009-simbolos-anidados` prueba la
**causa**: que el módulo contiene dos funciones con nombres distintos, dos descriptores de clase y
dos entradas de destrucción. Hacen falta las dos — un comportamiento correcto por casualidad
pasaría la fixture, y dos nombres distintos que no se ejecutan no prueban que el programa haga lo
correcto.

### 7.2 Pruebas unitarias

- `ClassMemberSyntaxTests` — las cinco formas en clase y en struct, la accesibilidad escrita en el
  AST, ocho modificadores rechazados y dos controles (`abstract` y `protected` **sí** valen).
- `ProjectNameLookupTests` — identidad con `+`, el mismo nombre anidado bajo dos contenedores, el
  `private` por omisión, el nombre corto dentro del contenedor, `public` cruzando proyecto, y los
  dos negativos de cruce (anidado internal, y anidado public de contenedor internal).
- `ProjectMemberModelTests` — el contenedor tiene exactamente sus dos miembros y el anidado los
  suyos: un tipo anidado no es un miembro ejecutable, y ahora se descarta **explícitamente** en
  `appendExpandedMemberNodes` en vez de por no coincidir con ninguna rama.
- `CompilationModelTests` ya cubría `TypeIdentity` con `containingTypes` y la distinción frente al
  tipo de namespace. No se tocó.

### 7.3 En el contenedor (clang 18 + libLLVM-18, frontend real)

```
barrido --stage=bodies : OK=190 FAIL=5     (era 189; la que sube es c45)
barrido --stage=llvm   : OK=190 FAIL=5
ClassMemberSyntaxTests  : 58 checks
ProjectNameLookupTests  : 61 checks
ProjectMemberModelTests : 64 checks
```

Los 5 fallos son los negativos intencionados de siempre.

Símbolos medidos en el IR de c45:

```
__nsc_classdesc_c45_tipos_anidados__c45_Caja$Pieza
__nsc_classdesc_c45_tipos_anidados__c45_Caja_Pieza
__nsc_destroy_c45_tipos_anidados__c45_Caja$Pieza
__nsc_destroy_c45_tipos_anidados__c45_Caja_Pieza
__nsc_method_c45_tipos_anidados__c45_Caja$Pieza__Peso
__nsc_method_c45_tipos_anidados__c45_Caja_Pieza__Peso
```

### 7.4 Lo que no se midió aquí

El **enlace y el PE** los corre `nscbuilder.py` en Windows con LLVM 23; este contenedor llega hasta
verificar el módulo LLVM. `$` en un símbolo es ordinario en COFF y en LLVM, pero eso es un
argumento, no una medición: la primera corrida de c45 en Windows es la que lo convierte en un
hecho.

---

## 8. Un defecto propio que conviene no repetir

Al convertir la validación de constraints en fase propia (ID 007), `compileProjectMethodBodies`
pasó a **encadenarse por el valor de retorno de esa fase**. Y esa fase devuelve `false` también
cuando el programa es válido en todo salvo en un `where` — que no es motivo para parar. El
resultado: cuando una fase *anterior* fallaba sin dejar `loadError` —un error de diagnóstico, no de
carga— se seguía adelante con los modelos sin construir, y el modelo de cuerpos desreferenciaba un
puntero nulo. **Segfault en `ProjectMemberModel::isBuilt`**, medido con la primera versión de c45.

La lección es concreta: una cadena de fases se encadena por **«la fase anterior se construyó»**, no
por «la fase anterior no encontró errores». Son dos preguntas distintas y este compilador necesita
las dos por separado.

---

## Ficheros tocados

```
include/nativesharp/Parser/Parser.h                   NestedTypeModifiers, atNestedTypeDeclaration,
                                                      parseNestedTypeDeclaration
src/Parser/Parser.cpp                                 despacho en parseClassMember, los rechazos
include/nativesharp/Compilation/TypeIdentity.h        symbolFragmentForIdentityKey
src/Compilation/TypeIdentity.cpp                      idem
src/Frontend/ProjectCompilation.cpp                   semillas con cadena de contenedores
include/nativesharp/Frontend/ProjectNameLookup.h      Private/Protected, isNested,
                                                      containingTypeOrdinal, indice de nombres
                                                      fuente, cadena en el binding
src/Frontend/ProjectNameLookup.cpp                    predeclare recursivo, escalon de
                                                      contenedores, visibilidad, diagnostico
include/nativesharp/Frontend/ProjectInheritanceModel.h  checkProtectedNestedTypeUses
src/Frontend/ProjectInheritanceModel.cpp                idem
src/Frontend/ProjectMemberModel.cpp                   un tipo anidado no es miembro, dicho
src/Frontend/ProjectOopHirModel.cpp                   mangleCallable y typeSuffix por identidad
src/Frontend/ProjectOopMirModel.cpp                   descriptor/vtable/ifmap por identidad
src/Frontend/ProjectDestructionModel.cpp              entrada de destruccion por identidad
tests/cc1/c45_tipos_anidados/                         nueva
tests/unit/Cc1CorrectnessTests.cpp                    c45, la prueba de IR, los rechazos
tests/unit/ClassMemberSyntaxTests.cpp                 las cinco formas y los modificadores
tests/unit/ProjectNameLookupTests.cpp                 identidad, accesibilidad, cruce de proyecto
tests/unit/ProjectMemberModelTests.cpp                un anidado no es miembro
mkdown/122_limit_009_tipos_anidados.md                este documento
```


---

## 9. Segunda vuelta: la codificación, y dos agujeros en `protected`

### 9.1 La codificación de símbolos no era inyectiva

La primera corrección codificaba `+` como `$` y dejaba el resto igual. Arreglaba exactamente el par
que la prueba medía —`N.Outer+Item` frente a `N.Outer.Item`— **y ninguno más**, porque `.` y `:`
seguían yendo a `_`, que sí puede venir de un identificador de usuario:

```
Mod::A.B    ->  Mod__A_B      clase `B` del namespace `A`
Mod::A_B    ->  Mod__A_B      clase llamada `A_B`
```

El argumento que escribí —«`$` no puede venir del usuario»— era cierto y **no bastaba**: la
inyectividad la rompía `_`, no `$`. Es el mismo error de razonamiento que el documento 119 ya
registra: verificar contra el caso que se está mirando en vez de contra la regla.

`symbolFragmentForIdentityKey` es ahora una codificación **reversible**: alfanumérico ASCII y `_`
se copian; cualquier otro byte se escribe `$` + dos dígitos hexadecimales. Sin excepciones ni
caracteres reservados que recordar — `.`, `:`, `+`, el acento grave de la aridad y lo que se añada
después quedan separados por construcción. Los rangos ASCII se comparan a mano y no con
`std::isalnum`, que depende del locale: el nombre de un símbolo no puede cambiar según la máquina
que compile.

**El cambio de formato no se disimula.** Todo símbolo derivado de una identidad lleva ahora
`$3A$3A` —el `::` del módulo— y ninguno del formato anterior podía llevarlo, así que un objeto
viejo mezclado con uno nuevo falla con un externo no resuelto. Ese es exactamente el fallo que se
quiere: no hay forma de que enlace en silencio contra el símbolo equivocado. Cuatro expectativas de
prueba con el nombre completo se actualizaron —`ProjectOopLoweringTests`, `Stage45LinkerPeEhTests`
y `Stage56C8TypeRootTests`—; las demás comprueban prefijos y no cambian.

`CompilationModelTests` mide ahora la propiedad, no el caso: los dos pares que colapsaron, que el
subrayado del usuario sobrevive, que el punto no lo es, y que la aridad queda separada del nombre
que la precede.

### 9.2 `protected` no cruzaba el límite de proyecto

`typeIsPubliclyVisible` exigía `public` en toda la cadena, así que un anidado protegido de una
biblioteca **no llegaba a resolverse** en el proyecto consumidor — y la comprobación de herencia,
que es la única que sabe decidirlo, no recibía nunca la oportunidad. La fixture no lo detectaba
porque `Base`, `Derivada` y `Reservada` viven en el mismo proyecto.

Ahora un eslabón **anidado** cruza si es `public` **o** `protected`; `private` e `internal` siguen
cortando, porque ninguna fase posterior puede convertirlos en alcanzables desde fuera del módulo. Un
tipo raíz que no sea `public` tampoco sale: sólo un anidado puede ser `protected`.

Las pruebas nuevas de `ProjectNameLookupTests` llegan hasta `compileProjectInheritance`, no se paran
en los nombres: la decisión depende del grafo de herencia, y pararse antes mediría otra cosa.

### 9.3 `protected` se evadía por un receptor estático

`Base.Reservada.Codigo()` nombra el tipo **sin escribir un `TypeSyntax`**: el receptor es una
expresión, y produce un `ExpressionTypeNameBinding`. La comprobación diferida sólo recorría los
`TypeNameBinding`, de modo que un tipo ajeno alcanzaba el anidado protegido con sólo llamar a un
miembro estático — precisamente porque la resolución de nombres deja pasar `protected` a la espera
de una comprobación que en ese camino no ocurría.

`ExpressionTypeNameBinding` guarda ahora la misma cadena léxica, `expressionTypeBindings()` la
expone, y `firstUnreachableProtectedLink` es la función común que recorre los dos tipos de binding.
Un binding de expresión puede tener varios candidatos: basta con que **uno** sea alcanzable para no
decir nada, porque elegir entre ellos es trabajo del resolvedor de miembros y adelantarse aquí
convertiría una ambigüedad en un error de protección que no es.

### 9.4 Medido

```
barrido --stage=bodies : OK=190 FAIL=5
barrido --stage=llvm   : OK=190 FAIL=5
CompilationModelTests 67 / ClassMemberSyntaxTests 58 / ProjectNameLookupTests 64 /
ProjectMemberModelTests 64

protected desde una derivada en OTRO proyecto      -> compila
protected desde un tipo ajeno en OTRO proyecto     -> rechazado
Base.Reservada.Codigo() desde un tipo ajeno        -> rechazado
Base.Reservada.Codigo() desde una derivada         -> compila
```

`ProjectOopLoweringTests` no enlaza en este contenedor —usa `LlvmTargetMachine` y `LlvmOptimizer`,
excluidos por la API C de LLVM 18—, así que sus dos expectativas actualizadas se comprobaron contra
una bajada real: `__nsc_method_Stage30Test$3A$3ADemo$2ECounter__Add__builtin$3Aint`.


---

## 10. Tercera vuelta: la cadena de contenedores llegaba a la mitad

El modelo de nombres distinguía `Ns.A+Inner` de `Ns.B+Inner`. **`ProjectGenericModel` no.** Llevaba
un solo nombre de dueño —el del contenedor más interno— y lo **reemplazaba** al bajar, de modo que
al entrar en `A { Inner { … } }` el dueño pasaba a ser «Inner» y `A` desaparecía.

Con eso, dos plantillas sin ninguna relación compartían clave canónica, se agrupaban como
sobrecargas la una de la otra y podían compartir especialización. `ownerName` es ahora la **cadena
entera**, unida con `+` igual que la identidad, y se apila en vez de reemplazar; la clave canónica,
la resolución y la agrupación de sobrecargas usan esa misma cadena.

Al medirlo apareció la otra mitad: `A.Inner.F<int>(1)` no resolvía. El sitio de llamada sólo leía
un receptor de **un** identificador —`A.F<int>(x)`— y con tipos anidados eso deja de ser un caso
raro. `receiverOwnerChain` aplana la cadena de identificadores del receptor a `A+Inner`; si el
receptor no es una cadena de identificadores devuelve vacío y se conserva el comportamiento
anterior, incluidos los reintentos sin filtro de dueño.

Medido en el IR, dos cuerpos con identidades distintas:

```
__nsc_method_r$3A$3Aa3$2EA$2BInner__F__g1__int__h…
__nsc_method_r$3A$3Aa3$2EB$2BInner__F__g1__int__h…
```

La mitad de los **tipos** genéricos anidados sigue sin poder ejercitarse: el parser rechaza un
anidado genérico. La cadena se escribe igualmente en su clave, para que el día que se admitan la
identidad salga completa y no ambigua.

## 11. Y una corrección que NO se aplicó, con la medición que lo decide

La revisión pedía que un owner recién creado dejara de satisfacer un parámetro **sin calificador**:

```
if (argumentIsFreshOwner || argumentTransfer || argumentOwnership == Borrowed)
```

**No se aplicó.** Medido: rompe cinco de los ejemplos del libro con `no viable overload of
'Insert'`, sobre la forma más ordinaria que hay en este lenguaje:

```
public void Insert(Student value)          // parámetro sin calificador
tree.Insert(new Student(50, "Ana", 90));
```

Y sería además arbitrario: `Student s = new Student(…); tree.Insert(s);` hace exactamente lo mismo
con el mismo objeto y siempre fue legal. Lo que un parámetro sin calificador no hace es **rastrear**
la vida del valor; eso vale igual para un objeto recién creado que para uno que ya existía, y no es
algo que la comprobación de argumentos pueda arreglar. Rechazar sólo la forma en línea sería una
regla sobre la sintaxis, no sobre la propiedad.

Lo que sí estaba mal era la **contradicción** que la revisión detectó correctamente: el comentario
de `ExpressionResult::ownershipFreshOwner` afirmaba que el parámetro sin calificador lo rechazaba.
Era falso. El comentario dice ahora lo que el código hace y por qué: `ownershipFreshOwner` **sólo
relaja** el requisito de un parámetro `owned` —que no podía recibir un `new` porque no hay de dónde
mover—, nunca endurece el de los demás.


---

## 12. El identificador del header C

`sanitizeIdentifier` aplastaba a `_` todo lo no alfanumérico, de modo que `Mod::A.B` y `Mod::A_B`
pedían el mismo nombre. **Yo afirmé que producía dos typedefs iguales y era falso**: el generador
detecta el choque y añade `_2`. El defecto real es peor de lo que parece — cuál de los dos se queda
con el nombre limpio y cuál con el `_2` depende del **orden en que se recorren los tipos**, así que
mover una declaración de sitio cambiaba el header, y un header generado es un contrato con el
código C que lo incluye.

Dos cambios:

- La forma calificada usa una codificación reversible **válida en C** —aquí no puede haber `$`—:
  alfanumérico tal cual, `_` como `__`, cualquier otro byte como `_HH`. `_` abre siempre un escape
  y no es dígito hexadecimal, así que `__` nunca se confunde con `_HH`.
- Los tipos se recorren en **orden canónico**, por clave de identidad, de modo que si el desempate
  llegara a activarse el resultado sería el mismo en cada build.

El nombre **corto** se conserva cuando es único en todo el programa —es lo que hace legible un
header generado, y `simpleCounts == 1` no depende del orden—, así que `NS_Counter` y `NS_Node`
siguen siendo lo que eran. Sólo cambia la forma calificada, que es la que estaba rota.

Medido con el generador real, no razonado:

```
Collisions::First.Pair   -> NS_Collisions_3A_3AFirst_2EPair
Collisions::Second.Pair  -> NS_Collisions_3A_3ASecond_2EPair
Ns.Inner                 -> NS_Inner        (corto, unico)
Ns_Inner                 -> NS_Ns_Inner     (corto, unico)
sufijos `_2` en el header: 0
```

`Stage48_1CHeaderGeneratorTests` no enlaza en este contenedor —usa `LldLinker` y
`PeExecutableVerifier`—, así que las expectativas nuevas se comprobaron llamando a
`CHeaderGenerator::writeHeader` directamente sobre un proyecto con los cuatro tipos. Las cadenas de
la prueba son las que el generador emite, no las que supuse.

---

## 13. La prueba de inyectividad no provocaba ninguna colisión

La sección 12 arregló el codificador y midió sus salidas con el generador real. Lo que **no** se
arregló fue la prueba, y la prueba era un falso verde.

`testQualifiedNamesAreInjective` usaba `Ns.Inner` contra `Ns_Inner`. Esos dos tipos se distinguen
por el **nombre simple** —`Inner` contra `Ns_Inner`—, así que el generador les da su nombre corto y
no llega nunca al codificador. La prueba habría seguido en verde con el saneador viejo puesto.

El par que sí provoca la colisión son dos identidades **distintas** cuyo nombre simple es el
**mismo**:

```
A.B+Pair    un Pair anidado dentro de la clase B del namespace A
A_B.Pair    un Pair del namespace A_B
```

Con el codificador actual:

```
NS_Injective_3A_3AA_2EB_2BPair     el anidado:  '+' es _2B, '.' es _2E
NS_Injective_3A_3AA__B_2EPair      el plano:    el '_' que escribio el usuario es __
sufijos `_2`: 0
```

Y con el saneador viejo —todo lo no alfanumérico a un solo `_`—, medido sustituyéndolo y volviendo a
generar el header:

```
typedef struct NS_Injective__A_B_Pair   NS_Injective__A_B_Pair;
typedef struct NS_Injective__A_B_Pair_2 NS_Injective__A_B_Pair_2;
```

La misma cadena para las dos, y un `_2` puesto por el desempate. Las tres comprobaciones de la
prueba —las dos cadenas y la ausencia de `_2`— se ponen rojas las tres con esa sustitución. Eso es
lo que hace que la prueba mida el invariante 5 del documento 121: **el nombre C depende sólo de la
identidad del tipo, nunca del orden de declaración**.

La medición se hizo, como la de la sección 12, llamando a `CHeaderGenerator::writeHeader`
directamente: `Stage48_1CHeaderGeneratorTests` sigue sin enlazar en este contenedor.

---

## 14. El `$` obliga a LLVM a entrecomillar, y eso costó dieciocho comprobaciones

La sección 12 dejó escrito, en `TypeIdentity.h`, que «`$` es un carácter ordinario en un símbolo
COFF y en un identificador de LLVM, que es donde acaba». La primera mitad es cierta. La segunda es
falsa, y es el mismo error que este proyecto lleva siete vueltas persiguiendo: **afirmé una
propiedad sin comprobarla contra la cosa que de verdad la decide** —aquí, el impresor de IR—.

`AsmWriter::PrintLLVMName` entrecomilla un nombre en cuanto contiene un byte que no sea
alfanumérico, `-`, `.` o `_`. `$` no está en esa lista. De modo que lo impreso no es

```
define i32 @__nsc_method_Mod$3A$3AA$2EB__F(...)
```

sino

```
define i32 @"__nsc_method_Mod$3A$3AA$2EB__F"(...)
```

Y como `TypeIdentity::key()` es siempre `Modulo::…`, el `::` está siempre, su codificación `$3A$3A`
está siempre, y por tanto **todo símbolo derivado de una identidad va siempre entrecomillado**. No
es una casualidad del conjunto actual de nombres: es un invariante.

### 14.1 Qué rompió, y qué no

**No rompió nada de producción.** Las comillas son de la representación **textual**: el objeto, la
tabla de símbolos, el mapa del enlazador y las bibliotecas de importación no las ven.

Lo comprobé donde hacía falta comprobarlo, porque hay código de producción que **sí** lee el texto
del IR: el escaneo de imports de `createWindowsImportLibraries`, al que llegan
`WindowsExecutableBuilder` y `WindowsLibraryBuilder` con el IR de los dos módulos concatenado. Ese
escaneo busca **cuatro** nombres —`@__C_specific_handler`, `@llvm.memset`, `@llvm.memcpy`,
`@llvm.memmove`—, los cuatro fijos y los cuatro sin `$`, así que salen sin comillas y el escaneo
está intacto.

Eso es una propiedad del conjunto que busca hoy, no una garantía. Un cero en ese escaneo no da un
rojo: da una biblioteca de importación a la que le falta una entrada, y el fallo aparece en el
enlace o más tarde. Queda anotado en el propio escáner.

**Rompió dieciocho comprobaciones**, repartidas por nueve suites, todas del mismo tipo: buscar
`@__nsc_algo` sobre el texto del IR.

### 14.2 El control que lo demuestra sin ejecutar nada

En `Stage36RecoveryTests` las líneas 615 y 617 **pasan** y la 619 **falla**:

```
615  "%nativesharp.interface_map_entry = type { i64, ptr }"   nombre fijo      -> pasa
619  "@__nsc_ifmap_"                                          derivado         -> falla
```

La misma suite, el mismo módulo, la misma llamada a `find`. La única diferencia es si el nombre
sale de una identidad. Ese contraste se repite en `DriverTests`, donde `@__nativesharp_alloc` y
`@__nsc_ctor_` conviven en la **misma** aserción y sólo cae la segunda.

### 14.3 Por qué NO se cambió el carácter de escape

La salida obvia era cambiar `$` por `.` o `-` —los dos están en la lista de LLVM— y que dejara de
entrecomillar. Se descartó, y el motivo no es la pereza:

* El formato de símbolo **ya se cambió una vez** esta semana (`Mod__A_B` → `Mod$3A$3AA$2EB`), y está
  documentado como incompatible a propósito. Cambiarlo otra vez le entrega al carril del enlazador
  un segundo formato incompatible **por una cuestión de cosmética del texto**: `$` y `.` son
  igual de inyectivos y el enlazador no distingue uno de otro.
* No hay ganancia funcional. Nada de producción lee esos nombres del texto (§14.1).

Así que las comillas se quedan, y lo que se corrige es la causa de que dieciocho pruebas se
escribieran mal: el comentario que decía lo contrario.

### 14.4 Lo que se hizo

* `TypeIdentity.h` — el párrafo falso, sustituido por el hecho medido, el invariante «siempre
  entrecomillado» y la consecuencia para quien escriba una búsqueda sobre el IR.
* `symbolNeedsIrTextQuotes`, `irTextSymbolReference`, `irTextIdentitySymbolPrefix` — la regla de
  LLVM escrita **una vez**, para no depender de que alguien la recuerde. Con prueba en
  `CompilationModelTests`, al lado de la codificación, y en los **dos** sentidos: un nombre fijo del
  runtime no lleva comillas, uno derivado sí. Con sólo el segundo, un ayudante que entrecomillara
  siempre pasaría.
* El escáner de imports — anotado con lo que asume y con qué hacer el día que busque un símbolo
  nuestro.
* Doce aserciones que buscaban `@__nsc_…` — corregidas a `@"__nsc_…`.

### 14.5 Y cuatro que no eran las comillas

Estas cuatro exigían la salida del **saneador viejo**, el que aplastaba `.` y `:` al mismo `_`. Esa
forma no puede volver bajo ninguna codificación inyectiva, que es exactamente lo que se pidió:

```
Stage40StructValueAbiTests   __Demo_S1__Echo__            -> Demo$2ES1__Echo__          (y S2/S4/S8)
                             __Demo_S8__Mutate__builtin_int(...)
                                                          -> Demo$2ES8__Mutate__builtin$3Aint"(...)
Stage56C6StructMembersTests  set_X__builtin_float(ptr     -> set_X__builtin$3Afloat"(ptr
Cc1CorrectnessTests          limit004pad_P__Iguales       -> limit004pad$2EP__Iguales
```

**Y una que estaba en VERDE y era falsa.** `Stage40StructValueAbiTests` comprobaba la AUSENCIA de
`classdesc_Stage40Abi__Demo_S8` —la forma vieja—, que desde el cambio de codificación no puede
aparecer jamás. Pasaba sola, sobre una cadena imposible, y habría seguido pasando aunque un struct
sí recibiera descriptor. Es el mismo patrón que la guarda `!cuerpo.empty()` de la prueba de relleno
existe para atrapar — y que allí, otra vez, hizo su trabajo: el recorte de función buscaba
`limit004pad_P__Iguales` y la guarda dio el rojo en vez de dejar pasar cuatro ausencias sobre una
cadena vacía.

### 14.6 Y dos contratos que cambiaron a propósito

Nada que ver con símbolos. `Stage44GenericsMonomorphizationTests` exigía dos mensajes que ya no
existen:

* `duplicate generic template identity` → hoy es `duplicate generic type identity`, y sólo para
  **tipos**: dos plantillas de método con el mismo nombre y aridad ya no son un error, son
  sobrecargas (doc 121 §12). El rechazo del programa sigue comprobándose; lo que cambió es el texto.
* `generic constraints are deferred beyond Stage 44` → **se borró** al implementar NSC-LIMIT-007.
  El programa de la prueba, `class Box<T> where T { … }`, sigue siendo un `where` mal escrito y el
  parser lo sigue rechazando, ahora por lo que de verdad le pasa: `expected ':' after the
  constrained type parameter`. La prueba se renombró a `ConstraintWithoutConstraintList`, porque su
  nombre anterior afirmaba algo que ya es falso.
