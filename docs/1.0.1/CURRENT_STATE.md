# Estado actual de NativeSharp 1.0.1

## Modelo de programación

NativeSharp 1.0.1 produce código nativo AOT para Windows x64. No existe CLR, garbage collector
implícito, reflexión administrada ni marshalling automático. El programa expresa layout, ownership,
préstamos, destrucción, excepciones y fronteras ABI de forma que el compilador pueda producir COFF,
PE32+, bibliotecas estáticas, DLLs e información PDB estándar.

La sintaxis familiar no cambia este principio. Una construcción se considera disponible cuando está
en la gramática y en la especificación, no porque exista en C#. El código de port debe distinguir:

1. una característica nativa soportada;
2. una adaptación normal a otra API o modelo de ownership;
3. una comodidad de C# deliberadamente ausente;
4. un defecto real donde NSC incumple su propio contrato.

La línea 2.0 reconoce estructuralmente `asm { ... }` como una sentencia dentro de funciones y
métodos. El lexer conserva exactamente los bytes y saltos físicos del cuerpo, y el AST registra
cada línea no vacía con su rango fuente e inicio de instrucción. Operandos, restricciones,
validación y emisión se incorporan en los ítems siguientes de la tanda 4; hasta entonces no
constituye todavía una sentencia ejecutable.

Un bloque puede declarar entradas posicionales mediante `asm { ... } in(expr0, expr1);`. Las
expresiones usan el binding NativeSharp ordinario, se evalúan en orden y corresponden a `$0`, `$1`,
etc. dentro de la plantilla. La cláusula no vacía exige el `;`; un bloque sin cláusulas conserva la
forma breve `asm { ... }`.

Los destinos de salida se declaran antes de las entradas:
`asm { ... } out(destino0, destino1) in(expr0);`. Cada destino debe ser un lvalue modificable y la
sentencia lo trata como escritura, por lo que puede inicializar una local todavía no leída. La
plantilla los nombra `$out0`, `$out1`, etc., sin renumerar los placeholders `$0`, `$1` de entrada.

Cada input u output puede declarar un constraint de registros con `: "..."`. Las entradas aceptan
registro general (`"r"`) o fijo (`"{rcx}"`); las salidas distinguen escritura (`"=r"`,
`"={rax}"`) y lectura-escritura (`"+r"`, `"+{rax}"`). Si se omite, el compilador conserva la
sintaxis anterior y normaliza a `r` o `=r`. Los nombres concretos se contrastarán con el target en
el item de validación x64; esta etapa ya fija y verifica estructura y dirección.

Los constraints `"i"` y `"m"` modelan inmediatos integrales constantes y ubicaciones de memoria
direccionables. Outputs de memoria usan `"=m"` o `"+m"`. El compilador distingue una ubicación
real de una property asignable y rechaza cálculos temporales; no crea spills ocultos para satisfacer
el assembly.

## Superficie del lenguaje

La edición cubre namespaces, clases parciales y anidadas, structs, interfaces, herencia, miembros
estáticos, propiedades, indexadores, operadores, conversiones de usuario, enums, arrays, punteros,
delegates, eventos, lambdas y closures. El flujo incluye condicionales, ciclos, `switch`, excepciones
nativas, `finally`, `using`, RAII y destrucción determinista.

El layout explícito admite `align(N)` tanto en el agregado como en un campo de instancia. En un
campo actúa como alineación mínima, se aplica después de `pack(N)` y eleva también la alineación y
el padding final del contenedor. `at(offset)` puede combinarse con ella sólo cuando el offset
satisface la alineación declarada. El mismo contrato se publica en NSMETA y en headers C.

Un struct puede terminar en `T[*] Tail;` para modelar un flexible array member de C. El campo fija
el offset y la alineación de `T`, pero no agrega un elemento a `sizeof(struct)`. El propietario
reserva el encabezado más los elementos y conserva explícitamente su cantidad. Sólo se admiten
elementos de lifetime trivial; la forma no puede aparecer en clases, unions, locales ni firmas.

