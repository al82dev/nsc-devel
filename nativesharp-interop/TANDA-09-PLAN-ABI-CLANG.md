# T09-003: plan para transportar el ABI calculado por Clang

Estado: diseño en curso. No habilita agregados SysV ni declara terminada la
conservacion de todas las convenciones.

## Evidencia y punto de partida

`ClangSession.cpp` ya mantiene una unidad y puede construir un `CodeGenerator`
con el AST y las opciones de esa unidad. La sonda
`nsc.cpp-interop.clang-abi-probe` verifico que la API publica de Clang 23.1.0
entrega tanto `CGFunctionInfo` como la firma LLVM y sus atributos para las
muestras descritas en [la bitacora SysV](TANDA-09-SYSV-ABI.md).

El contrato de snapshots `CppFunctionDeclaration` no contiene un plan ABI;
`CppSemanticFunction` proyecta su simbolo, convencion y tipos al modelo de
funciones de NSC. HIR y MIR conservan esos datos, pero `ProjectOopMirFunctionAbi`
representa un portador directo o indirecto y una posicion fisica por parametro.
Eso no describe por si solo una descomposicion SysV en dos registros ni `byval`.

## Representacion a implementar y comprobar

1. Obtener el plan mientras viven AST, generador y contexto LLVM. Copiarlo a
   valores propios de NSC; no guardar punteros, referencias ni nombres de
   tipos temporales de Clang o LLVM. Una forma no representable se informa como
   no disponible y sigue rechazada; no se sustituye por la clasificacion Win64.
2. Distinguir la firma logica de la fisica: retorno, parametros fisicos,
   convencion y atributos; por cada argumento logico conservar su clase
   (`direct`, `extend`, `indirect`, `ignore` u otra representada), coercion,
   desplazamiento, alineacion, `byval` y posibilidad de descomposicion.
   Conservar el orden y cualquier retorno oculto. Los atributos de extension,
   `sret`, `byval` y alineacion deben llegar a declaracion y llamada.
3. Representar por valor los tipos LLVM necesarios, incluyendo entero de ancho
   arbitrario, flotantes, puntero, estructuras y los elementos que realmente
   sean necesarios para el caso. Registrar empaquetado y alineacion cuando
   afecten el contrato; rechazar tipos aun no representados. No identificar un
   tipo solo por el texto que imprime LLVM.
4. Transportar el plan desde la declaracion importada hasta el backend sin
   romper identidad ni las rutas cdecl y vectorcall existentes. El plan debe
   corresponder a la declaracion y configuracion de sesion efectivas. La ruta
   precompilada debe conservar la informacion necesaria mediante el unico
   contrato NSMeta vigente, sin etiquetas o estrategias historicas.
5. Emitir conversiones y copias segun el plan, con pruebas dirigidas para R3,
   R16, record mixto y R24, y contraste contra Clang. Cubrir estatica, DLL,
   optimizaciones, retorno, paso por valor, alineacion y aliasing; probar
   tambien el rechazo de una forma que el plan todavia no represente.

## Riesgos abiertos

La sonda demuestra la consulta de firmas concretas, no demuestra que el plan
se pueda trasladar o serializar sin perdida. Falta verificar la correspondencia
de cada pieza fisica con los argumentos logicos, el significado de atributos
de optimizacion, el coste de crear el generador por sesion y la dependencia del
target. Las funciones variadicas requieren el plan de la llamada concreta, no
solo el de la declaracion. Las clases C++ no triviales, la destruccion y el
desenrollado de excepciones requieren contratos adicionales posteriores.

## Primer prototipo aislado

`include/nativesharp/Interop/CppAbiPlan.h` y
`src/Interop/Cpp/ClangAbiPlan.cpp` contienen una copia por
valor experimental: tipos fisicos y de coercion representables, clases logicas,
alineacion y atributos seleccionados. La sonda CTest copia planes de R3, R16,
un record mixto, R24 y signed char; destruye el generador y la unidad Clang
y verifica los datos copiados. Tambien exige el rechazo de un vector LLVM que
el prototipo aun no representa. La prueba paso 1/1, registros
`A:/temp/t09-abi-probe-build.log` y `A:/temp/t09-abi-probe-ctest.log`.

