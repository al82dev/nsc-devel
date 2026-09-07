---
title: "NativeSharp: el lenguaje de programación"
subtitle: "La referencia absoluta para programadores de C# que quieren más"
language: "es"
edition: "Primera edición de revisión"
language-version: "NativeSharp 0.1"
compiler-version: "nsc 0.9.0-alpha, Stage 57"
verified-build: "0.9.0-alpha+build.20260906T204233Z.c7f7fa9da221841c"
platform: "x86_64-pc-windows-msvc"
date: "2026-09-06"
---

# NativeSharp: el lenguaje de programación

## La referencia absoluta

### Para programadores de C# que quieren más: código nativo, control de memoria, ABI y rendimiento

#### Primera edición en español

**Lenguaje NativeSharp 0.1**  
**Compilador `nsc` 0.9.0-alpha, Stage 57**  
**Plataforma de referencia: Windows x64, ABI de Microsoft, objetos COFF y ejecutables PE32+**

> **Contrato de esta edición.** Los 23 tutoriales y los 27 estudios de caso fueron compilados y
> ejecutados de nuevo el 6 de septiembre de 2026 con la build
> `0.9.0-alpha+build.20260906T204233Z.c7f7fa9da221841c`, tanto en `-O0` como en `-O2`. No se
> presupuso que los ejemplos antiguos seguían siendo correctos: las 50 versiones editoriales con
> formato Allman también se recompilaron y ejecutaron, con salida idéntica entre ambos niveles. La
> suite registrada por el repositorio contiene 297 pruebas.

---

## Derechos, propósito y procedencia editorial

Este manuscrito presenta NativeSharp al mundo. Es una obra original escrita a partir del
comportamiento comprobable del compilador, su biblioteca estándar, sus pruebas contractuales y sus
programas de ejemplo. Los libros de Bjarne Stroustrup, la especificación de C# y los textos de
programación y estructuras de datos citados en la bibliografía se usaron como referencias de
organización, profundidad, progresión pedagógica y disciplina expositiva. No se reproduce su texto
ni se intenta hacer pasar a NativeSharp por C++ o por C#.

NativeSharp tiene una sintaxis deliberadamente familiar para quien conoce C#, pero genera código
nativo, no depende del CLR, no emplea un recolector de basura y hace visibles la propiedad de los
recursos, su transferencia y su destrucción. Por esa razón, una explicación correcta no puede
limitarse a traducir documentación de C# y sustituir el nombre del lenguaje.

Esta edición describe un compilador alfa. La palabra *referencia* significa aquí “referencia
completa y verificable de la superficie implementada en la build indicada”, no una promesa de
compatibilidad perpetua con builds futuras. Cuando el lenguaje cambie, debe cambiar también el
número de edición, la matriz de validación y las afirmaciones normativas.

## A quién va dirigido

El libro admite tres recorridos.

1. Quien ya programa puede leer los capítulos 1 a 4, compilar el primer proyecto y saltar a las
   partes que necesite.
2. Quien quiere aprender el lenguaje con profundidad debe avanzar en orden hasta dominar clases,
   excepciones, ownership y genéricos antes de entrar a concurrencia o interoperabilidad.
3. Quien implementa herramientas, bibliotecas o bindings debe leer además las partes VII y VIII y
   consultar los apéndices como referencia cotidiana.

No se exige experiencia previa con compiladores. Sí se asume que el lector entiende qué es un
archivo, un proceso y una función. Los conceptos de ABI, COFF, PE, RAII, monomorfización y análisis
de escape se introducen antes de usarse como vocabulario normal.

## Convenciones de lectura

- `código` designa texto literal, identificadores, comandos o nombres de archivo.
- **Regla** introduce una condición que el compilador aplica.
- **Consecuencia** explica por qué esa condición importa al diseñar programas.
- **Límite actual** distingue una ausencia real de una omisión del libro.
- Los listados abreviados se usan sólo para aislar una idea. Los programas editoriales completos
  están incluidos, archivo por archivo, en la Parte XI.
- Todo código NativeSharp usa Allman, cuatro espacios, `Function()` sin espacio antes del
  paréntesis y un máximo estricto de 120 columnas.
- Una firma que contiene `owned` transfiere propiedad; una que contiene `borrowed` promete un
  préstamo. Esas palabras nunca deben borrarse mentalmente al leer una API.

---

# Contenido

## Parte I. El lenguaje en perspectiva