`readonly struct` convierte la inmutabilidad posterior a construcción en propiedad del tipo. Cada
campo de instancia debe declararse `readonly`; el constructor del propio struct puede inicializarlo,
pero ningún método ordinario puede reasignarlo. Los campos estáticos no forman parte del valor y
mantienen sus reglas normales. El modificador no cambia layout ni ABI y sobrevive en NSMETA.

Un método, property getter o indexer getter de struct puede declarar `readonly`. Su `this` permite
lecturas, pero no stores ni llamadas a miembros mutadores. La misma restricción se propaga al usar
un struct mediante un parámetro `in` o un campo readonly. Todos los miembros de instancia de un
`readonly struct` reciben el contrato implícitamente. No se generan copias defensivas ocultas.

Los arreglos locales `T[length]` pueden tomar su longitud de una expresión integral evaluada al
ejecutar la declaración. Su almacenamiento es contiguo, pertenece al frame actual y no requiere
`new` ni `delete`. Esta forma no se extiende a campos, parámetros o retornos: esas posiciones forman
parte de contratos de layout o ABI y conservan dimensiones constantes o tipos dinámicos explícitos.
La forma admite varias dimensiones: `T[rows, columns]` captura cada extensión una sola vez y calcula
direcciones row-major sobre una única reserva contigua.

Las dimensiones con signo se comprueban antes de convertirse a `nuint`. El producto completo se
valida sin overflow y cada declaración limita su reserva a 1 MiB. Un VLA es almacenamiento del
frame, no un puntero propietario: puede prestarse durante la llamada actual, pero no retornarse,
guardarse en un campo ni capturarse en una lambda que pueda sobrevivir al frame. Los aliases
locales de puntero conservan esa procedencia.

Los strings son objetos nativos. `const string` representa una constante de compilación; los
literales, comparaciones y operaciones documentadas no dependen de `System.String` ni del CLR. Los
constructores pueden recibir préstamos de string conforme a las reglas de lifetime y escape.

Los genéricos se monomorfizan AOT. La identidad de una especialización cerrada incluye sus tipos y
su origen declarador; las interfaces, eventos, conversiones, constructores, destructores y miembros
publicados deben conservar una identidad física utilizable cuando cruzan una biblioteca.

Un alias de tipo puede parametrizarse: `using Pointer<T> = T*;` y
`using PairOf<T> = Pair<T, T>;`. El compilador sustituye sus argumentos y expande cadenas antes de
calcular especializaciones genéricas. El alias es transparente: layout, ownership, overloads,
metadata y ABI observan exclusivamente el tipo expandido. Una aridad incorrecta o un ciclo se
rechazan en compilación.

Los métodos genéricos admiten inferencia local en llamadas como `Identity(value)`,
`First(buffer)` para un parámetro `T[]` y `Read(box)` para un parámetro `Box<T>`. El compilador
unifica la forma declarada con el tipo local del argumento antes de monomorfizar. Literales,
locals explícitos o `var`, parámetros, campos, casts, `new`, arrays, punteros y argumentos
by-reference aportan evidencia. `null`, una lambda sin target o un parámetro presente sólo en el
retorno no aportan tipo; en esos casos se escriben los argumentos explícitos.

El compilador modela el inlining con cuatro políticas semánticas exclusivas: automático,
preferido (`inline`), obligatorio (`forceinline`) y prohibido (`noinline`). La política gobierna
la decisión de optimización, nunca el significado del programa. Evaluación, efectos, ownership,
cleanups, excepciones, dispatch dinámico, ABI, símbolos públicos y observabilidad de depuración
deben permanecer correctos con o sin expansión. Esta entrega fija el contrato interno; la sintaxis
y el lowering de cada modificador se habilitan en los siguientes ítems de la tanda 3.

