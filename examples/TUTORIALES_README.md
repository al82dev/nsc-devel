# NativeSharp -- Paquete de ejemplos academicos (23 tutoriales)

Este paquete es material **didactico**, no una suite de tests: cada carpeta `tutorial_NN_*` es un
proyecto NativeSharp completo, independiente, pensado para leerse y ejecutarse. Cada uno imprime
salida real por consola (`Console.WriteLine`) y termina con un codigo de salida verificable a mano
(documentado en un comentario al final de cada `Main()`), para poder confirmar que el programa
hizo lo que dice que hace sin tener que confiar "a ojo" en el resultado.

## Como compilar y correr un ejemplo

Cada carpeta trae su propio `build.bat` y `clean.bat` (Windows -- son los que produce este
compilador, que emite ejecutables PE32+ nativos de Windows x64). Parado en la carpeta del
ejemplo:

```
build.bat     rem compila Main.ns -> TutorialNN....exe, usando el nsc.exe ya compilado del repo
TutorialNN....exe
clean.bat     rem borra el .exe/.pdb/.ilk generados
```

`build.bat` busca `nsc.exe` en `build\bin`, `build-clang\bin`, `build-msvc\bin` o
`build-ninja\bin` dentro de la raiz de ESTE paquete (dos niveles arriba de la carpeta del
ejemplo); si tu compilador vive en otro lado (por ejemplo, ya compilado dentro del repo completo
de NativeSharp), define la variable de entorno `NSC_EXE=ruta\a\nsc.exe` antes de correr
`build.bat`. En Linux/Wine (como se verifico este paquete durante su desarrollo), el equivalente
es invocar `nsc` directamente con `--emit-project-oop-exe` sobre el `.nsproj` de la carpeta y
correr el resultado bajo `wine`.

Este paquete es **autocontenido**: trae su propia copia de `lib/NativeSharp.Core` y
`lib/NativeSharp.Core.Collections.Generic` (las mismas que usa el repo completo), asi que cada
`.nsproj` resuelve sus `ProjectReference` (`../../lib/...`) sin depender de nada externo -- se
verifico compilando y corriendo los 23 ejemplos desde una extraccion limpia de este mismo zip,
usando solo lo que trae adentro. Si preferis integrarlos al repo completo de NativeSharp en vez de
usarlos sueltos, simplemente copia las 23 carpetas `tutorial_NN_*` dentro de la carpeta
`examples/` del repo (junto a los `project_stageNN` existentes) -- ahi las mismas rutas relativas
`../../lib/...` apuntan directo al `lib/` real del repo, sin necesidad de la copia incluida aca.

## Fuentes de inspiracion

- **K&R** -- Brian W. Kernighan y Dennis M. Ritchie, *The C Programming Language* (el libro que
  el usuario adjunto en PDF). Cubre los tutoriales 1 a 6 (fundamentos: variables, control de
  flujo, funciones, arreglos/strings, structs).
- **Systems Programming with C# and .NET** (`SystemsProgrammingwithCSharpand.NETmain.zip`) --
  interfaces (tutorial 9) y varias ideas de diseño de tipos a lo largo del paquete.
- **Learn C# Programming** (`LearnCSharpProgrammingmaster.zip`) -- clases/objetos, herencia,
  excepciones (tutoriales 7, 8, 19).
- **C# Data Structures & Algorithms Fundamentals**
  (`CSharpDataStructuresAlgorithmsFundamentalsmain.zip`) -- generics, colecciones, delegates,
  eventos, recursion, busqueda/ordenamiento (tutoriales 10, 14-18, 20-21).
- **AllFiles.zip** (curso clasico de estructuras de datos en C++, estilo Sahni: genBST.h,
  genDLList.h, HuffmanCoding.h/HuffmanEncoder.cpp/HuffmanDecoder.cpp, trie.cpp, etc.) -- lista
  enlazada/pila/cola manuales y codificacion de Huffman (tutoriales 22-23).

Los primeros seis tutoriales siguen la progresion clasica de K&R capitulo por capitulo, mostrando
en cada paso el equivalente NativeSharp del C original y donde NativeSharp se aparta
deliberadamente de C (por ejemplo, sin punteros crudos, con ownership real en su lugar). Los
tutoriales 7 en adelante son terreno propio de NativeSharp como lenguaje orientado a objetos con
ownership: no tienen equivalente directo en K&R (que es C puro, sin clases).

## Indice completo