El conversor se compila en `nsc_cpp_interop`, pero todavia no se conecta a los
snapshots de produccion, al backend ni a NSMeta. La sonda paso tras el traslado
y NSC se compilo incrementalmente; registros A:/temp/t09-abi-probe-ctest.log y
A:/temp/t09-compilar-sysv.log. Todavia no representa todas las
clases `ABIArgInfo`, atributos, mapeos logico-fisicos o tipos LLVM. El rechazo
del vector valida el limite del prototipo, no implementa interoperabilidad con
vectores. Antes de utilizar estos datos hay que completar y comprobar esos
contratos para las formas que se habiliten.
La regresion de las firmas SysV ya admitidas paso de nuevo en estatica y DLL
O0–O3, con la muestra precompilada y los rechazos, en
A:/temp/nsc-t09-sysv-prueba-rwppfjgv.

Ampliacion del prototipo: se asignan posiciones y cantidades fisicas a cada
argumento logico, comprobando los tipos que Clang genero. Se contemplan un
retorno oculto `sret`, expansion de una estructura de coercion y argumentos
indirectos; padding no representado, tipos no reconocidos y llamadas variadicas
se rechazan. Una firma mixta con retorno oculto, escalar, record mixto, record
de 3 bytes y argumento `byval` paso la sonda, junto con los rechazos de vector y
variadica. CTest paso 1/1 en A:/temp/t09-abi-probe-ctest.log. El mapeo es
experimental y aun no se usa para generar llamadas ni garantiza toda forma ABI.

El plan copiado ahora conserva el triple y `data layout` de la unidad; la
prueba los inspecciona despues de destruir la sesion. Se agrego un record SysV
empaquetado con `char` y `double`: Clang usa retorno `sret` alineado a 1 y
argumento `byval` alineado a 8. La copia retiene ambas alineaciones y el tipo
empaquetado; la prueba CTest paso 1/1 en A:/temp/t09-abi-probe-ctest.log.
El caso sigue sin habilitarse en NSC.

La copia incorpora el simbolo LLVM y la convencion numerica extraidos de la
declaracion de Clang. La sonda comprobo despues de cerrar la sesion el marcador
de escape y el nombre decorado de `pvec@@8`, junto con vectorcall. CTest paso
1/1; sigue siendo una sonda aislada, no una ruta nueva de llamadas.

La prueba incluye ademas una funcion `void(void)` SysV y comprueba que no
aparezcan parametros fisicos ni resultado materializado; una declaracion C
sin prototipo se rechaza en el prototipo. CTest paso 1/1 en
A:/temp/t09-abi-probe-ctest.log. Esto no establece el soporte de las llamadas
C sin prototipo en la ruta de produccion.

La copia experimental ahora acepta una lista acotada de atributos de retorno y
parametros; ante otro atributo devuelve un error en lugar de descartarlo en
silencio. La sonda exige el rechazo de una funcion con `returns_nonnull`, ademas
de conservar las comprobaciones anteriores. CTest paso 1/1. Los atributos de
optimizacion aceptados que no se copian no se interpretan como garantias nuevas;
la politica aun debe revisarse antes de usar el plan para emitir llamadas.
La comprobacion de vida tambien destruye ahora el contexto LLVM antes de leer
los planes copiados; CTest volvio a pasar 1/1.

## Consulta experimental en una sesion real

`CppInteropSession::cFunctionAbiPlan` permite solicitar el plan de una funcion
C externa por nombre y tipo canonico dentro de su sesion. La implementacion
Clang limita la consulta al modo C, evita declaraciones ambiguas o incompletas,
usa el generador temporal bajo el bloqueo de sesion y devuelve la copia por valor.
No es una consulta de metodos o templates C++ y `nullopt` tambien representa
casos no soportados. El consumidor del compilador aun no usa esta consulta.