En política automática, O0 conserva límites de llamada. O1, O2 y O3 usan los pipelines canónicos
de LLVM y su modelo de costo dependiente del target con agresividad creciente. El compilador no
mantiene umbrales paralelos ni heurísticas ocultas que puedan discrepar del nivel solicitado.

Un método concreto puede declarar `inline`. El modificador expresa preferencia y llega a LLVM como
`inlinehint`; el optimizador aún puede conservar la llamada cuando expandir no sea legal o rentable.
No se admite en fields, properties, tipos, imports ni contratos abstractos. No elimina por sí mismo
la definición fuera de línea ni cambia dispatch, ABI o ownership.

`forceinline` eleva esa política a una obligación para llamadas con destino estáticamente resuelto.
El backend la representa como `alwaysinline` y ejecuta la expansión obligatoria incluso en O0, sin
habilitar por ello el inlining automático. Sólo se admite en métodos concretos con cuerpo. Una
combinación con `inline`, un contrato abstracto o un ciclo formado exclusivamente por métodos
`forceinline` se rechaza en la fuente en vez de degradar silenciosamente la promesa.

`noinline` fija la política opuesta: el método concreto conserva el límite de llamada aun bajo O2
u O3. LLVM recibe `noinline`, y el modificador no cambia visibilidad, identidad física ni ABI.
Sólo se admite donde existe un cuerpo ordinario; duplicados, contratos abstractos y cualquier
combinación con `inline` o `forceinline` son errores de compilación.

La monomorfización conserva la política de inlining declarada por un método genérico. Esto vale
tanto para especializaciones solicitadas con `<T>` como para las creadas por inferencia local. Cada
función física cerrada recibe exactamente la política de su plantilla: preferencia, obligación o
prohibición, sin volver a decidirla a partir del nombre especializado.

Los métodos concretos de structs participan en el mismo contrato sin boxing ni cambio de ABI. Para
interfaces, el compilador devirtualiza únicamente una conversión cuyo operando es una construcción
de clase fresca y exacta, usando la tabla semántica de implementación para seleccionar el callable.
Parámetros, fields, locals y expresiones cuyo tipo dinámico no está demostrado conservan el lookup
de interfaz en ejecución.

Las políticas cruzan `LibraryReference`. NSMETA transporta la declaración y el cuerpo de métodos
ordinarios `inline` y `forceinline`; el consumidor los analiza como código local reutilizable y
emite una copia `weak_odr` con COMDAT por símbolo. `noinline` transporta sólo la política y conserva
la llamada hacia la identidad física del productor. El mismo mecanismo cubre métodos estáticos,
receptores de clase y receptores de struct; las plantillas genéricas continúan usando su clausura
reutilizable especializada por el consumidor.

Una obligación `forceinline` requiere que caller y callee estén juntos durante la optimización.
Hasta disponer de summaries importables entre unidades, el generador conserva como módulo único
solamente los productos que contienen esa obligación. En O0 el PDB resultante representa la
expansión mediante un `S_INLINESITE` con líneas y locales del cuerpo, mientras `inline` preferido y
`noinline` conservan sus procedimientos y límites de llamada. Proyectos O0 sin `forceinline`
continúan usando generación particionada y paralela.

## Ownership y lifetime

`owned` transfiere responsabilidad; `borrowed` concede acceso sin transferencia. Las salidas,
retornos, campos y capturas se validan contra escapes. `raii` liga la destrucción al scope y las
rutas excepcionales ejecutan sus cleanups nativos. La raíz `Object` aporta identidad y jerarquía,
pero no introduce boxing ni un recolector.

El estado global propietario no se inventa mediante `static owned`: la ausencia de finalización
global honesta hace que esa forma permanezca fuera del contrato. Las bibliotecas y aplicaciones
deben elegir un propietario con lifetime explícito.

## Interop y ABI

El target contractual usa el ABI Microsoft x64. NativeSharp expresa `extern C`, imports desde DLL,
exports C, callbacks, punteros crudos, structs por valor, parámetros `ref/out/in`, uniones, arrays de
tamaño fijo y handles tipados. Las llamadas por vtable se construyen con punteros y delegates
`extern C`; no se requiere un sistema de tipos COM integrado al compilador.