| # | Carpeta | Tema | Fuente |
|---|---------|------|--------|
| 01 | `tutorial_01_hello_mundo` | Primer programa, `Console.WriteLine`, estructura minima de un proyecto NativeSharp | K&R 1.1 |
| 02 | `tutorial_02_variables_tipos_operadores` | Tipos primitivos, operadores aritmeticos/logicos/comparacion, precedencia | K&R cap. 2 |
| 03 | `tutorial_03_control_de_flujo` | `if`/`else if`/`else`, `while`, anidamiento (sin `switch`/`do-while`/`goto`: no existen en NativeSharp) | K&R cap. 3 |
| 04 | `tutorial_04_funciones_y_recursion` | Funciones, parametros, `return`, recursion basica (factorial, fibonacci) | K&R cap. 4 |
| 05 | `tutorial_05_arrays_y_strings` | Arreglos, recorrido, `string` (reemplaza los punteros crudos de K&R por ownership real) | K&R cap. 5 |
| 06 | `tutorial_06_structs` | `struct`, semantica de VALOR (copia en asignacion), campos | K&R cap. 6 + Stage 40 ABI |
| 07 | `tutorial_07_clases_y_objetos` | `class`, semantica de REFERENCIA, modificadores de acceso, campos/metodos `static`, destructor | Learn C# cap. 4/5 |
| 08 | `tutorial_08_herencia_y_polimorfismo` | Herencia, `override`, despacho virtual (Shape/Rectangle/Square/Circle) | Learn C# cap. 6 |
| 09 | `tutorial_09_interfaces` | `interface`, una clase implementando multiples interfaces | Systems Programming cap. 1 |
| 10 | `tutorial_10_propiedades_indexadores_operadores` | Propiedades, indexadores, sobrecarga de operadores (Vector2) | DSA-Fundamentals |
| 11 | `tutorial_11_rtti_is_as` | RTTI: `is`/`as`, casteo seguro entre tipos relacionados (Animal/Dog/Cat) | Learn C# / diseño propio |
| 12 | `tutorial_12_ownership_raii_owned_borrowed_move` | El modelo de ownership de NativeSharp: `raii`, `owned`, `borrowed`, `move`, limpieza automatica de scope y ante excepcion | Propio de NativeSharp (sin equivalente en K&R/C# real) |
| 13 | `tutorial_13_ownership_en_funciones` | Ownership a traves de fronteras de funcion: crear/inspeccionar/consumir un recurso | Propio de NativeSharp |
| 14 | `tutorial_14_generics_basicos` | Tipos y metodos genericos (`Box<T>`, `Pair`, argumentos de tipo EXPLICITOS -- no hay inferencia) | DSA-Fundamentals |
| 15 | `tutorial_15_generic_collections_tour` | Tour de las 10 colecciones de `NativeSharp.Core.Collections.Generic` (List, Dictionary, HashSet, SortedList/Dictionary/Set, Queue, Stack, LinkedList, PriorityQueue) | DSA-Fundamentals |
| 16 | `tutorial_16_foreach_sobre_colecciones` | `foreach` sobre arreglo, `List`, `Queue`, `Stack`, y el patron `Keys()`/`Values()` para `Dictionary` (que no tiene enumerador propio) | DSA-Fundamentals |
| 17 | `tutorial_17_delegates_y_lambdas` | `delegate`, funciones con nombre, lambdas, lambdas de bloque, captura de variables | DSA-Fundamentals |
| 18 | `tutorial_18_eventos` | `event`, suscripcion/desuscripcion con `+=`/`-=`, multiples suscriptores | DSA-Fundamentals |
| 19 | `tutorial_19_excepciones` | `try`/`catch`/`finally`/`throw`, excepciones propias, multiples `catch`, `throw;` (rethrow) | Learn C# cap. 14 |
| 20 | `tutorial_20_recursion_clasica` | Mas patrones de recursion: sobre un entero (Power/SumRange), sobre un arreglo (SumArray), sobre un string (Reverse/IsPalindrome, "recursion que construye un resultado") | DSA-Fundamentals (Recursion II-IV) |
| 21 | `tutorial_21_busqueda_y_ordenamiento` | Busqueda lineal y binaria; Bubble/Selection/Insertion sort, todos iterativos e in-place | DSA-Fundamentals (Searching/Sorting) + K&R 5.6 |
| 22 | `tutorial_22_linked_list_stack_queue_manual` | Lista enlazada, pila y cola construidas a mano sobre un `Node` self-referential (`class Node { ...; Node Next; }`, el equivalente NativeSharp del `struct Node { ...; struct Node *next; }` de K&R 6.5) | K&R 6.5 + DSA-Fundamentals + AllFiles.zip |
| 23 | `tutorial_23_huffman_priority_queue` | Codificacion de Huffman completa (construccion del arbol, generacion de codigos, encode/decode con verificacion de round-trip) usando `PriorityQueue<TElement,TPriority>` real | AllFiles.zip (HuffmanCoding.h/Encoder/Decoder) |

## Progresion sugerida de lectura

1. **Fundamentos (K&R): 01 -> 06** -- sintaxis base, tipos, control de flujo, funciones, arreglos/
   strings, structs.
2. **Orientacion a objetos: 07 -> 11** -- clases, herencia, interfaces, propiedades/operadores,
   RTTI.
3. **Lo propio de NativeSharp -- ownership: 12 -> 13** -- el tema que mas distingue a este
   lenguaje de C#/Java/C: quien es dueño de que memoria y cuando se libera, sin garbage collector.
4. **Genericos y colecciones: 14 -> 16** -- tipos genericos, la libreria de colecciones completa,
   `foreach`.
5. **Programacion funcional/orientada a eventos: 17 -> 18** -- delegates, lambdas, eventos.
6. **Manejo de errores: 19** -- excepciones.
7. **Algoritmos clasicos: 20 -> 23** -- recursion, busqueda/ordenamiento, estructuras de datos
   manuales, y un algoritmo real de punta a punta (Huffman) que junta recursion + arreglos +
   clases self-referential + `PriorityQueue<T>` en un solo programa.

## Limitaciones reales del lenguaje encontradas escribiendo estos ejemplos

Estos son hallazgos genuinos sobre el estado actual del compilador (no son "trucos" del ejemplo,
son restricciones reales que cualquier programa NativeSharp tiene que respetar hoy), documentados
inline en el codigo del tutorial donde aplican:

- No hay `switch`/`case`, `do-while`, `goto`, ni operador ternario `?:` -- se usan cadenas de
  `if`/`else if`/`else` (ver tutorial_03).
- No hay parametros `ref`/`out`.
- `++`/`--` estan tokenizados pero el lowering OOP (Stage 34) todavia no los soporta -- se usa
  `x = x + 1;` explicito (ver tutorial_02 en adelante).
- Asignacion compuesta (`+=`, `-=`, etc.) tampoco esta lowered en modo OOP -- se usa
  `x = x + 5;` (ver tutorial_02 en adelante).
- Un argumento de llamada solo acepta un valor ya materializado: una llamada owned-returning
  anidada directamente como argumento (`Concat(Copy(x), ToString(y))`) falla -- cada resultado
  owned necesita su propio local `raii` primero (ver casi todos los tutoriales que usan `String`).
- Los genericos requieren argumentos de tipo EXPLICITOS -- no hay inferencia desde los argumentos
  (ver tutorial_14).
- `foreach` sobre un `raii` local PREEXISTENTE falla ("RAII owner cannot be copied into a
  non-owning local") -- la coleccion tiene que declararse sin `raii` y liberarse con `delete`
  explicito al final (ver tutorial_16).
- El resultado de un operador sobrecargado no se reconoce como una expresion owned-returning,
  incluso si el metodo del operador declara `owned T` como retorno -- `raii T x = a + b;` falla.
  El ejemplo de operadores (tutorial_10) usa locales `class` planos (no `raii`) como workaround,
  igual que la convencion ya usada en `project_stage38`.
- Una lambda puede LEER pero no puede MUTAR una variable local capturada del scope externo
  ("left side of assignment is not assignable") -- para mutar estado desde una lambda suscrita a
  un evento hay que pasar por un campo de un objeto en vez de una variable capturada (ver
  tutorial_18).

## Bug real de compilador encontrado durante este trabajo

Al escribir `tutorial_19_excepciones` se encontro un **crash real y reproducible del compilador**
(SIGSEGV, exit 135 bajo Wine), fuera del alcance de este paquete de ejemplos para arreglar (es un
bug del compilador, no algo que un ejemplo academico deba resolver). Repro minimo, condiciones
necesarias confirmadas por bisección, e hipotesis sin verificar en:

**`BUGS_ENCONTRADOS/bug_try_catch_reuso_objeto_tras_catch.md`**

Resumen: reusar el MISMO objeto `raii` dentro de un segundo `try` (con o sin `finally`) DESPUES de
que un primer `try`/`catch` sobre ese mismo objeto atrapo una excepcion de verdad, crashea el
binario. `tutorial_19_excepciones/Main.ns` esta escrito para evitarlo (cada seccion de demo usa su
propia cuenta `Account`), no para ocultarlo -- el comentario de cabecera de ese archivo explica el
bug y remite a este reporte.