La sonda abre una sesion real, obtiene planes de R3, R24, una firma mixta y
vectorcall; verifica seleccion por tipo, preservacion de diagnosticos, rechazo
de tipo no representado, variadica, declaracion sin prototipo y atributo no
representado. Tras cerrar y destruir la sesion, comprueba los planes copiados;
una consulta posterior al cierre devuelve ausencia. CTest paso 1/1 en
A:/temp/t09-abi-probe-ctest.log. El primer intento no abrio por faltar el
directorio de trabajo temporal; la prueba ahora lo crea y fue repetida.
Esto aun no implementa llamadas de agregados ni guarda planes en NSMeta.
La sonda ampliada tambien paso con una redeclaracion C compatible; rechazo un
parametro de tipo incompleto y una consulta sobre una sesion C++. Por ahora se
crea un generador por consulta; no se ha medido su coste ni se ha habilitado un
cache, y la informacion queda atada a la configuracion de la sesion consultada.
Tras enlazar la consulta, NSC se construyo incrementalmente y la regresion
SysV existente paso en A:/temp/nsc-t09-sysv-prueba-lzpmdeme.

## Transporte al inventario y proyeccion

El inventario solicita el plan solo para funciones C externas `sysv_abi` con
un record por valor en resultado o parametros. La declaracion y el simbolo
semantico pueden conservar una referencia compartida e inmutable al valor
propio del plan; no contiene punteros de Clang. Si la consulta no puede
representar el plan, queda ausente. Esto no habilita la llamada: HIR sigue
rechazando agregados SysV y no hay transporte precompilado del plan aun.

La sonda construye una sesion y un grafo de proyecto temporales, comprueba que
el inventario adjunta el plan a una declaracion con record y no lo solicita
para las muestras escalares o de puntero, y lee el plan tras cerrar la sesion.
Tambien verifica el transporte por proyeccion semantica con una firma escalar y
otra con puntero a record. El primer intento de proyectar por valor fallo porque
el fixture no habia instalado el tipo con su layout; no se presenta como prueba
de esa proyeccion. El control de inventario requirio crear un archivo de proyecto
temporal para que la comprobacion de identidad del grafo pudiera encontrarlo.
Tras esos ajustes, CTest paso 1/1: A:/temp/t09-abi-probe-ctest.log. La
construccion incremental paso y las regresiones dirigidas de SysV y cdecl pasaron
en A:/temp/nsc-t09-sysv-prueba-p98m9rp8 y A:/temp/nsc-t09-llamadas-yuidcadn.

Se completo despues la prueba de proyeccion por valor: la sonda instala los
tipos y layouts observados en el lookup de un grafo temporal y proyecta la
funcion `p3`, verificando que su simbolo semantico conserve el plan. CTest paso
1/1 en A:/temp/t09-abi-probe-ctest.log. Esta comprobacion no emite ni ejecuta
la llamada por valor; HIR continua rechazandola hasta implementar su ABI.

## Transporte HIR y MIR

El simbolo de funcion puede pasar la referencia inmutable del plan a HIR y MIR.
Una prueba construye un proyecto temporal, inyecta una funcion SysV escalar
con plan obtenido de Clang y comprueba la misma informacion en ambas etapas;
la sonda CTest paso 1/1 en A:/temp/t09-abi-probe-ctest.log. El backend aun no
utiliza el plan para registros y la barrera de HIR sigue activa para ellos.
Este control no demuestra la persistencia por NSMeta de planes de agregados.
NSC se enlazo incrementalmente y las regresiones SysV y vectorcall pasaron
8/8 cada una en A:/temp/nsc-t09-sysv-prueba-yjxkpyye y
A:/temp/nsc-t09-vectorcall-5y2ug6rd.
