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

`pruebas-tanda-09-sysv/ClangAbiSnapshot.h` y `.cpp` contienen una copia por
valor experimental: tipos fisicos y de coercion representables, clases logicas,
alineacion y atributos seleccionados. La sonda CTest copia planes de R3, R16,
un record mixto, R24 y signed char; destruye el generador y la unidad Clang
y verifica los datos copiados. Tambien exige el rechazo de un vector LLVM que
el prototipo aun no representa. La prueba paso 1/1, registros
`A:/temp/t09-abi-probe-build.log` y `A:/temp/t09-abi-probe-ctest.log`.

Este codigo solo se compila dentro del target de prueba; no se conecta a los
snapshots de produccion, al backend ni a NSMeta. Todavia no representa todas las
clases `ABIArgInfo`, atributos, mapeos logico-fisicos o tipos LLVM. El rechazo
del vector valida el limite del prototipo, no implementa interoperabilidad con
vectores. Antes de utilizar estos datos hay que completar y comprobar esos
contratos para las formas que se habiliten.

Ampliacion del prototipo: se asignan posiciones y cantidades fisicas a cada
argumento logico, comprobando los tipos que Clang genero. Se contemplan un
retorno oculto `sret`, expansion de una estructura de coercion y argumentos
indirectos; padding no representado, tipos no reconocidos y llamadas variadicas
se rechazan. Una firma mixta con retorno oculto, escalar, record mixto, record
de 3 bytes y argumento `byval` paso la sonda, junto con los rechazos de vector y
variadica. CTest paso 1/1 en A:/temp/t09-abi-probe-ctest.log. El mapeo es
experimental y aun no se usa para generar llamadas ni garantiza toda forma ABI.