Los delegates que cruzan una frontera C usan una representación y una barrera de excepciones
definidas. Los eventos reutilizan esa ABI, incluido el paso indirecto de agregados que Win64 no
transporta directamente por valor. Las aplicaciones GUI pueden seleccionar
`/subsystem:windows`; las de consola conservan `/subsystem:console`.

## Bibliotecas precompiladas

Un producto precompilado combina `.lib`, `.nsmeta` y, para una biblioteca compartida, `.dll` más su
import library. `LibraryReference` consume el contrato sin incorporar las fuentes del productor.
NSMETA 1.13 conserva:

- tipos, miembros, accesibilidad efectiva y nombres físicos;
- constantes y valores necesarios para reconstruir la API;
- imports `extern C` y DLL propietaria;
- templates reutilizables y clausura transitiva privada;
- especializaciones genéricas cerradas alcanzables;
- dependencias nominales provenientes de otras bibliotecas;
- política de inlining y cuerpos ordinarios reutilizables entre productor y consumidor;
- requisitos de runtime seleccionados únicamente cuando un símbolo alcanzado los necesita.

`--verify-library-artifacts` compara metadata, archivo COFF, import library y tabla de exportaciones.
La emisión debe fallar temprano si un callable público carece de identidad física; no es aceptable
producir metadata que sólo falle al compilar al consumidor.

## Depuración

La ruta de depuración usa CodeView en objetos COFF y Microsoft PDB/MSF en el ejecutable final. `-g0`,
`-gline-tables-only` y `-g` son independientes de `-O0` a `-O3`. El contrato incluye checksums de
fuente, remapeo reproducible de prefijos, líneas, parámetros, locales, `this`, structs, funciones
sintetizadas, dispatch virtual e interfaz, SEH, closures y fidelidad bajo optimización.

En O0, una expansión `forceinline` conserva frame inline, ubicación de llamada y líneas internas
recorribles. Una preferencia `inline` no cumplida y una frontera `noinline` se describen como frames
ordinarios; la información de depuración refleja el código realmente emitido, no sólo el modificador.

Esta superficie continúa en desarrollo dentro de 1.0.1. La existencia del contrato indica qué debe
producirse y verificarse; el sufijo `-dev` evita presentar el trabajo actual como baseline final.

## Biblioteca nativa

La distribución conserva proyectos NativeSharp para Core, colecciones genéricas, IO, diagnósticos y
threading. Incluye strings, arrays y spans, archivos y rutas, tiempo, ambiente, argumentos de proceso,
colecciones, sincronización y utilidades nativas. Una aplicación puede referenciar esas fuentes o
consumir productos precompilados cuando el paquete correspondiente los publique.

## Diferencias deliberadas respecto de C#

No forman parte del contrato necesario de NativeSharp `params`, argumentos nombrados, métodos de
extensión, interpolación `$"..."`, `??`, `??=` ni `?.`. Su ausencia no bloquea un port: el código
debe expresar la operación nativa equivalente. Tampoco se presupone LINQ, reflection, dynamic,
boxing, attributes COM ni generación automática de vtables.

Eso no autoriza workarounds silenciosos frente a defectos reales. Si una construcción documentada
compila dentro del productor pero pierde identidad al consumirse desde NSMETA, si el ABI pasa un
agregado con forma incorrecta, o si la tabla PE diverge de la metadata pública, el defecto pertenece
al toolchain y debe reducirse sin deformar la API portada.

## Validación de desarrollo

El flujo activo de esta línea prioriza O0 para iteración rápida. Los builds locales pueden compilarse
e instalarse sin baseline; conservan `-dev` y su identificador único. Un baseline es una acción
separada y explícita. Esta documentación no afirma que el contenido copiado haya sido recertificado
con el build que la generó.
