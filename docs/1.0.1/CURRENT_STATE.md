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

## Superficie del lenguaje

La edición cubre namespaces, clases parciales y anidadas, structs, interfaces, herencia, miembros
estáticos, propiedades, indexadores, operadores, conversiones de usuario, enums, arrays, punteros,
delegates, eventos, lambdas y closures. El flujo incluye condicionales, ciclos, `switch`, excepciones
nativas, `finally`, `using`, RAII y destrucción determinista.

Los arreglos locales `T[length]` pueden tomar su longitud de una expresión integral evaluada al
ejecutar la declaración. Su almacenamiento es contiguo, pertenece al frame actual y no requiere
`new` ni `delete`. Esta forma no se extiende a campos, parámetros o retornos: esas posiciones forman
parte de contratos de layout o ABI y conservan dimensiones constantes o tipos dinámicos explícitos.
La forma admite varias dimensiones: `T[rows, columns]` captura cada extensión una sola vez y calcula
direcciones row-major sobre una única reserva contigua.

Los strings son objetos nativos. `const string` representa una constante de compilación; los
literales, comparaciones y operaciones documentadas no dependen de `System.String` ni del CLR. Los
constructores pueden recibir préstamos de string conforme a las reglas de lifetime y escape.

Los genéricos se monomorfizan AOT. La identidad de una especialización cerrada incluye sus tipos y
su origen declarador; las interfaces, eventos, conversiones, constructores, destructores y miembros
publicados deben conservar una identidad física utilizable cuando cruzan una biblioteca.

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
NSMETA 1.12 conserva:

- tipos, miembros, accesibilidad efectiva y nombres físicos;
- constantes y valores necesarios para reconstruir la API;
- imports `extern C` y DLL propietaria;
- templates reutilizables y clausura transitiva privada;
- especializaciones genéricas cerradas alcanzables;
- dependencias nominales provenientes de otras bibliotecas;
- requisitos de runtime seleccionados únicamente cuando un símbolo alcanzado los necesita.

`--verify-library-artifacts` compara metadata, archivo COFF, import library y tabla de exportaciones.
La emisión debe fallar temprano si un callable público carece de identidad física; no es aceptable
producir metadata que sólo falle al compilar al consumidor.

## Depuración

La ruta de depuración usa CodeView en objetos COFF y Microsoft PDB/MSF en el ejecutable final. `-g0`,
`-gline-tables-only` y `-g` son independientes de `-O0` a `-O3`. El contrato incluye checksums de
fuente, remapeo reproducible de prefijos, líneas, parámetros, locales, `this`, structs, funciones
sintetizadas, dispatch virtual e interfaz, SEH, closures y fidelidad bajo optimización.

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