1. [Qué es NativeSharp](#1-qué-es-nativesharp)
2. [Del fuente al ejecutable](#2-del-fuente-al-ejecutable)
3. [El primer proyecto serio](#3-el-primer-proyecto-serio)
4. [Errores, diagnósticos y método de trabajo](#4-errores-diagnósticos-y-método-de-trabajo)

## Parte II. Facilidades fundamentales

5. [Tipos, valores y representación](#5-tipos-valores-y-representación)
6. [Declaraciones, alcance y tiempo de vida](#6-declaraciones-alcance-y-tiempo-de-vida)
7. [Expresiones y conversiones](#7-expresiones-y-conversiones)
8. [Control de flujo](#8-control-de-flujo)
9. [Funciones y paso de argumentos](#9-funciones-y-paso-de-argumentos)
10. [Namespaces, archivos y proyectos](#10-namespaces-archivos-y-proyectos)

## Parte III. Tipos definidos por el usuario

11. [Structs, enums, arrays fijos y unions](#11-structs-enums-arrays-fijos-y-unions)
12. [Clases, invariantes y destrucción](#12-clases-invariantes-y-destrucción)
13. [Herencia, interfaces y despacho](#13-herencia-interfaces-y-despacho)
14. [Propiedades, indexadores y operadores](#14-propiedades-indexadores-y-operadores)
15. [RTTI nativo: `is`, `as` y la raíz `Object`](#15-rtti-nativo-is-as-y-la-raíz-object)

## Parte IV. Memoria, recursos y errores

16. [Punteros, arrays, strings y vistas](#16-punteros-arrays-strings-y-vistas)
17. [Destrucción determinista con `raii`](#17-destrucción-determinista-con-raii)
18. [Ownership: `owned`, `borrowed` y `move`](#18-ownership-owned-borrowed-y-move)
19. [Excepciones, `finally` y `using`](#19-excepciones-finally-y-using)

## Parte V. Abstracción y programación genérica

20. [Delegados, lambdas, cierres y eventos](#20-delegados-lambdas-cierres-y-eventos)
21. [Genéricos, constraints y monomorfización](#21-genéricos-constraints-y-monomorfización)
22. [`var`, `foreach` y el protocolo enumerador](#22-var-foreach-y-el-protocolo-enumerador)
23. [Colecciones y algoritmos](#23-colecciones-y-algoritmos)

## Parte VI. Biblioteca de plataforma

24. [Texto, binarios y sistema de archivos](#24-texto-binarios-y-sistema-de-archivos)
25. [Tiempo, entorno, procesos y diagnóstico](#25-tiempo-entorno-procesos-y-diagnóstico)
26. [Concurrencia y sincronización](#26-concurrencia-y-sincronización)

## Parte VII. Código nativo e interoperabilidad

27. [ABI, `extern C` y callbacks](#27-abi-extern-c-y-callbacks)
28. [Bibliotecas estáticas, DLL, headers y metadatos](#28-bibliotecas-estáticas-dll-headers-y-metadatos)
29. [Layout explícito y contratos de frontera](#29-layout-explícito-y-contratos-de-frontera)

## Parte VIII. Referencia del lenguaje y de las herramientas

30. [Referencia léxica](#30-referencia-léxica)
31. [Operadores y precedencia](#31-operadores-y-precedencia)
32. [Referencia del compilador](#32-referencia-del-compilador)
33. [Diferencias esenciales frente a C# y C++](#33-diferencias-esenciales-frente-a-c-y-c)

## Parte IX. Curso completo para programadores de C#

34. [De C# administrado a NativeSharp nativo](#34-de-c-administrado-a-nativesharp-nativo)
35. [Los 23 tutoriales completos](#35-los-23-tutoriales-completos)

## Parte X. Algoritmos y estructuras de datos

36. [Cómo analizar un algoritmo](#36-cómo-analizar-un-algoritmo)
37. [Búsqueda y ordenamiento](#37-búsqueda-y-ordenamiento)
38. [Listas, pilas, colas y deques](#38-listas-pilas-colas-y-deques)
39. [Recursión y backtracking](#39-recursión-y-backtracking)
40. [Árboles, heaps y tries](#40-árboles-heaps-y-tries)
41. [Hashing y conjuntos](#41-hashing-y-conjuntos)
42. [Grafos](#42-grafos)
43. [Programación dinámica](#43-programación-dinámica)
44. [Texto y compresión](#44-texto-y-compresión)
45. [Algoritmos numéricos y álgebra](#45-algoritmos-numéricos-y-álgebra)

## Parte XI. Estudios de caso completos

46. [Cómo leer los proyectos editoriales](#46-cómo-leer-los-proyectos-editoriales)
47. [Los 27 programas completos](#47-los-27-programas-completos)

## Parte XII. Drozdek completo en NativeSharp

48. [Programación orientada a objetos para estructuras de datos](#48-programación-orientada-a-objetos-para-estructuras-de-datos)
49. [Análisis de complejidad](#49-análisis-de-complejidad)
50. [Listas enlazadas](#50-listas-enlazadas)
51. [Pilas, colas, colas de prioridad y deques](#51-pilas-colas-colas-de-prioridad-y-deques)
52. [Recursión y backtracking](#52-recursión-y-backtracking)
53. [Árboles binarios](#53-árboles-binarios)
54. [Árboles multiway](#54-árboles-multiway)
55. [Grafos](#55-grafos)
56. [Ordenamiento](#56-ordenamiento)
57. [Hashing](#57-hashing)
58. [Compresión de datos](#58-compresión-de-datos)
59. [Administración de memoria](#59-administración-de-memoria)
60. [Búsqueda de cadenas](#60-búsqueda-de-cadenas)
61. [Cálculo de O grande](#61-apéndice-drozdek-a-cálculo-de-o-grande)
62. [Catálogo de algoritmos de biblioteca](#62-apéndice-drozdek-b-catálogo-de-algoritmos-de-biblioteca)
63. [NP-completitud](#63-apéndice-drozdek-c-np-completitud)
64. [Matriz de equivalencia de listados](#64-matriz-de-equivalencia-de-los-listados-de-drozdek)
65. [Criterio de aceptación](#65-criterio-de-aceptación-de-la-cobertura-drozdek)

## Parte XIII. Live code al estilo Deitel, con filosofía NativeSharp

66. [La filosofía live-code de NativeSharp](#66-la-filosofía-live-code-de-nativesharp)
67. [Cuenta bancaria nativa](#67-aplicación-1-cuenta-bancaria-nativa)
68. [Control de flujo con políticas reales](#68-aplicación-2-control-de-flujo-con-políticas-reales)
69. [Azar reproducible, baraja y craps](#69-aplicación-3-azar-reproducible-baraja-y-craps)
70. [`GradeBook`, arrays y excepciones](#70-aplicación-4-gradebook-arrays-y-excepciones)
71. [Una clase de tiempo correcta](#71-aplicación-5-una-clase-de-tiempo-correcta)
72. [Nómina, facturas e interfaces](#72-aplicación-6-nómina-facturas-e-interfaces)
73. [Consultas funcionales sin ocultar costo](#73-aplicación-7-consultas-funcionales-sin-ocultar-costo)
74. [Archivo de crédito estable](#74-aplicación-8-archivo-de-crédito-con-formato-estable)
75. [Eventos y una interfaz nativa](#75-aplicación-9-eventos-y-una-interfaz-nativa)
76. [Hilos nativos y estado atómico](#76-aplicación-10-dos-hilos-nativos-y-estado-atómico)
77. [Doce talleres live-code](#77-doce-talleres-live-code)
78. [Correspondencia con Deitel](#78-correspondencia-editorial-con-la-progresión-deitel)
79. [Criterio de aceptación](#79-criterio-de-aceptación-de-un-ejemplo-live-code)

## Apéndices

- [A. Índice público de la biblioteca](#apéndice-a-índice-público-de-la-biblioteca)
- [B. Tabla de tipos y ABI](#apéndice-b-tabla-de-tipos-y-abi)
- [C. Lista de comprobación para publicar código](#apéndice-c-lista-de-comprobación-para-publicar-código)
- [D. Bibliografía editorial](#apéndice-d-bibliografía-editorial)

---

# Parte I. El lenguaje en perspectiva

## 1. Qué es NativeSharp

NativeSharp es un lenguaje de propósito general para producir programas nativos. Su superficie
sintáctica toma ideas conocidas de la familia C y, en particular, de C#: bloques con llaves,
declaraciones de clases y structs, propiedades, interfaces, delegados, excepciones, genéricos y
`foreach`. Esa familiaridad reduce el costo de lectura, pero no define su semántica completa.

La diferencia central aparece al preguntar quién posee la memoria. En un entorno administrado se
puede entregar una referencia y confiar en que el runtime descubrirá cuándo dejó de ser alcanzable.
NativeSharp no hace eso. Un objeto nativo debe destruirse de forma explícita o quedar ligado a un
alcance mediante `raii`; una API puede declarar que recibe o devuelve un dueño con `owned`, o que
sólo observa temporalmente un valor con `borrowed`; `move(x)` transfiere una propiedad y deja la
fuente consumida. El compilador comprueba reglas que previenen copias implícitas de dueños, uso
después de movimiento y escapes de préstamos.

La segunda diferencia es material: el destino de esta edición es `x86_64-pc-windows-msvc`.
`nsc` emite objetos COFF, bibliotecas estáticas, DLL PE32+, import libraries, headers para hosts C o
C++ y metadatos de biblioteca. La representación de structs y las llamadas de frontera se diseñan
contra la ABI de Microsoft x64. No hay una máquina virtual intermedia en la instalación del
usuario; LLVM es infraestructura de compilación, no un runtime que el programa distribuido deba
arrastrar.

La tercera diferencia es de filosofía. NativeSharp no intenta ocultar el costo de las abstracciones.
Una clase tiene identidad y vive por referencia. Un `struct` es un valor con layout. Un array es un
bloque nativo cuyo tiempo de vida debe razonarse. Los genéricos se monomorfizan: una instancia
cerrada produce código especializado. Un delegado lleva el destino necesario para invocar una
función o método, y un cierre captura estado real. Una excepción debe limpiar correctamente los
recursos en cada borde de alcance.

### 1.1 Lo que NativeSharp ofrece hoy

La build documentada incluye:

- tipos enteros con y sin signo, flotantes, `bool`, `char`, `string`, punteros, arrays y valores
  definidos por el usuario;
- funciones, sobrecargas, `ref`/`out`/`in`, namespaces, compilación separada y referencias entre
  proyectos;
- clases abstractas y selladas, herencia, interfaces, métodos virtuales, constructores,
  destructores, inicialización estática, propiedades, indexadores y operadores;
- RTTI nativo, `is`, `as`, `ToString`, `Equals` y `GetHashCode`, sin boxing;
- delegados, eventos, lambdas y cierres;
- excepciones nativas de Windows x64, `try`, `catch`, `finally`, `throw` y `using`;
- RAII y un sistema explícito de ownership y préstamos;
- genéricos AOT, constraints, `var`, `foreach` y colecciones genéricas;
- strings, spans, vistas, buffers binarios, archivos, directorios, tiempo, procesos, diagnóstico,
  threads y primitivas de sincronización;
- interoperabilidad C, callbacks, layouts de C, exportación de bibliotecas y metadatos consumibles.

### 1.2 Lo que no debe inferirse

Que una construcción exista en C# no implica que exista en NativeSharp. Esta edición no atribuye
al lenguaje CLR, garbage collection, boxing, `dynamic`, reflection administrada, LINQ, `async`/
`await`, expresiones de consulta, atributos generales, arrays multidimensionales administrados ni
compatibilidad binaria con ensamblados .NET. La forma `T?` tampoco es un tipo nullable: `?` pertenece
al operador condicional.

Cuando un diseño dependa de una característica no descrita aquí, la respuesta profesional no es
adivinar; es compilar un caso mínimo con la build objetivo y convertir el resultado en una prueba
de regresión.

## 2. Del fuente al ejecutable

Un archivo NativeSharp usa la extensión `.ns`. Un proyecto usa `.nsproj` y declara al menos un
nombre, los archivos que se compilan y las referencias necesarias. El proyecto mínimo de consola
es suficientemente pequeño para entenderlo completo:

```xml
<Project>
  <PropertyGroup>
    <Name>Ledger</Name>
  </PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
  </ItemGroup>
</Project>
```

La ruta de cada `ProjectReference` se resuelve desde el archivo de proyecto que la contiene. Las
referencias transitivas forman un grafo; un proyecto no debe depender circularmente de sí mismo.
El nombre de proyecto participa en identidades internas y en símbolos generados, por lo que no es
un rótulo decorativo.

El punto de entrada convencional es un método estático `Main` que devuelve `int`:

```csharp
using NativeSharp.Core;

namespace Ledger
{
    class Program
    {
        public static int Main()
        {
            Console.WriteLine("Ledger listo");
            return 0;
        }
    }
}
```

Compilar no es una sola operación conceptual. El frontend carga el grafo, tokeniza y parsea los
fuentes, resuelve nombres, construye miembros, comprueba herencia, interfaces, cuerpos, ownership,
construcción y destrucción, y produce representaciones intermedias. El backend baja esas
representaciones a LLVM, verifica el módulo, optimiza según el nivel pedido, emite COFF y enlaza un
producto PE.

La orden normal es:

```powershell
nsc --emit-project-oop-exe Ledger.nsproj -O2 -o Ledger.exe
```

Los niveles `-O0`, `-O1`, `-O2` y `-O3` son parte del contrato de compilación. Un programa correcto
debe conservar su semántica en todos; si sólo funciona en uno, hay un bug en el programa, en una
dependencia o en el compilador. Para desarrollo conviene comprobar al menos `-O0` y `-O2`, porque
esa pareja descubre tanto dependencias accidentales de inicialización como transformaciones
incorrectas del optimizador.

### 2.1 Las comprobaciones intermedias

`nsc` permite detenerse en capas concretas. `--check-project` valida la carga del proyecto;
`--check-project-method-bodies` llega al análisis de cuerpos; `--check-project-destruction` y
`--check-project-constructions` inspeccionan los modelos de tiempo de vida; `--verify-project-oop-llvm`
comprueba la salida LLVM. Los modos `--dump-*` imprimen modelos para diagnóstico. No son adornos
internos: constituyen una escalera para aislar si un error nace en sintaxis, resolución, semántica,
lowering, optimización, emisión o enlace.

### 2.2 Productos distintos

El mismo proyecto puede producir:

| Producto | Opción | Uso |
|---|---|---|
| Objeto COFF | `--emit-project-oop-obj` | Integración en un enlace controlado por otra herramienta |
| Ejecutable PE32+ | `--emit-project-oop-exe` | Aplicación nativa de consola o subsistema configurado |
| Biblioteca estática | `--emit-project-oop-lib` | Código enlazado dentro del consumidor |
| DLL e import library | `--emit-project-oop-dll` | Módulo cargable con exports estables |
| Header de host | `--emit-project-host-header` | Declaraciones C/C++ derivadas del modelo ABI |
| Metadatos `.nsmeta` | `--emit-project-library-metadata` | Superficie pública NativeSharp de una biblioteca |

## 3. El primer proyecto serio

Un ejemplo editorial debe demostrar más que una llamada a `WriteLine`. El primer dominio del libro
es un libro mayor que recibe movimientos, mantiene una invariante y produce un reporte. Aquí se
presenta su diseño; el estudio de racionales y el sistema de nómina de la Parte XI muestran la
implementación completa de ideas semejantes.

Una entidad `Account` puede guardar un identificador, un nombre y un saldo entero en centavos.
Usar centavos evita introducir error binario de punto flotante en una cantidad monetaria. El
constructor establece la invariante; los métodos `Deposit` y `Withdraw` son las únicas puertas de
mutación; una excepción hace imposible retirar un importe inválido sin obligar al llamador a
interpretar números mágicos.

```csharp
using NativeSharp.Core;

namespace Ledger.Domain
{
    public class InsufficientFundsException : Exception
    {
    }

    public class Account
    {
        private int id;
        private owned string name;
        private long balanceInCents;

        public Account(int id, owned string name, long openingBalance)
        {
            if (openingBalance < 0)
            {
                throw new ArgumentOutOfRangeException();
            }

            this.id = id;
            this.name = move(name);
            balanceInCents = openingBalance;
        }

        public int Id
        {
            get
            {
                return id;
            }
        }

        public long BalanceInCents
        {
            get
            {
                return balanceInCents;
            }
        }

        public void Deposit(long cents)
        {
            if (cents <= 0)
            {
                throw new ArgumentOutOfRangeException();
            }

            balanceInCents += cents;
        }

        public void Withdraw(long cents)
        {
            if (cents <= 0)
            {
                throw new ArgumentOutOfRangeException();
            }

            if (cents > balanceInCents)
            {
                throw new InsufficientFundsException();
            }

            balanceInCents -= cents;
        }

        ~Account()
        {
            delete name;
        }
    }
}
```

Este listado ya obliga a discutir decisiones reales. `Account` es clase porque tiene identidad y
estado mutable compartido; el saldo es `long` porque la suma puede superar el rango de `int`; el
nombre es `owned string` porque la cuenta debe conservarlo más allá de la llamada; el constructor
consume esa propiedad mediante `move`; el destructor libera el campo. Si el nombre fuera sólo
prestado, la clase podría retener una referencia después de que el llamador destruyera su buffer.

Un libro de lenguaje debe enseñar a hacer esas preguntas, no sólo a memorizar llaves y punto y
coma.

## 4. Errores, diagnósticos y método de trabajo

NativeSharp distingue errores de carga de proyecto, léxicos, sintácticos, de nombres, de tipos, de
ownership, de lowering y de enlace. El mensaje importa, pero la fase que lo produce suele importar
más. Un símbolo no resuelto en la etapa de enlace no se arregla cambiando un cast; un préstamo que
escapa no se arregla desactivando optimización.

El flujo recomendado para un problema nuevo es reproducible:

1. Reducir el caso sin borrar la propiedad que lo hace fallar.
2. Ejecutar `--check-project` y después la comprobación semántica más cercana.
3. Verificar LLVM antes de emitir el objeto.
4. Compilar en `-O0` y `-O2` con nombres de salida distintos.
5. Ejecutar ambos productos y comparar salida, código de retorno y efectos externos.
6. Convertir el caso mínimo en prueba si revela un defecto.

Las pruebas negativas son documentación ejecutable. Por ejemplo, leer una local escalar por un
camino donde nunca fue escrita es un error de asignación definida; copiar implícitamente un dueño
es un error de ownership; guardar una vista local en un objeto que sobrevivirá al buffer es un
escape de préstamo. Un manual serio muestra tanto el programa válido como el rechazo intencional.

---

# Parte II. Facilidades fundamentales

## 5. Tipos, valores y representación

NativeSharp separa tipos de valor, tipos de referencia y punteros. La separación determina copia,
identidad, layout, destrucción y ABI.

Los tipos integrales incorporados son `sbyte`, `byte`, `char`, `short`, `ushort`, `int`, `uint`,
`long`, `ulong`, `nint` y `nuint`. `sbyte` es el entero de 8 bits con signo; `byte` y `char` son de 8
bits sin signo. `char` no es UTF-16 ni `wchar_t`. `nint` y `nuint` tienen el ancho del puntero en el
destino, que en esta edición es 64 bits. `float` usa 32 bits y `double`, 64. `bool` ocupa un byte.

Los literales enteros se tipan por valor y sufijo conforme al modelo implementado; las operaciones
mixtas pasan por conversiones numéricas que conservan el signo y el ancho del tipo común. No debe
suponerse que una expresión que mezcla `ulong` y un literal negativo tiene sentido: hay que escoger
el tipo de la operación de forma explícita y comprobar límites.

Un `struct` contiene sus campos directamente y se copia como valor. Una clase se maneja mediante
una referencia nativa y tiene identidad. Un enum usa por omisión representación de 32 bits, aunque
la declaración puede fijar un tipo subyacente integral admitido. Un puntero ocupa 8 bytes. Un array
dinámico y un `string` son objetos de runtime con longitud explícita; no son arrays de C desnudos.

### 5.1 `default(T)` e inicialización

`default(T)` produce el valor predeterminado del tipo. Las locales sin inicializador reciben una
representación cero en backend, pero la asignación definida sigue rechazando la lectura prematura
de escalares, punteros, referencias y delegados. Cero físico no significa permiso semántico para
leer antes de escribir.

Los agregados admiten inicialización por lista:

```csharp
struct Pixel
{
    public byte r;
    public byte g;
    public byte b;
    public byte a;
}

Pixel opaqueRed = { 255, 0, 0, 255 };
Pixel transparent = default(Pixel);
```

Una lista parcial conserva cero en los campos no alcanzados. Las listas anidadas siguen el layout
anidado. Esta forma es especialmente importante al declarar estructuras que cruzarán una frontera
C.

## 6. Declaraciones, alcance y tiempo de vida

Una declaración introduce un nombre en un alcance: namespace, tipo, miembro, parámetro, bloque o
variable de control. La resolución considera el namespace actual, directivas `using`, miembros,
herencia y referencias de proyecto. La accesibilidad puede ser `public`, `internal`, `protected` o
`private`.

El tiempo de vida no es sinónimo del alcance léxico, pero el alcance puede gobernarlo. Una local de
valor existe en su slot mientras se ejecuta el bloque. Una referencia ordinaria puede apuntar a un
objeto cuya destrucción sea manual. Una local `raii` se convierte en dueña ligada al alcance y se
destruye en todas las salidas normales o excepcionales. Un préstamo sólo es válido mientras vive
su origen y no puede escaparse a un consumidor más largo.

`const` expresa un valor conocido y no modificable. `readonly` restringe la escritura de un campo
después de la construcción. `static` asocia el miembro al tipo, no a una instancia. La
inicialización estática es determinista y su guard es seguro ante concurrencia y reentrancia en la
implementación actual.

### 6.1 Asignación definida

El análisis es sensible al control de flujo. En el siguiente método, `answer` está asignada en cada
rama que llega al `return`:

```csharp
static int Select(bool fast)
{
    int answer;

    if (fast)
    {
        answer = 17;
    }
    else
    {
        answer = 29;
    }

    return answer;
}
```

Quitar el `else` vuelve inválida la lectura final. En cambio, una variable pasada como `out` puede
estar sin inicializar en el llamador porque el contrato obliga al callee a escribirla antes de
volver.

## 7. Expresiones y conversiones

Las expresiones producen valores, designan almacenamiento o causan efectos. NativeSharp incluye
acceso a miembros, llamadas, indexación, construcción, operadores unarios y binarios, asignaciones
simples y compuestas, casts, pruebas RTTI, `sizeof`, `default`, `stackalloc`, `move` y el operador
condicional.

Las conversiones implícitas deben ser seguras dentro de las reglas del lenguaje; las conversiones
que pueden perder información o reinterpretar representación requieren un cast o un operador de
conversión declarado. Los operadores `implicit` y `explicit` permiten que un tipo definido por el
usuario participe en el sistema sin exponer su representación.

```csharp
public struct Meters
{
    public double Value;

    public static explicit operator Meters(double value)
    {
        Meters result = { value };
        return result;
    }

    public static implicit operator double(Meters value)
    {
        return value.Value;
    }
}
```

El cortocircuito de `&&` y `||` es observable: el operando derecho no se evalúa cuando el izquierdo
decide el resultado. El operador `condición ? verdadero : falso` evalúa una sola rama y puede
aparecer dentro de argumentos o retornos. Las ramas deben converger en un tipo que el compilador
pueda representar sin violar ownership.

Las asignaciones compuestas `+=`, `-=`, `*=`, `/=`, `%=` y las variantes bit a bit y de shift
equivalen conceptualmente a leer, operar y escribir, pero el destino se evalúa una sola vez. Esa
distinción importa para un indexador o una expresión con efectos.

## 8. Control de flujo

NativeSharp ofrece bloques, `if`/`else`, `while`, `do`/`while`, `for`, `foreach`, `switch`, `break`,
`continue`, `return`, `throw` y los statements de manejo de excepciones y recursos.

`do`/`while` ejecuta el cuerpo al menos una vez:

```csharp
int attempts = 0;
bool ready;

do
{
    attempts += 1;
    ready = Probe(attempts);
}
while (!ready && attempts < 5);
```

`switch` evalúa una vez su selector integral o enum y lo compara con etiquetas constantes. Las
secciones no tienen caída implícita: agrupar varias etiquetas expresa los casos compartidos sin
permitir que una sección terminada siga accidentalmente en la siguiente.

```csharp
enum JobState
{
    Created = 0,
    Running = 1,
    Completed = 2,
    Failed = 3
}

static int Severity(JobState state)
{
    switch (state)
    {
        case JobState.Created:
            return 0;

        case JobState.Running:
            return 1;

        case JobState.Completed:
            return 0;

        case JobState.Failed:
            return 3;

        default:
            return 4;
    }
}
```

`default` es opcional, único y puede escribirse en una posición no final. Las etiquetas deben ser
únicas después de su evaluación constante. `break` sale del `switch`; `continue` pertenece al loop
envolvente. Un `return` o un `throw` ejecutan antes las limpiezas exigidas por los alcances activos.

## 9. Funciones y paso de argumentos

Una firma está formada por nombre, parámetros, modificadores relevantes y tipo de retorno. Las
sobrecargas se resuelven por las conversiones disponibles y por los argumentos de tipo cuando hay
genéricos. Un método de instancia recibe además `this`; un método `static` no depende de una
instancia.

El paso ordinario copia un valor o una referencia según el tipo. Los modificadores contextuales
`ref`, `out` e `in` introducen una referencia a almacenamiento:

```csharp
struct Rect
{
    public int width;
    public int height;
}

static void Increment(ref int value)
{
    value += 1;
}

static void Measure(out Rect result)
{
    result.width = 40;
    result.height = 30;
}

static int Area(in Rect value)
{
    return value.width * value.height;
}
```

- `ref` exige que el argumento ya tenga valor y permite leerlo y escribirlo.
- `out` permite un argumento aún no asignado y obliga al método a producir valor.
- `in` pasa por referencia para evitar copia, pero el callee no puede modificarlo.

La forma debe coincidir también en el sitio de llamada: `Increment(ref counter)`,
`Measure(out rect)`, `Area(in rect)`. No es una pista opcional; hace visible la dirección del flujo.

`owned` y `borrowed` responden una pregunta distinta. `ref`/`out`/`in` describen cómo se accede al
slot durante la llamada; ownership describe quién debe destruir el recurso y si puede conservarse.

## 10. Namespaces, archivos y proyectos

Un namespace organiza nombres y evita colisiones; no crea por sí mismo una unidad de enlace. Un
tipo puede dividirse mediante declaraciones parciales soportadas por el modelo del proyecto, y
varios archivos pueden contribuir al mismo namespace. `using X.Y;` hace accesibles nombres de un
namespace sin copiar ni importar dinámicamente su código.

La unidad real de compilación moderna es el grafo de `.nsproj`. Cada proyecto declara fuentes,
referencias a otros proyectos y, para productos precompilados, referencias de biblioteca y
metadatos. La resolución debe conservar la identidad calificada de tipos y miembros a través del
grafo.

Para una solución mediana conviene separar por responsabilidad:

```text
src/
  Domain/Domain.nsproj
  Storage/Storage.nsproj       -> Domain
  Cli/Cli.nsproj               -> Domain, Storage, NativeSharp.Core
tests/
  DomainTests/DomainTests.nsproj
```

Esta organización impide que la interfaz de dominio dependa accidentalmente de la consola o del
sistema de archivos. El mismo principio se aprecia en la propia biblioteca: `NativeSharp.Core` no
depende de `Collections.Generic`, porque hacerlo crearía un ciclo; los algoritmos que necesitan
delegados genéricos viven con las colecciones.

---

# Parte III. Tipos definidos por el usuario

## 11. Structs, enums, arrays fijos y unions

Un `struct` representa un valor agregado. Sus campos forman el layout; copiarlo copia sus datos.
Puede tener constructores, métodos, propiedades, indexadores y operadores. No necesita una caja
para `ToString`, `Equals` o `GetHashCode`: el compilador sintetiza el soporte estático necesario sin
cambiar el layout.

Un `enum` define nombres para valores integrales. Es preferible a un conjunto de números sueltos
porque hace visible el dominio y permite al compilador rechazar mezclas indebidas. Los operadores
bit a bit son útiles para enums de flags cuando el tipo subyacente y las conversiones están
definidos de forma intencional.

La interoperabilidad añade agregados con layout de C:

```csharp
public union Payload
{
    public int signedValue;
    public uint unsignedValue;
    public float realValue;
}

public align(16) struct Packet
{
    public int kind;
    public byte[16] bytes;
}
```

`union` coloca sus miembros en el mismo offset y toma el tamaño del miembro mayor, redondeado por
alineación. `fixed T field[N]` inserta los elementos en el propio agregado; no guarda un puntero a
otro bloque. Los modificadores de layout `pack(N)`, `align(N)` y `at(N)` permiten expresar contratos
foráneos, pero también pueden producir accesos menos eficientes o layouts incorrectos si se usan
por estética. Deben copiar una especificación ABI medida, no una intuición.

## 12. Clases, invariantes y destrucción

Una clase combina estado, operaciones e identidad. El constructor debe dejar cada instancia en un
estado válido o no producirla. Los campos privados reducen el número de lugares capaces de romper
la invariante. Los métodos públicos expresan operaciones de dominio, no sólo setters mecánicos.

NativeSharp permite constructores de instancia, miembros estáticos y destructores. El destructor
libera los recursos que la instancia posee: arrays, strings owned, handles encapsulados u otros
objetos. Destruir un recurso que no se posee es un error de diseño; omitir un recurso poseído es una
fuga.

```csharp
public sealed class IntBuffer
{
    private owned int[] values;
    private int count;

    public IntBuffer(int capacity)
    {
        if (capacity < 1)
        {
            capacity = 1;
        }

        values = new int[capacity];
        count = 0;
    }

    public void Add(int value)
    {
        if (count == values.Length)
        {
            throw new InvalidOperationException();
        }

        values[count] = value;
        count += 1;
    }

    ~IntBuffer()
    {
        delete[] values;
    }
}
```

`sealed` declara que la clase no puede derivarse. `abstract` impide construir una clase incompleta y
permite declarar operaciones que las derivadas deben implementar. Un método virtual tiene un slot
de despacho; uno no virtual se resuelve estáticamente.

## 13. Herencia, interfaces y despacho

La herencia de clases modela una relación de sustitución, no una estrategia para reutilizar unas
cuantas líneas. Si `HourlyEmployee` deriva de `Employee`, cualquier consumidor de `Employee` debe
poder usarla sin conocer su clase concreta y sin ver rota la invariante del contrato base.

Una interfaz declara capacidad. Una clase puede implementar varias interfaces aunque sólo tenga
una base de clase. El despacho de interfaz usa metadatos nativos y ajustes de receptor calculados
por el compilador. `virtual`, `abstract` y `override` hacen explícito el papel de cada método.

```csharp
public interface IWritable
{
    void WriteTo(TextWriter target);
}

public abstract class Report : IWritable
{
    public abstract void WriteTo(TextWriter target);

    public virtual owned string Kind()
    {
        return String.Copy("report");
    }
}
```

Los destructores participan en la jerarquía. Borrar una referencia base debe destruir la instancia
dinámica correcta. Este punto es semántica de recursos, no sólo RTTI: equivocarse deja campos de la
derivada sin limpiar o ejecuta un destructor incompatible.

## 14. Propiedades, indexadores y operadores

Una propiedad presenta una operación como acceso a un valor. Puede tener `get`, `set` o ambos. No
debe ocultar una operación sorprendentemente cara o fallable sin que su nombre o documentación lo
advierta.

Un indexador permite que una abstracción defina `container[index]`. El getter y setter deben
compartir reglas de rango. Las colecciones estándar lanzan excepciones apropiadas cuando un índice
no pertenece al intervalo válido.

Los operadores son funciones estáticas asociadas al tipo. Son valiosos cuando preservan la
intuición algebraica: suma de racionales, producto de matrices, comparación de dinero. Son
perjudiciales cuando el símbolo no anticipa el costo o el efecto.

El estudio de números racionales implementa normalización, máximo común divisor, aritmética,
comparaciones y conversión a texto. El laboratorio de polinomios implementa suma, resta,
multiplicación, derivación y evaluación de Horner. El de vectores y matrices muestra que la
sobrecarga no es un juguete de diez líneas: define un pequeño dominio algebraico completo y lo
somete a destrucción explícita.

## 15. RTTI nativo: `is`, `as` y la raíz `Object`

`x is T` pregunta si la instancia dinámica es compatible con `T`. `x as T` intenta la conversión y
produce una referencia nula cuando no es posible. La comprobación usa descriptores nativos de
clase e interfaz; no invoca reflection del CLR.

Las clases participan en una raíz `NativeSharp.Core.Object` con operaciones virtuales fundamentales.
Los structs reciben operaciones equivalentes resueltas estáticamente. No existe boxing: asignar un
entero a `Object` no crea una caja oculta. Esta decisión conserva layout, costo y ownership
predecibles, a cambio de que los contenedores heterogéneos deban diseñarse explícitamente.

Un cast RTTI correcto no sustituye una interfaz. Si un algoritmo pregunta repetidamente por cinco
clases concretas, probablemente falta una operación polimórfica. RTTI es adecuado en fronteras,
serialización controlada, herramientas y migraciones; el despacho virtual suele ser más estable en
el centro de un modelo.

---

# Parte IV. Memoria, recursos y errores

## 16. Punteros, arrays, strings y vistas

Un puntero `T*` contiene una dirección. La aritmética, desreferencia y conversión de punteros deben
reservarse para código de bajo nivel y fronteras ABI. `stackalloc` obtiene almacenamiento ligado al
frame actual; devolver un puntero o vista a ese almacenamiento dejaría una dirección inválida.

Un array dinámico `T[]` conoce su longitud y se crea con `new T[count]`. Se libera con `delete[]` si
el dueño no está ligado por otra abstracción. El acceso usa bounds checks conforme a la operación
generada. Un array de referencias no implica propiedad automática de cada objeto almacenado: el
contenedor y sus elementos son responsabilidades distintas.

`string` tiene longitud explícita y datos en bytes; no promete terminador NUL. `CString` existe para
materializar una representación terminada al cruzar una API C. Tratar el buffer de un string como
si siempre tuviera NUL es un bug de frontera.

`Span<T>` y `ReadOnlySpan<T>` describen memoria contigua sin apropiarse de ella. `StringView` hace
lo propio para texto. Una vista evita copiar, pero no extiende la vida del origen. La regla esencial
es sencilla de enunciar y difícil de aplicar sin disciplina: ninguna vista debe sobrevivir al
almacenamiento que observa.

La biblioteca ofrece `ByteBuffer`, `BinaryWriter` y el `borrowed struct BinaryReader`. Este último
es un ejemplo deliberado: el lector mantiene posición sobre bytes ajenos; marcarlo prestado permite
que la API exprese que no debe escapar del buffer.

## 17. Destrucción determinista con `raii`

`raii` liga un dueño local al alcance. Al salir por el final del bloque, por `return` o por una
excepción, el compilador programa la destrucción correspondiente. La ventaja no es ahorrar la
palabra `delete`; es hacer que todos los caminos compartan una garantía.

```csharp
using NativeSharp.Core.IO;

static void ProduceReport(string path)
{
    raii FileTextWriter writer = new FileTextWriter(path);
    writer.WriteLine("NativeSharp build report");
    writer.WriteLine("status: verified");
}
```

Si la segunda escritura lanza, `writer` sigue limpiándose. En un método con cinco retornos
tempranos, no hace falta duplicar cinco secuencias de liberación. Éste es el mismo principio que
hace viable la programación nativa moderna: el recurso se representa mediante un objeto cuyo
destructor encarna la operación de cierre.

No todo tipo admite `raii` de la misma manera. La build documentada lo aplica a referencias de
clase o interfaz y al string nativo en las formas soportadas; un array requiere su disciplina
específica. El compilador rechaza usos que no tengan una operación de limpieza coherente.

## 18. Ownership: `owned`, `borrowed` y `move`

Ownership responde “quién debe destruir”. `owned T` es una obligación transferible. `borrowed T`
es acceso temporal sin esa obligación. `move(x)` cambia el dueño y consume la fuente.

```csharp
static owned Resource Create(int value)
{
    raii Resource result = new Resource(value);
    return move(result);
}

static int Inspect(borrowed Resource value)
{
    return value.Read();
}

static int Consume(owned Resource value)
{
    return value.Read();
}
```

Después de `Take`, el sobre ya no posee el mensaje. El compilador impide tratar la vieja fuente
como si aún fuera válida. Una copia ordinaria de referencia puede existir cuando no duplica la
obligación de destrucción; una copia de dueño está prohibida porque produciría doble liberación.

El análisis sigue ramas, loops, retornos y excepciones. Un dueño debe consumirse exactamente donde
corresponde. Un préstamo no puede almacenarse en un campo o devolverse cuando la vida del receptor
superaría la del origen. Las colecciones genéricas usan estas reglas de manera visible: `Add(owned
T value)` consume el elemento, `Pop()` devuelve `owned T`, y un enumerador expone una observación
compatible con la vida de su colección.

### 18.1 Diseño de firmas

Una buena firma elimina dudas:

- `void Render(borrowed Scene scene)` observa una escena durante la llamada.
- `void Submit(owned Job job)` se convierte en responsable del trabajo.
- `owned Buffer Finish()` entrega un buffer que el llamador deberá destruir.
- `borrowed Buffer Current()` presta almacenamiento que sigue perteneciendo al objeto.

La ausencia deliberada de GC vuelve estas decisiones parte de la API pública. No deben relegarse a
comentarios informales.

## 19. Excepciones, `finally` y `using`

Las excepciones separan el camino normal del fallo cuando el llamador no puede resolver localmente
la condición. NativeSharp implementa `throw`, `try`, catches tipados y `finally` sobre el modelo de
excepciones nativas de Windows x64. El unwinding ejecuta limpiezas de scopes y destructores.

```csharp
try
{
    ParseAndStore(input);
}
catch (ArgumentException error)
{
    Console.ErrorWriteLine("entrada inválida");
}
finally
{
    metrics.Flush();
}
```

Los catches se prueban en orden de compatibilidad. Una cláusula demasiado general antes que una
específica puede hacerla inalcanzable conceptualmente. `finally` se ejecuta tanto en éxito como en
fallo y no debe convertirse en un segundo centro de lógica de negocio.

`using` expresa un patrón de recurso descartable en el que la limpieza debe suceder al abandonar
el statement. `raii` es la herramienta general de vida ligada al alcance; `using` hace explícita
una región de uso. En ambos casos, la corrección exige que el backend modele también los bordes
excepcionales, no sólo el camino feliz.

Una excepción NativeSharp no debe escapar a través de un callback C y desenrollar frames foráneos.
La frontera de callback instalada por el compilador aplica un guard y convierte esa violación en
un fallo controlado conforme al contrato de interop.

---

# Parte V. Abstracción y programación genérica

## 20. Delegados, lambdas, cierres y eventos

Un delegado es un tipo de referencia invocable con una firma concreta. Permite pasar comportamiento
sin reducirlo a un puntero de función sin tipo. Puede referirse a una función estática, a un método
de instancia con receptor o a una lambda. La compatibilidad se decide por la firma, no por el nombre
informal del método.

```csharp
public delegate int Comparison<T>(T left, T right);

static int CompareByPriority(Job left, Job right)
{
    if (left.Priority < right.Priority)
    {
        return -1;
    }

    if (left.Priority > right.Priority)
    {
        return 1;
    }

    return 0;
}
```

Una lambda introduce una función en el punto de uso. Cuando captura una local, el compilador crea
un cierre que conserva el estado necesario. Ese entorno tiene representación y tiempo de vida;
capturar no es magia ni necesariamente gratis. La interacción con ownership debe leerse igual que
en cualquier objeto: un cierre no puede prolongar silenciosamente un préstamo inválido.

Los eventos encapsulan una colección de suscriptores y restringen quién puede emitir. Son adecuados
cuando varios observadores reaccionan a una transición y el productor no debe conocerlos. No
conviene usar un evento como sustituto de una llamada cuyo resultado o fallo forme parte del flujo
principal.

```csharp
public delegate void ProgressChanged(int completed, int total);

public class Importer
{
    public event ProgressChanged Progress;

    private void Notify(int completed, int total)
    {
        Progress(completed, total);
    }
}
```

Al diseñar callbacks que cruzan C, use `extern C delegate`. Esa forma representa un puntero de
función de la ABI C y no debe confundirse con un delegado NativeSharp que transporte receptor o
cierre.

## 21. Genéricos, constraints y monomorfización

Un genérico define una familia de tipos o métodos. `List<int>` y `List<Job>` son instancias cerradas
distintas. El compilador monomorfiza: comprueba e instancia el cuerpo para los argumentos concretos
y genera código especializado. Esto evita una máquina virtual genérica en runtime y permite layout
directo, a cambio de que muchas instancias puedan aumentar el tamaño del binario.

```csharp
public interface IWeighted
{
    int Weight();
}

public class Box<T> where T : class, IWeighted
{
    private T value;

    public Box(T value)
    {
        this.value = value;
    }

    public int TotalWeight()
    {
        return value.Weight();
    }
}
```

Los constraints se escriben con `where`. `class` exige un tipo de referencia; una clase base exige
esa clase o una derivada; una interfaz exige implementación. Un mismo parámetro puede combinar
`class` e interfaces. Los constraints forman parte de la identidad semántica exportada y se
preservan a través de `ProjectReference` y metadatos de biblioteca.

La resolución de sobrecargas genéricas utiliza constraints para descartar candidatos inválidos,
pero no debe depender de un miembro que el constraint no promete. Si un algoritmo necesita
`Weight()`, declare `where T : IWeighted`; que los tipos usados hoy casualmente tengan ese método no
es una interfaz estable.

`default(T)` permite inicializar slots de una colección sin saber si `T` será valor o referencia.
Las operaciones `owned T` y `move` funcionan también dentro de código genérico, y son precisamente
donde una implementación descuidada produciría dobles liberaciones. Por eso las colecciones se
validan con argumentos de valor y de referencia.

## 22. `var`, `foreach` y el protocolo enumerador

`var` pide al compilador inferir el tipo estático del inicializador. No crea tipado dinámico. La
variable sigue teniendo un tipo concreto, conocido al compilar, y todas las operaciones posteriores
se comprueban contra él.

`foreach` tiene dos modos. Para un array, el compilador recorre índices y longitud. Para una
colección, usa un protocolo estructural: `GetEnumerator()`, `MoveNext()` y la propiedad `Current`.
No exige una jerarquía administrada `IEnumerable<T>`.

```csharp
raii List<int> values = new List<int>();
values.Add(7);
values.Add(11);
values.Add(13);

int total = 0;
foreach (int value in values)
{
    total += value;
}
```

El enumerador es un recurso temporal. El desugaring conserva la semántica de `break`, `continue`,
retornos y limpieza. En modo array, `continue` todavía debe ejecutar el incremento del índice;
este caso aparentemente pequeño merece una prueba porque un lowering ingenuo produciría un loop
infinito.

No modifique estructuralmente una colección mientras la enumera salvo que la API documente esa
operación. Aunque el compilador pueda expresar el código, el enumerador podría guardar posición o
referencias que una inserción invalide.

## 23. Colecciones y algoritmos

`NativeSharp.Core.Collections.Generic` incluye secuencias, colecciones asociativas, conjuntos,
colas, pilas, heaps, listas ordenadas, enumeradores, comparadores, delegates y algoritmos sobre
spans.

| Abstracción | Operación dominante | Costo esperado |
|---|---|---|
| `List<T>` | acceso por índice, append | O(1) amortizado para append |
| `LinkedList<T>` | insertar/quitar con nodo | O(1) con el nodo; O(n) para buscar |
| `Stack<T>` | `Push`/`Pop` | O(1) amortizado |
| `Queue<T>` | `Enqueue`/`Dequeue` | O(1) amortizado |
| `Dictionary<TKey,TValue>` | lookup por hash | O(1) esperado; depende del hash |
| `HashSet<T>` | pertenencia y álgebra de conjuntos | O(1) esperado por operación básica |
| `SortedDictionary`/`SortedSet` | orden total | O(log n) esperado |
| `PriorityQueue<TElement,TPriority>` | extraer prioridad extrema | O(log n) |

El costo no puede separarse de ownership. `List<T>.Add(owned T value)` consume el elemento. Para un
tipo de referencia, construir el valor y olvidarse de `move` puede ser rechazado o dejar ambigua la
responsabilidad. Para un tipo valor trivial, la misma API se especializa sin necesidad de heap por
elemento.

Los algoritmos libres trabajan con `Span<T>` o `ReadOnlySpan<T>`: `Find`, `FindIf`, `AnyOf`,
`AllOf`, `Min`, `Max`, `Reverse`, `RemoveIf` y `Sort`. Esta separación evita obligar a que todo
algoritmo conozca el tipo concreto del contenedor. Un span proporciona el par esencial: memoria
contigua y longitud.

Los estudios de caso no se limitan a llamar las colecciones estándar. Implementan listas, pilas,
colas, deques, árboles, tries, tablas hash y grafos para mostrar invariantes, complejidad y
destrucción desde dentro. Después, el lector puede comparar esa implementación pedagógica con la
biblioteca productiva.

---

# Parte VI. Biblioteca de plataforma

## 24. Texto, binarios y sistema de archivos

`NativeSharp.Core.String` ofrece una superficie extensa de búsqueda, partición, recorte, casing,
relleno, predicados, conversión y formato posicional. Las operaciones que producen un string nuevo
deben leerse como asignación de recurso. `StringBuilder` reduce las copias repetidas al construir
texto incremental.

`TextWriter` define salida textual con implementaciones para consola, acumulación en memoria y
archivo. Programar contra `TextWriter` permite que un reporte cambie de destino sin reescribir el
algoritmo. `Console` es la fachada mínima para stdout y stderr.

`File` cubre existencia, borrado, lectura y escritura de texto y bytes, tamaño, copia y movimiento.
`Directory` crea, borra, mueve y enumera archivos o subdirectorios mediante un protocolo de conteo
y buffer destino. `Path` combina, separa componentes y obtiene una ruta absoluta.

```csharp
int count = Directory.GetFileCount(source);
string[] files = new string[count];
int written = Directory.GetFiles(source, files);

int index = 0;
while (index < written)
{
    Console.WriteLine(files[index]);
    index += 1;
}

delete[] files;
```

La separación conteo/relleno hace visible la asignación y evita que el runtime invente una
colección administrada. Entre ambas llamadas el directorio puede cambiar; una aplicación robusta
debe aceptar que el número escrito sea menor o que necesite reintentar según su política.

`ByteBuffer`, `BinaryWriter` y `BinaryReader` sirven para formatos binarios. El writer emite enteros
de anchos explícitos y ASCII fijo; el reader devuelve `Optional<T>` cuando no hay suficientes
bytes. Un protocolo de archivo debe fijar endianness, tamaños y validación, no depender del layout
accidental de una clase.

## 25. Tiempo, entorno, procesos y diagnóstico

`TimeSpan` representa una duración en ticks de 100 ns. `DateTime` representa ticks desde
0001-01-01 UTC en el contrato actual. `Stopwatch` usa un contador de alta resolución y distingue
tiempo transcurrido de fecha de calendario. Mezclar esos dominios produce errores incluso cuando
ambos caben en `long`.

`Environment` expone contador de milisegundos, número de procesadores, variables de entorno,
directorio actual, argumentos de comando y terminación del proceso. Las variables y argumentos
entran desde APIs Windows que usan representaciones distintas; el runtime materializa strings
NativeSharp con longitud explícita.

`Process` ejecuta un programa con línea de comandos y ruta de salida, y consulta información del
proceso actual: identificador, working set, tiempos de kernel y usuario, y prioridad. `SystemInfo`
describe la máquina: tamaño de página, arquitectura, granularidad de asignación y memoria física.
`Debug` consulta el debugger, emite texto de diagnóstico y captura el número disponible de frames.

El principio de diseño es separar métrica, mecanismo y política. `Stopwatch` mide; el algoritmo
decide si un resultado es lento. `Process.Run` lanza; la aplicación decide reintentos, timeouts y
qué hacer con un exit code no cero.

## 26. Concurrencia y sincronización

`Thread` representa un hilo del sistema operativo. Recibe un `ThreadStart`, puede `Join`, `Detach`
y dormir. Su disciplina RAII evita abandonar por accidente un handle activo: la política exacta de
destrucción debe entenderse antes de usar detach.

`AtomicInt32` y `AtomicInt64` proporcionan carga, incremento, decremento, suma, intercambio y
compare-exchange con operaciones atómicas reales. Un atomic evita data races sobre ese valor; no
convierte automáticamente en segura una invariante que involucra cinco campos.

`Mutex` ofrece exclusión mutua basada en handle. `ScopedLock` convierte `Lock`/`Unlock` en una vida
ligada al alcance. `Monitor` combina lock y condición con `Wait`, `Pulse` y `PulseAll`. `Semaphore`
limita el número de participantes concurrentes. `Event` representa una señal de reset manual o
automático.

```csharp
raii Mutex gate = new Mutex();

static void Update(Mutex gate, SharedState state)
{
    raii ScopedLock guard = new ScopedLock(gate);
    state.completed += 1;
}
```

La espera sobre condición siempre debe reevaluar el predicado:

```csharp
monitor.Enter();
try
{
    while (!queue.HasWork())
    {
        monitor.Wait(-1);
    }

    queue.TakeWork();
}
finally
{
    monitor.Exit();
}
```

Un timeout de `-1` significa espera infinita en las APIs documentadas; valores no negativos son
timeouts reales. La concurrencia correcta exige además una política de shutdown, ownership de los
datos compartidos y pruebas repetidas. Que un programa pase una vez no demuestra ausencia de
carreras.

---

# Parte VII. Código nativo e interoperabilidad

## 27. ABI, `extern C` y callbacks

Interop comienza con representación. La ABI de referencia es Microsoft Windows x64: registros de
argumentos, retorno, alineación, clasificación de agregados, símbolos C y tablas de excepciones
siguen ese contrato. Una firma que “se parece” a C puede seguir siendo binariamente incompatible si
un campo, un signo o una convención difiere.

Las importaciones se agrupan por biblioteca:

```csharp
public class Kernel32
{
    extern C from "kernel32.dll"
    {
        public uint GetCurrentProcessId();
    }
}
```

La identidad de una importación es el triple biblioteca, símbolo externo y firma. Dos DLL pueden
exportar un símbolo con el mismo nombre sin convertirse en la misma función.

Los callbacks C se declaran con `extern C delegate`:

```csharp
extern C delegate int Transform(int value);

public class HostApi
{
    public static export extern C int ApplyTwice(Transform transform, int value)
    {
        return transform(transform(value));
    }
}
```

Un puntero crudo sólo puede convertirse a una firma callable mediante reinterpretación explícita.
El compilador acepta parámetros y retornos de function pointer en las formas verificadas. Una
excepción NativeSharp no puede cruzar el callback hacia frames foráneos: el guard de frontera
impide un unwind no contratado.

## 28. Bibliotecas estáticas, DLL, headers y metadatos

Una biblioteca NativeSharp tiene dos consumidores posibles. Otro proyecto NativeSharp necesita la
superficie semántica: tipos, miembros, constraints, ownership e identidad. Un host C o C++ necesita
una superficie ABI: nombres exportados, structs compatibles y prototipos.

Para el primer caso, `nsc` produce `.nsmeta` y puede verificarlo antes de publicar. Una referencia
precompilada asocia la biblioteca física con sus metadatos. Para el segundo, el compilador genera
un header determinista desde el mismo modelo que usa el lowering. Mantener ese header a mano
duplicaría la verdad y permitiría drift.

El flujo de publicación recomendado es:

1. Compilar la biblioteca en `-O2` y conservar símbolos según la política de distribución.
2. Emitir y verificar `.nsmeta`.
3. Emitir el header de host.
4. Compilar al menos un host C y uno C++ contra ese header.
5. Probar DLL e import library o la biblioteca estática según el producto.
6. Comparar layouts con un compilador C/C++ independiente.
7. Ejecutar los consumidores en Windows real.

El host C# usa P/Invoke contra la ABI C exportada; no consume objetos NativeSharp como si fueran
objetos CLR. Los handles opacos y funciones de creación/destrucción suelen ser la frontera más
estable para recursos con identidad.

## 29. Layout explícito y contratos de frontera

La tabla de tamaños del apéndice es el punto de partida, no el final. Un struct agrega padding entre
campos y al final. `pack(N)` limita la alineación efectiva de cada campo; `align(N)` eleva la
alineación del agregado; `at(N)` fija un offset. Una union superpone campos. Un array fijo se
encuentra inline.

Todo tipo expuesto debe responder:

- ¿Cuál es su tamaño y alineación en ambos compiladores?
- ¿Cada campo tiene el mismo offset?
- ¿El signo y ancho de cada entero coinciden?
- ¿El host pasa el agregado por valor o por puntero?
- ¿Quién asigna y quién destruye la memoria?
- ¿Puede una excepción atravesar la llamada?
- ¿El texto lleva longitud, NUL, UTF-8 o UTF-16?

No exporte una clase directamente como si fuera un struct C. Una clase contiene identidad y
metadatos nativos. Exponga un handle opaco o un struct de transferencia estable. No exporte un
`string` NativeSharp donde el host espera `char*`; use una función que reciba puntero y longitud o
materialice `CString` con ownership documentado.

---

# Parte VIII. Referencia del lenguaje y de las herramientas

## 30. Referencia léxica

El fuente se compone de identificadores, palabras reservadas o contextuales, literales, signos de
puntuación, operadores, espacios y comentarios. Los identificadores distinguen mayúsculas de
minúsculas. Los comentarios de línea comienzan con `//`; los de bloque usan `/* ... */`.

### 30.1 Palabras reservadas reconocidas por el lexer

```text
void bool sbyte byte char short ushort int uint long ulong nint nuint float double string
const static readonly public internal protected private
if else while do for switch case break continue return
try catch finally throw
struct class interface enum namespace using
new delete stackalloc sizeof default
extern export
true false null this base
virtual override abstract sealed
get set operator implicit explicit is as
delegate event raii move owned borrowed var foreach in
```

`C`, `ref`, `out`, `where`, `from`, `pack`, `align`, `at` y otros vocablos de construcciones concretas
se interpretan contextualmente donde la gramática los permite. Un nombre contextual fuera de esa
posición puede seguir siendo identificador; no conviene explotarlo para crear código confuso.

### 30.2 Literales

El lenguaje reconoce literales enteros, flotantes, de carácter y de string, además de `true`,
`false` y `null`. Los escapes deben representar valores compatibles con el ancho de `char` y la
codificación del string. Los enteros fuera de rango se rechazan; no se permite que un overflow de
parsing cambie silenciosamente el valor.

### 30.3 Puntuación

Los delimitadores son `(` `)` `{` `}` `[` `]`; los separadores principales, `;` `,` `:` y `.`.
`->` y `=>` participan en formas específicas de acceso o función; `?` sólo pertenece al operador
condicional en la superficie actual.

## 31. Operadores y precedencia

De mayor a menor precedencia conceptual:

| Grupo | Operadores |
|---|---|
| Primarios | llamada `()`, indexación `[]`, acceso `.`, postfix `++` `--` |
| Unarios | `+` `-` `!` `~`, prefix `++` `--`, cast, `sizeof`, `new`, `delete`, `move` |
| Multiplicativos | `*` `/` `%` |
| Aditivos | `+` `-` |
| Shifts | `<<` `>>` |
| Relacionales | `<` `<=` `>` `>=`, `is`, `as` |
| Igualdad | `==` `!=` |
| AND bit a bit | `&` |
| XOR | `^` |
| OR bit a bit | `|` |
| AND condicional | `&&` |
| OR condicional | `||` |
| Condicional | `?:` |
| Asignación | `=` `+=` `-=` `*=` `/=` `%=` `&=` `|=` `^=` `<<=` `>>=` |

Use paréntesis cuando la intención no sea inmediata, incluso si conoce la tabla. La precedencia no
debe convertirse en una prueba para el lector. Los operandos de `&&`, `||` y `?:` tienen evaluación
condicional; la mayoría de operadores binarios evalúa ambos.

La división entera trunca conforme al signo de los operandos y rechaza división por cero en el
contrato generado. Los shifts exigen un operando integral y una cuenta válida conforme a las reglas
del tipo. Los operadores bit a bit sobre enums requieren compatibilidad con su representación.

## 32. Referencia del compilador

### 32.1 Comandos de proyecto

```text
--check-project
--dump-project
--check-project-graph
--dump-project-graph
--check-project-syntax
--dump-project-compilation
--check-project-generics
--dump-project-generics
--check-project-names
--dump-project-names
--check-project-members
--dump-project-members
--check-project-inheritance
--dump-project-inheritance
--check-project-interfaces
--dump-project-interfaces
--check-project-generic-constraints
--check-project-method-bodies
--dump-project-method-bodies
--check-project-virtual-dispatch
--dump-project-virtual-dispatch
--check-project-destruction
--dump-project-destruction
--check-project-constructions
--dump-project-constructions
--check-project-layouts
--dump-project-layouts
--check-project-oop-hir
--dump-project-oop-hir
--check-project-oop-mir
--dump-project-oop-mir
--verify-project-oop-llvm
--dump-project-oop-llvm
```

### 32.2 Emisión

```text
--emit-project-oop-obj <project> [-O0|-O1|-O2|-O3] -o <file.obj>
--emit-project-oop-exe <project> [-O0|-O1|-O2|-O3] -o <file.exe>
--emit-project-oop-lib <project> [-O0|-O1|-O2|-O3] -o <file.lib>
--emit-project-oop-dll <project> [-O0|-O1|-O2|-O3] -o <file.dll>
--emit-project-host-header <project> -o <file.h>
--emit-project-library-metadata <project> -o <file.nsmeta>
--verify-library-metadata <file.nsmeta>
```

Los modos de un solo archivo (`--check`, `--dump-tokens`, `--dump-ast`, `--dump-sema`, HIR, MIR,
CFG, dominancia, SSA, optimización, LLVM, objeto, asm y ejecutable) son útiles para casos mínimos y
para desarrollar el compilador. La tubería OOP de proyecto es la autoridad para aplicaciones y
bibliotecas modernas.

`--print-target`, `--print-data-layout`, `--print-linker`, `--print-codegen` y
`--print-llvm-pipeline` documentan la configuración efectiva. `--version` debe guardarse junto a una
certificación para que “funciona” tenga un referente concreto.

## 33. Diferencias esenciales frente a C# y C++

| Tema | NativeSharp | C# típico | C++ típico |
|---|---|---|---|
| Ejecución | binario nativo PE/COFF | IL y CLR, salvo AOT específico | binario nativo |
| Memoria de objetos | explícita, RAII y ownership | GC | RAII, smart pointers y manual |
| Sintaxis de objetos | cercana a C# | nativa de C# | nativa de C++ |
| Boxing | no | sí | no como mecanismo del lenguaje |
| Genéricos | AOT por monomorfización | reificación CLR | templates por instanciación |
| RTTI | descriptores nativos | metadata CLR | RTTI C++ |
| String | longitud + bytes, no NUL garantizado | UTF-16 administrado | depende del tipo/biblioteca |
| `char` | 8 bits sin signo | 16 bits UTF-16 | depende de implementación, normalmente 8 bits |
| Herencia de clases | simple; interfaces múltiples | simple; interfaces múltiples | herencia múltiple disponible |
| Interop C | parte central del lenguaje/toolchain | P/Invoke/interop | `extern "C"` y ABI |
| Plataforma de esta edición | Windows x64 MSVC ABI | multiplataforma según runtime | depende del toolchain |

La semejanza superficial con C# es una ventaja de legibilidad, no licencia para trasladar hábitos
administrados. Guardar una referencia no mantiene vivo al objeto por alcance global. Entregar un
string a C requiere adaptar representación. Una colección de referencias debe decidir quién
destruye sus elementos.

La cercanía semántica con C++ en código nativo tampoco vuelve idénticos ambos lenguajes. NativeSharp
no adopta el sistema de headers, templates, sobrecarga libre, herencia múltiple ni todas las formas
de conversión de C++. Su sistema de ownership utiliza palabras y análisis propios.

---

# Parte IX. Curso completo para programadores de C#

## 34. De C# administrado a NativeSharp nativo

El lector principal de esta edición es quien ya sabe programar en C# y quiere ir más allá del
entorno administrado: controlar el layout de los datos, producir una DLL sin CLR, hablar con una
API C, decidir cuándo se destruye un recurso y conservar abstracciones modernas sin pagar un
runtime pesado. Esa experiencia previa es una ventaja, pero también crea falsas expectativas si no
se cambia el modelo mental desde el principio.

La sintaxis permite empezar rápido. `namespace`, `class`, `struct`, `interface`, propiedades,
indexadores, operadores, delegados, eventos, excepciones, genéricos, `var` y `foreach` resultan
reconocibles. La semántica de memoria es distinta. En C#, una referencia administrada contribuye a
la alcanzabilidad del objeto; en NativeSharp, una referencia no sustituye al dueño. Copiar la
dirección no copia el derecho ni la obligación de destruir.

### 34.1 Mapa de conceptos

| Si en C# piensa en... | En NativeSharp debe pensar en... |
|---|---|
| objeto alcanzable por GC | objeto nativo con dueño y punto de destrucción |
| `IDisposable` para recursos especiales | destructor y RAII como disciplina general |
| referencia ordinaria que prolonga vida | alias que puede sobrevivir indebidamente al dueño |
| `string` UTF-16 administrado | objeto de bytes con longitud explícita y sin NUL garantizado |
| generic reificado por el CLR | instancia cerrada monomorfizada AOT |
| delegate administrado | par invocable nativo; quizá receptor o cierre |
| reflection/metadata del ensamblado | descriptor RTTI limitado o `.nsmeta` de compilación |
| P/Invoke añadido en la orilla | ABI C como superficie de primera clase |
| Task y thread pool | threads del sistema y sincronización explícita |
| array cuyos elementos referencia sigue el GC | contenedor y elementos con ownership independiente |

### 34.2 El cambio más importante: una referencia no mantiene vivo al objeto

Considere una fábrica. En C#, devolver una referencia local es normal: el GC observa la referencia
del llamador. En NativeSharp, la fábrica debe entregar ownership:

```csharp
static owned Resource Create(int value)
{
    raii Resource result = new Resource(value);
    return move(result);
}
```

`raii` protege el camino excepcional durante la construcción. `owned` en el retorno anuncia la
obligación que recibe el llamador. `move(result)` consume el dueño local para que el cleanup del
scope no destruya el objeto que acaba de salir.

Observar no debe transferir:

```csharp
static int Inspect(borrowed Resource resource)
{
    return resource.Read();
}
```

Consumir sí:

```csharp
static int Consume(owned Resource resource)
{
    return resource.Read();
}
```

Después de `Consume(move(value))`, `value` está consumida. El compilador usa esa información para
rechazar uso después de movimiento y copias implícitas de dueño.

### 34.3 Tipos de valor sin boxing

Un `struct` NativeSharp es layout, no una clase pequeña. Copiarlo copia los campos. Pasarlo por
valor puede consumir registros o memoria según la ABI. `in` evita una copia y prohíbe escribir;
`ref` permite lectura y escritura del slot; `out` obliga al callee a asignarlo.

No existe la conversión administrada `object value = 42`. La raíz `Object` sirve a clases y a las
operaciones fundamentales del sistema de tipos, pero NativeSharp no fabrica una caja oculta para
un entero o un struct. Un diseño heterogéneo debe usar una jerarquía, una union etiquetada o una
representación propia.

### 34.4 Excepciones sin GC

El patrón `try`/`catch`/`finally` es familiar, pero su costo de corrección cambia. Cuando una
excepción desenrolla la pila, cada dueño de alcance debe limpiarse. El compilador genera funclets y
tablas de excepciones compatibles con Windows x64. Un destructor incorrecto puede convertir un
error recuperable en fuga o doble liberación.

`finally` sigue siendo adecuado para una acción que debe ocurrir en ambos caminos. Para un recurso,
prefiera encapsular la limpieza en el destructor y declarar la local `raii`; así la obligación no
se duplica cada vez que se agrega un retorno.

### 34.5 Genéricos sin CLR

`List<int>` y `List<Job>` generan instancias cerradas. El compilador conoce tamaño, alineación,
operaciones y constraints de cada `T`. Esto permite código especializado y evita boxing, pero puede
aumentar el tamaño del binario. Un constraint `where T : IComparable` no es documentación opcional:
es la prueba que autoriza al cuerpo genérico a usar esa interfaz.

La biblioteca utiliza `owned T` en inserciones y retornos. El mismo template debe funcionar cuando
`T` es un entero trivial y cuando es una referencia que necesita destrucción. Por eso las pruebas
de una colección genérica deben cubrir ambos.

### 34.6 Texto y frontera nativa

Un string de C# puede entregarse a P/Invoke mediante marshaling. NativeSharp hace visible la
representación. Su `string` tiene longitud explícita y bytes; una API que requiere `char*`
terminado en cero necesita `CString` o una adaptación equivalente. Una API UTF-16 necesita una
conversión real, no un cast.

Esta visibilidad evita costos invisibles, pero traslada al diseño tres preguntas: codificación,
longitud y dueño del buffer.

### 34.7 Concurrencia sin `Task`

La biblioteca actual expone threads de sistema, atomics, mutexes, monitores, semáforos y eventos.
No atribuya al lenguaje `async`/`await`, scheduler, cancellation tokens ni thread pool por conocerlos
de .NET. Una aplicación concurrente debe definir explícitamente quién posee el estado compartido,
cómo se detiene cada hilo y qué primitiva protege cada invariante.

### 34.8 Ruta recomendada

Quien llega desde C# puede estudiar los tutoriales en cuatro tramos:

1. Tutoriales 1-6: representación, valores, control, funciones, arrays y structs.
2. Tutoriales 7-11: modelo de objetos, interfaces, operadores y RTTI.
3. Tutoriales 12-19: ownership, genéricos, colecciones, delegates y excepciones.
4. Tutoriales 20-23: algoritmos y estructuras que integran todo lo anterior.

Después debe leer la Parte X antes de modificar los estudios de caso. Saber escribir un loop no
equivale a poder defender la complejidad o la corrección de un algoritmo.

## 35. Los 23 tutoriales completos

Los tutoriales existentes fueron recompilados con la build de esta edición en `-O0` y `-O2` y
ejecutados en Windows. Ambos niveles produjeron la misma salida y el mismo retorno en los 23 casos.
Sus comentarios históricos contenían afirmaciones superadas por el compilador actual -por ejemplo,
que no existían el operador condicional, `switch`, `do`/`while`, `ref`/`out` o los constraints-. Por
esa razón, los listados editoriales eliminan esos comentarios y colocan la explicación vigente
fuera del código. El programa se conserva semánticamente intacto y recibe formato Allman.

<!-- GENERATED:TUTORIALS -->

---

# Parte X. Algoritmos y estructuras de datos

## 36. Cómo analizar un algoritmo

Un algoritmo no queda explicado al mostrar su fuente. Hay que especificar el problema, las
precondiciones, el resultado, la invariante que se conserva, el argumento de terminación, el costo
y el comportamiento en los bordes. Esta parte utiliza las implementaciones completas de la Parte
XI para realizar ese análisis.

### 36.1 Tamaño de entrada y operación dominante

La variable `n` debe significar algo concreto: cantidad de elementos, vértices, aristas, caracteres
o capacidad. Después se elige una operación que represente trabajo: comparación, movimiento,
acceso a arista, asignación de celda o llamada recursiva.

Decir que un algoritmo “tiene un loop” no determina su costo. Dos loops consecutivos de `n` pasos
son O(n); uno anidado dentro de otro puede ser O(n²); un loop que divide el espacio a la mitad es
O(log n). La estructura del progreso importa más que el número de palabras `for` o `while`.

### 36.2 O, theta y omega

- O(g(n)) da una cota superior asintótica.
- Ω(g(n)) da una cota inferior.
- Θ(g(n)) indica que ambas cotas coinciden hasta factores constantes.

Para comparar implementaciones prácticas se necesitan también constantes, localidad de memoria,
asignaciones, branch prediction y tamaño del dato. Insertion sort, Θ(n²) en general, puede superar
a merge sort sobre un arreglo diminuto o casi ordenado porque su cuerpo es pequeño y no requiere
buffer auxiliar.

### 36.3 Mejor, promedio y peor caso

La búsqueda lineal encuentra el primer elemento en Θ(1), pero necesita Θ(n) comparaciones si el
elemento no está. La búsqueda binaria mantiene Θ(log n) incluso en su peor caso, pero exige datos
ordenados y acceso aleatorio. La precondición puede costar más que la consulta si sólo se buscará
una vez.

El promedio sólo es significativo cuando se declara una distribución. “O(1) promedio” para una
tabla hash supone una función razonable y una carga controlada; no promete que una entrada hostil
no concentre todas las claves en un bucket.

### 36.4 Complejidad amortizada

Una lista dinámica duplica capacidad cuando se llena. Una inserción que crece cuesta Θ(n), pero
muchas inserciones desde vacío suman Θ(n); el costo amortizado por append es O(1). El argumento se
apoya en que cada elemento se copia sólo cuando la capacidad cruza potencias geométricas.

Amortizado no significa que cada operación sea rápida. Un sistema con límite de latencia debe
considerar el peor pico, reservar capacidad o usar una estructura diferente.

### 36.5 Memoria y ownership como parte del costo

En NativeSharp, el análisis incluye:

- almacenamiento inline frente a objetos por nodo;
- capacidad reservada pero no usada;
- buffers auxiliares;
- profundidad máxima de stack;
- número de construcciones, movimientos y destrucciones;
- retención accidental por enlaces o colecciones.

Un merge sort usa Θ(n) memoria auxiliar; quicksort in-place usa stack esperado O(log n), pero una
mala partición puede llevarlo a O(n). Una lista enlazada evita mover elementos al insertar, pero
asigna un nodo por elemento y pierde localidad.

### 36.6 Invariantes y prueba de corrección

Una invariante es una afirmación verdadera antes y después de cada iteración u operación. Para una
búsqueda binaria: si la clave existe, está dentro del intervalo activo. Para un heap: cada padre
tiene prioridad no peor que sus hijos. Para BFS: al extraer un vértice, su distancia es el mínimo
número de aristas desde el origen.

La prueba sigue tres pasos:

1. Inicialización: la invariante es cierta antes de comenzar.
2. Conservación: una iteración válida la mantiene.
3. Terminación: cuando el progreso termina, la invariante implica el resultado.

Este esquema descubre bugs que una demostración con tres datos no revela.

## 37. Búsqueda y ordenamiento

### 37.1 Búsqueda lineal

La búsqueda lineal inspecciona elementos hasta encontrar la clave o agotar el rango. No necesita
orden y funciona sobre cualquier secuencia. Su peor costo es Θ(n) y su memoria adicional, Θ(1).
Es la opción correcta para datos pequeños, una sola consulta o una fuente que sólo puede recorrerse
secuencialmente.

La invariante al entrar en la iteración `i` es: ninguna posición anterior contiene la primera
coincidencia buscada. Al terminar con `i == n`, la ausencia queda demostrada para todo el rango.

### 37.2 Búsqueda binaria

Mantiene un intervalo medio abierto o cerrado y descarta la mitad incompatible después de cada
comparación. Su costo es Θ(log n), pero la colección debe estar ordenada con el mismo comparador
usado por la búsqueda.

Los errores clásicos son mezclar intervalos `[low, high]` y `[low, high)`, calcular el medio con una
suma que puede desbordar y no hacer progreso cuando quedan uno o dos elementos. Use
`low + (high - low) / 2` y escriba la invariante junto al diseño.

La búsqueda exponencial primero duplica un límite hasta encerrar la clave y después aplica búsqueda
binaria. Es útil cuando no se conoce de antemano el tamaño relevante o la coincidencia suele estar
cerca del comienzo. Conserva O(log i) para una coincidencia en la posición `i`.

### 37.3 Criterios de un ordenamiento

Un análisis editorial debe registrar:

| Algoritmo | Mejor | Promedio/peor | Memoria | Estable |
|---|---:|---:|---:|---|
| Burbuja optimizada | O(n) | O(n²) | O(1) | sí |
| Selección | Θ(n²) | Θ(n²) | O(1) | normalmente no |
| Inserción | O(n) | O(n²) | O(1) | sí |
| Shell | depende de gaps | depende de gaps | O(1) | no |
| Merge sort | Θ(n log n) | Θ(n log n) | Θ(n) | sí si el merge lo preserva |
| Quicksort | O(n log n) | O(n²) | stack | no por omisión |
| Heap sort | Θ(n log n) | Θ(n log n) | O(1) | no |

Estable significa que elementos equivalentes conservan su orden relativo. Esto importa al ordenar
primero por nombre y después por departamento, o cuando el orden previo transporta significado.

### 37.4 Separar algoritmo y comparación

El laboratorio de ordenamiento usa `StudentSorter` y `StudentComparer`. Esa separación permite
ejecutar cada algoritmo por identificador y por calificación sin duplicar siete implementaciones.
También permite medir comparaciones en un único punto.

Una comparación debe imponer un orden coherente: antisimetría, transitividad y resultado cero para
equivalencia. Un comparador inconsistente puede hacer que quicksort o un árbol ordenado produzcan
resultados que parecen aleatorios sin violar memoria.

### 37.5 Cuándo no ordenar

Ordenar cuesta O(n log n) y cambia el orden. Si se necesita sólo el mínimo, un recorrido O(n) es
mejor. Si se necesitan los `k` mejores, un heap de tamaño `k` puede costar O(n log k). Si las claves
son enteros en rango pequeño, un conteo puede superar algoritmos de comparación. La estructura del
problema debe decidir la herramienta.

## 38. Listas, pilas, colas y deques

### 38.1 Lista simplemente enlazada

Cada nodo contiene valor y siguiente. La lista conserva cabeza, cola y cantidad. Insertar al frente
es O(1); insertar al final también lo es si se mantiene cola; buscar es O(n). El costo oculto es una
asignación por nodo y una lectura indirecta que perjudica caché.

Invariantes:

- cantidad cero implica cabeza y cola nulas;
- cantidad uno implica cabeza igual a cola;
- la cola tiene siguiente nulo;
- la cantidad coincide con los nodos alcanzables;
- cada nodo y cada valor owned se destruyen exactamente una vez.

### 38.2 Lista doble

Agregar enlace anterior permite eliminación local y recorrido inverso, pero cada mutación debe
mantener dos direcciones. Si `a.next == b`, debe cumplirse `b.previous == a`. Los casos de cabeza,
cola y lista unitaria no son excepciones administrativas: son donde la invariante cambia de forma.

### 38.3 Lista circular

En una lista circular no existe un enlace nulo al final; la cola vuelve a la cabeza. Esto modela
turnos y round-robin, pero hace que un recorrido “hasta null” nunca termine. Toda iteración necesita
un contador, un nodo centinela o la condición de volver al inicio. Antes de destruir, el ciclo debe
romperse o recorrerse con una estrategia que no dependa de null.

### 38.4 Pila

La pila es LIFO. `Push`, `Pop` y `Peek` deben ser O(1) amortizado sobre array dinámico. Sus usos
incluyen evaluación de expresiones, DFS iterativo, undo y simulación de llamadas.

Una pila vacía no tiene elemento superior. La API debe elegir entre excepción y `TryPop`; devolver
`default(T)` sin señal confunde un valor legítimo con ausencia.

### 38.5 Cola y deque

Una cola FIFO basada en array circular mantiene `head`, `count` y capacidad. La posición lógica
`i` se traduce a `(head + i) % capacity`. Al crecer, los elementos se copian en orden lógico a un
buffer lineal nuevo y `head` vuelve a cero.

Un deque agrega inserción y extracción en ambos extremos. Puede implementar cola, pila, ventana
deslizante y análisis de palíndromos. Su complejidad es O(1) amortizado por extremo, pero la
aritmética modular y el estado lleno/vacío necesitan pruebas combinatorias.

## 39. Recursión y backtracking

Una función recursiva necesita caso base y progreso hacia él. Cada llamada crea un frame nativo con
parámetros, locales, dirección de retorno y obligaciones de limpieza. La elegancia sintáctica no
elimina el consumo de stack.

### 39.1 Recursión de cola y no de cola

En recursión de cola, la llamada recursiva es la última operación. Una implementación puede
transformarla en loop, pero este libro no presupone que toda build de NativeSharp lo haga. En
recursión no terminal, el frame conserva trabajo: sumar, combinar árboles o restaurar estado.

Fibonacci ingenuo ejecuta un árbol exponencial de llamadas porque recalcula los mismos valores.
Memoización o programación dinámica lo convierte en O(n). La lección no es “la recursión es lenta”,
sino “la descomposición crea subproblemas repetidos”.

### 39.2 Euclides

El máximo común divisor usa `gcd(a, b) = gcd(b, a % b)` hasta `b == 0`. El residuo reduce el segundo
argumento y garantiza terminación para enteros no negativos. El estudio de racionales depende de
esta operación para conservar la representación canónica.

### 39.3 Backtracking

Backtracking elige una alternativa, actualiza estado, explora y deshace si falla. La corrección
depende de restaurar exactamente el estado previo, incluidos dueños temporales. Un recurso creado
en una rama debe destruirse al abandonarla; RAII reduce fugas, pero no corrige una marca lógica que
el algoritmo olvidó retirar.

Un parser de descenso recursivo es un caso estructurado: cada función reconoce una producción,
consume tokens y devuelve una construcción o error. El evaluador de expresiones incluido usa dos
pilas y precedencia, una alternativa iterativa que permite comparar diseños.

## 40. Árboles, heaps y tries

### 40.1 Árbol binario de búsqueda

Para cada nodo, todas las claves del subárbol izquierdo son menores y las del derecho, mayores bajo
el comparador. Buscar, insertar y eliminar cuestan O(h), donde `h` es altura. Un árbol degenerado
puede tener `h = n`; el orden de inserción importa.

Eliminar distingue:

1. hoja: quitar el enlace;
2. un hijo: enlazar el padre directamente con el hijo;
3. dos hijos: sustituir por sucesor o predecesor y eliminar ese nodo en su posición original.

El tercer caso debe transferir valor sin crear dos dueños del mismo objeto.

### 40.2 AVL

Un AVL limita la diferencia de alturas de ambos hijos a -1, 0 o 1. Después de mutar, actualiza
altura y aplica una de cuatro formas: izquierda-izquierda, derecha-derecha, izquierda-derecha o
derecha-izquierda. Las dos últimas son rotaciones dobles.

Buscar, insertar y eliminar quedan O(log n). El costo adicional es almacenar altura y rotar. Una
prueba sólida valida orden, altura calculada y balance en cada nodo después de secuencias variadas,
no sólo que `Find` encuentre los valores.

### 40.3 Heap binario

Un heap es un árbol completo almacenado en array. Para índice `i`, los hijos suelen estar en
`2i + 1` y `2i + 2`; el padre, en `(i - 1) / 2`. La invariante de prioridad es local y no implica
que el array completo esté ordenado.

Insertar agrega al final y hace sift-up. Extraer sustituye la raíz por el último elemento y hace
sift-down. Ambas operaciones cuestan O(log n); consultar la raíz, O(1). Heap sort construye un heap
y extrae dentro del mismo array para obtener Θ(n log n) y O(1) memoria auxiliar.

### 40.4 Trie

Un trie distribuye caracteres por niveles y comparte prefijos. Insertar o buscar una palabra cuesta
O(m), donde `m` es su longitud, independientemente del número total de palabras, aunque el factor de
memoria puede ser alto. Un nodo necesita distinguir “fin de palabra” de “prefijo existente”.

La elección de representación de hijos -array por alfabeto, lista o mapa- intercambia velocidad,
memoria y soporte de caracteres. Como `char` NativeSharp es un byte, un trie de Unicode debe operar
sobre code points decodificados o documentar que trabaja sobre bytes UTF-8.

## 41. Hashing y conjuntos

Una tabla hash transforma la clave en un entero, normaliza ese valor a un bucket y resuelve
colisiones. El hash no demuestra igualdad: claves distintas pueden compartirlo. La consulta siempre
debe comparar la clave dentro del bucket.

### 41.1 Encadenamiento y carga

El estudio de símbolos usa encadenamiento. Cada bucket contiene una lista de entradas. Con `n`
elementos y `m` buckets, el factor de carga es `α = n / m`; bajo distribución razonable, una
operación cuesta O(1 + α). Si todos colisionan, degenera a O(n).

Rehashing asigna una tabla mayor y vuelve a insertar cada entrada usando el nuevo módulo. Cambiar
sólo el array sin redistribuir rompe todas las búsquedas. La operación cuesta O(n), amortizada sobre
las inserciones que provocaron el crecimiento.

### 41.2 Funciones hash

Un hash debe ser determinista dentro del contrato necesario y distribuir las claves observadas. No
use una dirección de objeto como hash persistente. Si `Equals(a, b)` es verdadero, sus hashes deben
coincidir. La inversa no es obligatoria.

`Dictionary<TKey,TValue>` y `HashSet<T>` reciben funciones hash en sus constructores porque la
superficie actual no debe adivinar cómo hashear cualquier `T`. Esta explicitud es preferible a un
default incorrecto.

### 41.3 Set

Un set guarda pertenencia, no multiplicidad. Unión, intersección, diferencia y diferencia simétrica
pueden implementarse mediante consultas y altas/bajas. La complejidad depende de tamaños y de la
tabla subyacente. Para preservar ownership, una operación destructiva debe definir si mueve
elementos o sólo consulta al otro conjunto.

## 42. Grafos

Un grafo `G = (V, E)` contiene vértices y aristas. Puede ser dirigido o no, ponderado o no. Antes de
elegir algoritmo hay que fijar esas propiedades.

### 42.1 Representación

Una matriz de adyacencia usa Θ(V²) memoria y consulta una arista en O(1). Una lista de adyacencia
usa Θ(V + E) y recorre vecinos en proporción al grado. La matriz resulta clara y eficiente en
grafos densos o pequeños; la lista domina en grafos dispersos grandes.

### 42.2 BFS

BFS usa una cola. Marca el origen, encola y después descubre vecinos no visitados. En un grafo no
ponderado, la primera vez que descubre un vértice obtiene el menor número de aristas desde el
origen. Guardar predecesor permite reconstruir el camino.

Con lista de adyacencia cuesta Θ(V + E); con matriz, Θ(V²). Marcar al encolar evita insertar el mismo
vértice muchas veces.

### 42.3 DFS

DFS usa recursión o una pila explícita. Explora una rama antes de volver. Sirve para componentes,
ciclos, orden topológico y clasificación de aristas. La versión iterativa hace visible el stack y
evita agotar el stack de llamadas en un grafo profundo, aunque debe decidir el orden de inserción de
vecinos para reproducir el recorrido recursivo.

### 42.4 Dijkstra

Dijkstra requiere pesos no negativos. Mantiene distancias tentativas y fija repetidamente el
vértice no visitado con menor distancia. Relajar `(u, v)` prueba si pasar por `u` mejora a `v`.

La implementación con selección lineal cuesta O(V²); con heap y lista de adyacencia,
O((V + E) log V). Para el programa editorial pequeño se elige claridad y matriz. La precondición de
pesos no negativos no es negociable: una arista negativa puede mejorar un vértice después de
haberlo fijado.

### 42.5 Orden topológico

Sólo existe para un grafo dirigido acíclico. El algoritmo de Kahn calcula grado de entrada,
encola ceros y elimina conceptualmente sus aristas. Si procesa menos de `V` vértices, queda un ciclo.
Puede haber varios órdenes válidos; una prueba no debe exigir uno específico salvo que la política
de desempate lo defina.

### 42.6 Kruskal y union-find

Kruskal ordena aristas por peso y acepta una cuando une componentes distintas. Union-find responde
si dos vértices ya comparten representante y los une. Path compression y union by rank producen
costo amortizado casi constante.

En un grafo conectado, aceptar `V - 1` aristas produce un árbol de expansión mínimo. En uno
desconectado produce un bosque mínimo; la API debe decidir si eso es resultado o error.

## 43. Programación dinámica

La programación dinámica se aplica cuando hay subproblemas superpuestos y estructura óptima. Se
define un estado, una recurrencia, casos base y orden de llenado. La tabla no es el algoritmo hasta
que esas cuatro piezas están justificadas.

### 43.1 Mochila 0/1

`dp[i, w]` representa el mejor valor usando los primeros `i` artículos con capacidad `w`. Si el
artículo no cabe, se hereda la fila anterior; si cabe, se compara omitirlo con tomarlo y sumar el
estado de capacidad restante.

Tiempo y memoria son O(nW), pseudopolinomiales porque dependen del valor numérico de la capacidad.
Una tabla bidimensional permite reconstruir la selección. Una optimización a una fila reduce
memoria a O(W), pero debe recorrer capacidad hacia abajo para no reutilizar el mismo artículo.

### 43.2 Subsecuencia común más larga

`dp[i, j]` guarda la longitud para prefijos. Si los caracteres finales coinciden, se suma uno a la
diagonal; si no, se toma el máximo de quitar uno de los extremos. Cuesta O(mn) tiempo y memoria en
la forma que reconstruye.

Puede haber varias subsecuencias óptimas. Una política de desempate cambia el texto devuelto sin
cambiar la longitud. Las pruebas deben aceptar el contrato escogido.

### 43.3 Distancia de Levenshtein

Los bordes convierten vacío en un prefijo mediante inserciones o eliminaciones. Cada celda toma el
mínimo de borrar, insertar y sustituir; si los caracteres coinciden, la sustitución cuesta cero.
La forma estándar usa O(mn) tiempo y memoria, reducible a O(min(m, n)) cuando sólo se necesita la
distancia.

LCS y Levenshtein parecen tablas similares, pero responden preguntas distintas. Copiar una
recurrencia por parecido visual es una fuente habitual de bugs.

## 44. Texto y compresión

Los algoritmos de texto deben decidir si procesan bytes, caracteres o code points. Como `char` es
de 8 bits y el string contiene bytes, indexar un string UTF-8 encuentra unidades de codificación,
no necesariamente caracteres humanos. `Utf8` y vistas permiten construir capas explícitas.

### 44.1 Coincidencia y edición

Una búsqueda ingenua de patrón cuesta O(nm) en peor caso. Algoritmos como KMP preprocesan el patrón
para evitar reconsiderar prefijos y alcanzan O(n + m). Boyer-Moore y variantes saltan usando
información del patrón y son competitivos en texto práctico. La biblioteca actual ofrece búsqueda
de string; una futura colección de algoritmos debe documentar si trabaja sobre bytes o Unicode.

La distancia de edición extiende matching aproximado. Para archivos grandes, la tabla completa
puede ser prohibitiva; bandas diagonales, límites máximos o algoritmos especializados aprovechan
que sólo interesan distancias pequeñas.

### 44.2 Huffman

Huffman cuenta frecuencias, crea una hoja por símbolo y usa una cola de prioridad para combinar los
dos pesos menores. El árbol resultante asigna códigos prefijos: ningún código es prefijo de otro,
por lo que la decodificación es inequívoca.

Con `k` símbolos, construir mediante heap cuesta O(k log k). Codificar después cuesta O(n) más el
costo de emitir bits. El tutorial completo integra tabla de frecuencias, heap, árbol, recorrido y
ownership de nodos.

Un formato real debe almacenar o reconstruir el árbol, definir orden de bits, padding, encabezado,
caso de un solo símbolo y validación ante streams corruptos. Imprimir códigos no constituye todavía
un compresor de archivos.

## 45. Algoritmos numéricos y álgebra

### 45.1 Racionales

Un racional canónico mantiene denominador positivo y divide numerador y denominador por su máximo
común divisor. Así `1/2` y `2/4` tienen la misma representación. Suma y multiplicación pueden
desbordar `int` antes de normalizar; una biblioteca productiva debe elegir enteros más anchos,
comprobación o big integers.

Las comparaciones mediante producto cruzado evitan punto flotante, pero también pueden desbordar.
El programa editorial prioriza claridad del tipo y hace visible el límite.

### 45.2 Evaluación de polinomios

Para `a_n x^n + ... + a_0`, calcular cada potencia repite multiplicaciones. Horner reescribe como
`(...((a_n x + a_{n-1})x + a_{n-2})x...) + a_0` y usa O(n) multiplicaciones y O(1) memoria.

Sumar polinomios cuesta O(max(n, m)); multiplicarlos por convolución, O(nm); derivar, O(n). Una
representación densa es adecuada cuando muchos coeficientes existen. Para polinomios dispersos, una
lista ordenada de términos evita almacenar ceros.

### 45.3 Vectores y matrices

El producto punto combina componentes correspondientes. El producto cruz en tres dimensiones es
anticonmutativo: `a × b = -(b × a)`. La multiplicación de matrices no es conmutativa; `A * B`
representa aplicar `B` y después `A` bajo la convención usual de vectores columna.

Una matriz 3x3 densa usa nueve `double`. Multiplicar dos requiere 27 multiplicaciones y 18 sumas.
Generalizar a `n` produce O(n³) en el algoritmo clásico. El layout row-major o column-major debe
fijarse antes de interoperar con una API gráfica.

La sobrecarga de operadores mejora la lectura sólo si preserva estas leyes. Las pruebas útiles no
se limitan a números impresos: identidad, distributividad dentro de tolerancia, producto cruz
ortogonal y composición conocida.

---

# Parte XI. Estudios de caso completos

## 46. Cómo leer los proyectos editoriales

Los proyectos siguientes no son fragmentos decorativos. Cada uno contiene su `.nsproj`, todas las
clases auxiliares y el programa de entrada. En conjunto suman miles de líneas y recorren diseño de
tipos, invariantes, algoritmos, complejidad, polimorfismo, excepciones, genéricos, arrays, strings y
destrucción.

La secuencia recomendada para estudiar uno es:

1. Leer primero el problema y la invariante que preceden al listado.
2. Identificar dueños: cada `new`, cada campo que retiene una referencia y cada destructor.
3. Seguir un caso pequeño a mano.
4. Compilar en `-O0` y `-O2`.
5. Cambiar datos de entrada sin modificar el algoritmo.
6. Añadir una prueba de borde: vacío, uno, duplicado, capacidad exacta o fallo.
7. Sólo después, medir y optimizar.

Los listados se generan desde los archivos validados y sólo cambian en presentación: Allman,
cuatro espacios, sin espacio antes de paréntesis de función y máximo de 120 columnas. Las copias
editoriales resultantes se compilaron y ejecutaron de nuevo; por tanto, la legibilidad no descansa
en una transformación no comprobada.

## 47. Los 27 programas completos

<!-- GENERATED:BOOK_EXAMPLES -->

---

<!-- GENERATED:DROZDEK -->

---

<!-- GENERATED:DEITEL -->

---

# Apéndice A. Índice público de la biblioteca

Este índice se genera desde las declaraciones `public` de los fuentes actuales. Es una guía de
navegación, no reemplaza los contratos de ownership, excepciones y precondiciones descritos en cada
tipo.

<!-- GENERATED:PUBLIC_API -->

---

# Apéndice B. Tabla de tipos y ABI

| Tipo | Tamaño | Alineación | Observación |
|---|---:|---:|---|
| `bool` | 1 | 1 | valor lógico |
| `sbyte` | 1 | 1 | entero de 8 bits con signo |
| `byte` | 1 | 1 | entero de 8 bits sin signo |
| `char` | 1 | 1 | 8 bits sin signo; no UTF-16 |
| `short` | 2 | 2 | entero de 16 bits con signo |
| `ushort` | 2 | 2 | entero de 16 bits sin signo |
| `int` | 4 | 4 | entero de 32 bits con signo |
| `uint` | 4 | 4 | entero de 32 bits sin signo |
| `long` | 8 | 8 | entero de 64 bits con signo |
| `ulong` | 8 | 8 | entero de 64 bits sin signo |
| `nint` | 8 | 8 | entero nativo con signo en x64 |
| `nuint` | 8 | 8 | entero nativo sin signo en x64 |
| `float` | 4 | 4 | IEEE-754 binary32 |
| `double` | 8 | 8 | IEEE-754 binary64 |
| enum por omisión | 4 | 4 | puede declarar subyacente admitido |
| puntero | 8 | 8 | dirección x64 |

El destino es little-endian y la alineación natural máxima reportada es 8. Un agregado puede
elevarla mediante `align(N)`. El tamaño de un struct incluye padding final hasta su alineación.

# Apéndice C. Lista de comprobación para publicar código

## Corrección del lenguaje

- [ ] No se presupone una característica de C# o C++ que el compilador no implemente.
- [ ] Cada local se asigna en todos los caminos antes de leerse.
- [ ] Los casts que pueden perder información son explícitos y revisados.
- [ ] Cada `switch` tiene labels únicos y una política intencional para valores desconocidos.
- [ ] Las sobrecargas no dependen de conversiones ambiguas.

## Recursos y ownership

- [ ] Cada asignación de recurso tiene un dueño identificable.
- [ ] Cada campo owned se consume o destruye exactamente una vez.
- [ ] Cada transferencia usa una firma `owned` y `move` donde corresponde.
- [ ] Ninguna vista o referencia borrowed escapa de su origen.
- [ ] Los caminos de excepción ejecutan las mismas obligaciones de limpieza.
- [ ] Los elementos de arrays y colecciones tienen una política de destrucción explícita.

## Interoperabilidad

- [ ] Tamaños, alineaciones y offsets se compararon con un compilador C/C++ independiente.
- [ ] Strings y buffers declaran longitud, terminación y codificación.
- [ ] Los callbacks no dejan escapar excepciones.
- [ ] El header se generó desde el modelo, no se duplicó a mano.
- [ ] Los metadatos `.nsmeta` se emitieron y verificaron.
- [ ] La biblioteca se probó desde un host real.

## Verificación

- [ ] El proyecto pasa `--check-project` y `--verify-project-oop-llvm`.
- [ ] Compila y ejecuta en `-O0`.
- [ ] Compila y ejecuta en `-O2`.
- [ ] Las salidas y códigos de retorno coinciden.
- [ ] Hay casos vacíos, unitarios, de límite y de error.
- [ ] Se registró `nsc --version` junto al resultado.

# Apéndice D. Bibliografía editorial

Las siguientes obras orientaron el nivel de profundidad y la arquitectura pedagógica. NativeSharp
se documentó a partir de su propia implementación; estas referencias no definen su semántica.

- Bjarne Stroustrup, *The C++ Programming Language*, tercera edición especial.
- Bjarne Stroustrup, *The C++ Programming Language*, cuarta edición.
- Bjarne Stroustrup, *Programming: Principles and Practice Using C++*, tercera edición.
- Bjarne Stroustrup, *A Tour of C++*, tercera edición.
- Anders Hejlsberg et al., *The C# Programming Language*, cuarta edición.
- Paul Deitel y Harvey Deitel, *C# 6 for Programmers*, sexta edición.
- Adam Drozdek, *Data Structures and Algorithms in C++*, cuarta edición.

La organización general adopta una progresión deliberada: recorrido, facilidades básicas,
mecanismos de abstracción, biblioteca, concurrencia y compatibilidad; la enseñanza se apoya en
programas completos y estudios de caso; la referencia separa reglas normativas de consejos de
diseño.

---

## Colofón de verificación

Esta edición se generó desde el árbol de NativeSharp situado en `A:\\nsc-devel`. Los 23 tutoriales
y los 27 proyectos de `book_examples` fueron formateados para la edición, emitidos como PE32+ y
ejecutados en Windows. La validación cubrió `-O0` y `-O2`, con salida y checksum idénticos por
proyecto. El objetivo no es congelar el lenguaje alfa, sino hacer que cada edición pueda decir
exactamente qué compilador presentó al mundo.
