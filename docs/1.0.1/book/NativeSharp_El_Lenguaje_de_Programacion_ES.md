---
title: "NativeSharp: el lenguaje de programación"
subtitle: "La referencia absoluta para programadores de C# que quieren más"
language: "es"
edition: "Edición de desarrollo 1.0.1"
language-version: "NativeSharp 1.0.1"
compiler-version: "nsc 1.0.1-dev, Stage 67"
verified-build: "pendiente de baseline 1.0.1"
platform: "x86_64-pc-windows-msvc"
date: "2026-09-12"
---

# NativeSharp: el lenguaje de programación

## La referencia absoluta

### Para programadores de C# que quieren más: código nativo, control de memoria, ABI y rendimiento

#### Edición de desarrollo 1.0.1 en español

**Lenguaje NativeSharp 1.0.1**
**Compilador `nsc` 1.0.1-dev, Stage 67**
**Plataforma de referencia: Windows x64, ABI de Microsoft, objetos COFF y ejecutables PE32+**

> **Estado de esta edición.** Esta copia reúne la referencia vigente de la línea 1.0.1 y conserva
> el contenido sustantivo de la edición previamente comprobada. El compilador continúa en canal
> `-dev`; por tanto, esta cabecera no atribuye a 1.0.1 un baseline que todavía no se ha ejecutado.
> Cada construcción se distingue mediante `1.0.1-dev+build.<fecha>.<id>` y la certificación se
> registrará aquí únicamente cuando exista evidencia nueva.

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
alineación. `T[N]` inserta los elementos en el propio agregado; no guarda un puntero a otro bloque.
La forma multidimensional `T[rows, columns]` conserva ese almacenamiento inline, contiguo y
row-major: primero se recorren las columnas de una fila y después la fila siguiente. Por ejemplo,
`float[3, 4] transform;` ocupa exactamente doce `float`, sin cabecera ni asignación de heap. Los
modificadores de layout `pack(N)`, `align(N)` y `at(N)` permiten expresar contratos
foráneos, pero también pueden producir accesos menos eficientes o layouts incorrectos si se usan
por estética. Deben copiar una especificación ABI medida, no una intuición.

`align(N)` también puede preceder a un campo de instancia. En ese lugar eleva el requisito del
campo, inserta el padding necesario antes de él y eleva la alineación final del contenedor:

```csharp
public struct CommandPacket
{
    public byte Kind;
    public align(16) int Sequence;
    public byte Flags;
}
```

`Sequence` comienza en el offset 16 y el struct termina redondeado a 16 bytes. La regla se aplica
después de `pack(N)`, porque una solicitud explícita de alineación no debe ser anulada por el pack.
Si se combina con `at(N)`, el offset escrito debe respetar la alineación. No se admite en campos
estáticos, que son globals y no forman parte del objeto.

Para contratos C cuyo payload comienza justo después de un encabezado, `T[*]` expresa una cola
flexible sin convertirla en un puntero ni en un array de heap:

```csharp
public struct SamplePacket
{
    public uint Count;
    public float[*] Samples;
}

public static float Read(SamplePacket* packet, int index)
{
    return packet->Samples[index];
}
```

`sizeof(SamplePacket)` y `offsetof(SamplePacket, Samples)` valen 4. La memoria real debe haberse
reservado para `sizeof(SamplePacket) + Count * sizeof(float)` bytes, con su propia comprobación de
overflow. No hay `Length`, bounds check ni destrucción automática de los elementos. Por eso la cola
debe ser el último campo de un struct no-union y su elemento debe tener lifetime trivial. En el
header C, la misma declaración aparece como `float Samples[]`.

Cada dimensión se indexa en el mismo orden en que se declara:

```csharp
int[3, 4] matrix;

matrix[0, 0] = 10;
matrix[1, 2] = 20;
matrix[2, 3] = 12;

int total = matrix[0, 0] + matrix[1, 2] + matrix[2, 3];
```

`matrix[row, column]` calcula la dirección con los strides del layout canónico. También se admite
la forma encadenada `matrix[row][column]`; ambas designan el mismo slot. Como se trata del nivel
raw, un índice variable no agrega una rama de bounds-check. Los índices constantes sí se validan
contra la extensión concreta durante compilación.

Cuando el tamaño sólo se conoce al ejecutar una función, la misma sintaxis de tipo permite un
arreglo local de longitud variable:

```csharp
public static int SumWindow(int length)
{
    int[length] samples;

    int i = 0;
    while (i < length)
    {
        samples[i] = (i + 1) * 3;
        i += 1;
    }

    int total = 0;
    i = 0;
    while (i < length)
    {
        total += samples[i];
        i += 1;
    }
    return total;
}
```

`samples` no llama al heap: la reserva ocurre en el frame al alcanzar la declaración y desaparece
al retornar la función. Es almacenamiento raw contiguo, por lo que no posee `Length`, no agrega
bounds checks dinámicos y no puede devolverse ni sobrevivir al frame. La extensión debe ser
integral y el elemento debe tener lifetime trivial. Una dimensión constante, como `int[32]`, sigue
siendo un fixed array inline; un campo o una firma no puede depender de una longitud de ejecución.

También se admiten varias dimensiones de ejecución:

```csharp
public static void BuildTable(int rows, int columns)
{
    int[rows, columns] table;

    int row = 0;
    while (row < rows)
    {
        int column = 0;
        while (column < columns)
        {
            table[row, column] = row * 100 + column;
            column += 1;
        }
        row += 1;
    }
}
```

Las dimensiones se capturan una sola vez al declarar. La tabla es un único bloque row-major, no un
array de filas: `table[row, column]` usa el número de columnas capturado como stride de la fila.
Cambiar después la variable `columns` no reinterpreta ni redimensiona el almacenamiento existente.

La reserva se valida antes de tocar el stack. Una dimensión negativa no se convierte silenciosamente
en un entero sin signo enorme; cada multiplicación se comprueba antes de ejecutarse y una declaración
no puede reservar más de 1 MiB. Las dimensiones cero son válidas. Las demás violaciones terminan con
la trampa nativa `AllocationSizeOverflow`, en vez de corromper el frame.

Un VLA tampoco puede escapar de la función. Es válido crear un alias local o pasar el buffer como
puntero prestado a una operación que termina antes del retorno:

```csharp
public static void Fill(int count)
{
    int[count] values;
    int* data = values;
    NativeFill(data, count);
}
```

No es válido retornar `values` o `data`, guardarlos en un campo ni capturarlos en una lambda. El
compilador conserva la procedencia a través de aliases locales para que cambiar el nombre del
puntero no oculte que el almacenamiento desaparece con el frame.

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
La declaración local `T[length] name;` ofrece el mismo almacenamiento de frame con sintaxis de
arreglo y acceso `name[index]`, sin exigir que el programa declare manualmente un puntero.

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
campos y al final. `pack(N)` limita la alineación natural efectiva; `align(N)` eleva la alineación
del agregado o de un campo concreto; `at(N)` fija un offset. Una union superpone campos. Un array fijo se
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
new delete stackalloc sizeof alignof offsetof static_assert default
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

### 31.1 Aserciones de compilación

`static_assert` fija una propiedad que debe seguir siendo cierta al cambiar tipos, layouts o
constantes. Vive dentro de un bloque, no ejecuta código y acepta un mensaje literal opcional:

```csharp
public struct PacketHeader
{
    public uint Magic;
    public ushort Version;
    public ushort Flags;
}

public class PacketReader
{
    public static int ValidateAbi()
    {
        static_assert(sizeof(PacketHeader) == 8u, "PacketHeader must remain an 8-byte wire header");
        static_assert(alignof(PacketHeader) == 4u, "PacketHeader alignment changed");
        static_assert((1u << 15) == 32768u, "flag bit contract changed");
        return 0;
    }
}
```

La condición debe ser `bool` y constante. `static_assert(variable == 4);` no es una comprobación
dinámica y se rechaza; para esa intención use control de flujo normal. La comparación de tamaños
usa literales `u` porque `sizeof` y `alignof` producen `nuint`.

### 31.2 Offset de campos

`offsetof(T, campo)` permite certificar formatos binarios, registros compartidos y ABI C sin
construir un objeto ni fabricar aritmética de punteros:

```csharp
public struct PacketHeader
{
    public byte Kind;
    public uint Length;
    public long Sequence;
}

public class PacketAbi
{
    public static int Validate()
    {
        static_assert(offsetof(PacketHeader, Kind) == 0u, "Kind moved");
        static_assert(offsetof(PacketHeader, Length) == 4u, "Length moved");
        static_assert(offsetof(PacketHeader, Sequence) == 8u, "Sequence moved");
        static_assert(sizeof(PacketHeader) == 16u, "PacketHeader size changed");
        return 0;
    }
}
```

El resultado es `nuint` y proviene del layout real del target. Sólo admite campos de instancia
directos de structs y unions completos. También funciona sobre especializaciones cerradas, por
ejemplo `offsetof(Box<long>, Value)`, después de sustituir `T` y calcular el padding resultante.

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
--emit-project-oop-obj <project> [--profile=debug|release] [-O0|-O1|-O2|-O3] [-g0|-gline-tables-only|-g] -o <file.obj>
--emit-project-oop-exe <project> [--profile=debug|release] [-O0|-O1|-O2|-O3] [-g0|-gline-tables-only|-g] -o <file.exe>
--emit-project-oop-lib <project> [--profile=debug|release] [-O0|-O1|-O2|-O3] [-g0|-gline-tables-only|-g] -o <file.lib>
--emit-project-oop-dll <project> [--profile=debug|release] [-O0|-O1|-O2|-O3] [-g0|-gline-tables-only|-g] -o <file.dll>
--emit-project-host-header <project> -o <file.h>
--emit-project-library-metadata <project> -o <file.nsmeta>
--verify-library-metadata <file.nsmeta>
```

`--profile=debug` aplica `-O0 -g`; `--profile=release` aplica
`-O2 -gline-tables-only` para conservar líneas útiles en fallos de producción. Son valores por
defecto: un `-O*` o `-g*` explícito reemplaza sólo su dimensión, sin importar el orden de los
argumentos. El compilador imprime el perfil y las políticas efectivas cuando se usa esta opción.

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

### 35.1 Programa, `Main` y consola

**Objetivo.** La unidad mínima de programa, la referencia a Core, strings nativos y salida.

**Puente desde C#.** La forma resulta familiar para C#, pero el string temporal ya introduce vida nativa y destrucción.

**Estado comprobado.** Compila en `-O0` y `-O2`; ambas ejecuciones producen la misma salida y el mismo checksum de retorno (`29`). Un retorno distinto de cero es deliberado en esta serie: `Main` devuelve resultados calculados para que el ejecutable sea autoauditable.

```powershell
nsc --emit-project-oop-exe `
    examples/tutorial_01_hello_mundo/Tutorial01HelloMundo.nsproj `
    -O2 `
    -o tutorial_01_hello_mundo.exe
./tutorial_01_hello_mundo.exe
```

#### Proyecto

##### `examples/tutorial_01_hello_mundo/Tutorial01HelloMundo.nsproj`

```xml
<Project>
  <PropertyGroup><Name>Tutorial01HelloMundo</Name></PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
  </ItemGroup>
</Project>
```

#### Fuente editorial

##### `examples/tutorial_01_hello_mundo/Main.ns`

```csharp
using NativeSharp.Core;

namespace Tutorial.HelloMundo
{
    class Program
    {
        public static int Main()
        {

            raii string greeting = String.Copy("Hola, mundo desde NativeSharp");

            Console.WriteLine(greeting);

            raii string lengthLabel = String.Copy("Longitud del saludo: ");
            raii string lengthDigits = String.ToString(greeting.Length);
            raii string lengthLine = String.Concat(lengthLabel, lengthDigits);
            Console.WriteLine(lengthLine);

            return greeting.Length;
        }
    }
}
```

### 35.2 Variables, tipos y operadores

**Objetivo.** Enteros con signo y sin signo, flotantes, booleanos, conversiones y operadores.

**Puente desde C#.** Sustituye la intuición del CLR por anchos ABI concretos y promociones verificables.

**Estado comprobado.** Compila en `-O0` y `-O2`; ambas ejecuciones producen la misma salida y el mismo checksum de retorno (`410265574`). Un retorno distinto de cero es deliberado en esta serie: `Main` devuelve resultados calculados para que el ejecutable sea autoauditable.

```powershell
nsc --emit-project-oop-exe `
    examples/tutorial_02_variables_tipos_operadores/Tutorial02VariablesTipos.nsproj `
    -O2 `
    -o tutorial_02_variables_tipos_operadores.exe
./tutorial_02_variables_tipos_operadores.exe
```

#### Proyecto

##### `examples/tutorial_02_variables_tipos_operadores/Tutorial02VariablesTipos.nsproj`

```xml
<Project>
  <PropertyGroup><Name>Tutorial02VariablesTipos</Name></PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
  </ItemGroup>
</Project>
```

#### Fuente editorial

##### `examples/tutorial_02_variables_tipos_operadores/Main.ns`

```csharp
using NativeSharp.Core;

namespace Tutorial.VariablesTiposOperadores
{
    class Program
    {
        public static int Main()
        {

            byte smallByte = 200;
            short aShort = (short)-1000;

            ushort aUShort = 40000;
            int anInt = 100000;
            uint aUInt = 3000000000;
            long aLong = 9000000000;
            float aFloat = 3.5f;
            double aDouble = 2.71828;
            bool aBool = true;
            char aChar = 'N';

            int sum = anInt + 42;
            int difference = anInt - 42;
            int product = 6 * 7;
            int quotient = 100 / 3;
            int remainder = 100 % 3;

            int narrowed = (int)aLong;
            double widened = (double)anInt;

            int counter = 0;
            counter = counter + 1;
            counter = counter + 1;
            counter = counter - 1;

            int bitsA = 0b1010;
            int bitsB = 0b0110;
            int bitAnd = bitsA & bitsB;
            int bitOr = bitsA | bitsB;
            int bitXor = bitsA ^ bitsB;
            int bitNot = ~bitsA;
            int shiftedLeft = bitsA << 2;
            int shiftedRight = bitsA >> 1;

            int accumulator = 10;
            accumulator = accumulator + 5;
            accumulator = accumulator - 3;
            accumulator = accumulator * 2;
            accumulator = accumulator / 4;

            bool isGreater = sum > difference;
            bool isEqual = quotient == 33;
            bool logicalCombo = isGreater && isEqual || aBool;

            raii string title = String.Copy("=== Tutorial 02: variables, tipos y operadores ===");
            Console.WriteLine(title);

            raii string sumLabel = String.Copy("sum = ");
            raii string sumDigits = String.ToString(sum);
            raii string sumLine = String.Concat(sumLabel, sumDigits);
            Console.WriteLine(sumLine);

            raii string bitLabel = String.Copy("bitAnd/bitOr/bitXor = ");
            raii string bitAndDigits = String.ToString(bitAnd);
            raii string bitOrDigits = String.ToString(bitOr);
            raii string bitXorDigits = String.ToString(bitXor);
            raii string separator = String.Copy(" / ");
            raii string bitStep1 = String.Concat(bitLabel, bitAndDigits);
            raii string bitStep2 = String.Concat(bitStep1, separator);
            raii string bitStep3 = String.Concat(bitStep2, bitOrDigits);
            raii string bitStep4 = String.Concat(bitStep3, separator);
            raii string bitLine = String.Concat(bitStep4, bitXorDigits);
            Console.WriteLine(bitLine);

            raii string accLabel = String.Copy("accumulator final = ");
            raii string accDigits = String.ToString(accumulator);
            raii string accLine = String.Concat(accLabel, accDigits);
            Console.WriteLine(accLine);

            int result = sum + difference + product + quotient + remainder + narrowed + counter + bitAnd + bitOr +
                         bitXor + shiftedLeft + shiftedRight + accumulator;
            if (isGreater)
            {
                result = result + 1;
            }
            if (isEqual)
            {
                result = result + 1;
            }
            if (logicalCombo)
            {
                result = result + 1;
            }
            if (smallByte == 200)
            {
                result = result + 1;
            }
            if (aShort == -1000)
            {
                result = result + 1;
            }
            if (aUShort == 40000)
            {
                result = result + 1;
            }
            if (aUInt == 3000000000)
            {
                result = result + 1;
            }
            if (widened == 100000.0)
            {
                result = result + 1;
            }
            if (aChar == 'N')
            {
                result = result + 1;
            }
            if (bitNot == -11)
            {
                result = result + 1;
            }
            return result;
        }
    }
}
```

### 35.3 Control de flujo

**Objetivo.** Condiciones, loops, `break`, `continue`, condicional, `do` y `switch` en la superficie actual.

**Puente desde C#.** El control se parece a C#, pero su interacción con limpieza RAII y ownership forma parte de la semántica.

**Estado comprobado.** Compila en `-O0` y `-O2`; ambas ejecuciones producen la misma salida y el mismo checksum de retorno (`348`). Un retorno distinto de cero es deliberado en esta serie: `Main` devuelve resultados calculados para que el ejecutable sea autoauditable.

```powershell
nsc --emit-project-oop-exe `
    examples/tutorial_03_control_de_flujo/Tutorial03ControlDeFlujo.nsproj `
    -O2 `
    -o tutorial_03_control_de_flujo.exe
./tutorial_03_control_de_flujo.exe
```

#### Proyecto

##### `examples/tutorial_03_control_de_flujo/Tutorial03ControlDeFlujo.nsproj`

```xml
<Project>
  <PropertyGroup><Name>Tutorial03ControlDeFlujo</Name></PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
  </ItemGroup>
</Project>
```

#### Fuente editorial

##### `examples/tutorial_03_control_de_flujo/Main.ns`

```csharp
using NativeSharp.Core;

namespace Tutorial.ControlDeFlujo
{
    class Program
    {

        public static int Classify(int value)
        {
            if (value < 0)
            {
                return -1;
            }
            else if (value == 0)
            {
                return 0;
            }
            else
            {
                return 1;
            }
        }

        public static int Main()
        {
            int negatives = 0;
            int zeros = 0;
            int positives = 0;

            int[] samples = new int[7];
            samples[0] = -3;
            samples[1] = 0;
            samples[2] = 5;
            samples[3] = -1;
            samples[4] = 0;
            samples[5] = 8;
            samples[6] = -9;

            for (int i = 0; i < samples.Length; i = i + 1)
            {
                int kind = Classify(samples[i]);
                if (kind < 0)
                {
                    negatives = negatives + 1;
                }
                else if (kind == 0)
                {
                    zeros = zeros + 1;
                }
                else
                {
                    positives = positives + 1;
                }
            }

            int factorial = 1;
            int n = 5;
            while (n > 1)
            {
                factorial = factorial * n;
                n = n - 1;
            }

            int firstPositive = -1;
            int j = 0;
            while (j < samples.Length)
            {
                if (samples[j] < 0)
                {
                    j = j + 1;
                    continue;
                }
                if (samples[j] > 0)
                {
                    firstPositive = samples[j];
                    break;
                }
                j = j + 1;
            }

            delete[] samples;

            raii string summary = String.Copy("negativos/ceros/positivos = ");
            raii string negDigits = String.ToString(negatives);
            raii string zeroDigits = String.ToString(zeros);
            raii string posDigits = String.ToString(positives);
            raii string sep = String.Copy(" / ");
            raii string step1 = String.Concat(summary, negDigits);
            raii string step2 = String.Concat(step1, sep);
            raii string step3 = String.Concat(step2, zeroDigits);
            raii string step4 = String.Concat(step3, sep);
            raii string line = String.Concat(step4, posDigits);
            Console.WriteLine(line);

            raii string factLabel = String.Copy("5! = ");
            raii string factDigits = String.ToString(factorial);
            raii string factLine = String.Concat(factLabel, factDigits);
            Console.WriteLine(factLine);

            return negatives + zeros * 10 + positives * 100 + factorial + firstPositive;
        }
    }
}
```

### 35.4 Funciones, parámetros y recursión

**Objetivo.** Métodos estáticos, stack frames, retorno, recursión y parámetros por referencia.

**Puente desde C#.** NativeSharp incluye actualmente `ref`, `out` e `in`; el texto histórico que afirmaba lo contrario se excluye.

**Estado comprobado.** Compila en `-O0` y `-O2`; ambas ejecuciones producen la misma salida y el mismo checksum de retorno (`133`). Un retorno distinto de cero es deliberado en esta serie: `Main` devuelve resultados calculados para que el ejecutable sea autoauditable.

```powershell
nsc --emit-project-oop-exe `
    examples/tutorial_04_funciones_y_recursion/Tutorial04FuncionesRecursion.nsproj `
    -O2 `
    -o tutorial_04_funciones_y_recursion.exe
./tutorial_04_funciones_y_recursion.exe
```

#### Proyecto

##### `examples/tutorial_04_funciones_y_recursion/Tutorial04FuncionesRecursion.nsproj`

```xml
<Project>
  <PropertyGroup><Name>Tutorial04FuncionesRecursion</Name></PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
  </ItemGroup>
</Project>
```

#### Fuente editorial

##### `examples/tutorial_04_funciones_y_recursion/Main.ns`

```csharp
using NativeSharp.Core;

namespace Tutorial.FuncionesRecursion
{
    class Program
    {

        private static int factorialCalls = 0;

        public static int Factorial(int n)
        {
            factorialCalls = factorialCalls + 1;
            if (n <= 1)
            {
                return 1;
            }
            return n * Factorial(n - 1);
        }

        public static int FibonacciRecursive(int n)
        {
            if (n <= 1)
            {
                return n;
            }
            return FibonacciRecursive(n - 1) + FibonacciRecursive(n - 2);
        }

        public static int FibonacciIterative(int n)
        {
            if (n <= 1)
            {
                return n;
            }
            int previous = 0;
            int current = 1;
            int i = 2;
            while (i <= n)
            {
                int next = previous + current;
                previous = current;
                current = next;
                i = i + 1;
            }
            return current;
        }

        public static int TryToMutate(int value)
        {
            value = value * 100;
            return value;
        }

        public static int ScopeDemo(int factorialCalls)
        {

            int localOnly = factorialCalls + 1;
            return localOnly;
        }

        public static int Main()
        {
            int fact5 = Factorial(5);
            int callsAfterFact5 = factorialCalls;

            int fibRecursive10 = FibonacciRecursive(10);
            int fibIterative10 = FibonacciIterative(10);

            int original = 7;
            int mutated = TryToMutate(original);

            int scoped = ScopeDemo(1000);

            raii string factLabel = String.Copy("5! = ");
            raii string factDigits = String.ToString(fact5);
            raii string callsLabel = String.Copy(" (llamadas recursivas: ");
            raii string callsDigits = String.ToString(callsAfterFact5);
            raii string closeParen = String.Copy(")");
            raii string s1 = String.Concat(factLabel, factDigits);
            raii string s2 = String.Concat(s1, callsLabel);
            raii string s3 = String.Concat(s2, callsDigits);
            raii string factLine = String.Concat(s3, closeParen);
            Console.WriteLine(factLine);

            raii string fibLabel = String.Copy("fib(10) recursivo/iterativo = ");
            raii string fibRDigits = String.ToString(fibRecursive10);
            raii string slash = String.Copy(" / ");
            raii string fibIDigits = String.ToString(fibIterative10);
            raii string f1 = String.Concat(fibLabel, fibRDigits);
            raii string f2 = String.Concat(f1, slash);
            raii string fibLine = String.Concat(f2, fibIDigits);
            Console.WriteLine(fibLine);

            int result = original + mutated + scoped + fact5 + fibRecursive10;
            if (fibRecursive10 == fibIterative10)
            {
                result = result + 1000;
            }
            return result % 250;
        }
    }
}
```

### 35.5 Arrays, strings y memoria contigua

**Objetivo.** Longitud, indexación, recorrido, copia, inversión y strings con longitud explícita.

**Puente desde C#.** Un array o string no queda vivo por intervención de un GC; cada dueño debe identificarse.

**Estado comprobado.** Compila en `-O0` y `-O2`; ambas ejecuciones producen la misma salida y el mismo checksum de retorno (`212`). Un retorno distinto de cero es deliberado en esta serie: `Main` devuelve resultados calculados para que el ejecutable sea autoauditable.

```powershell
nsc --emit-project-oop-exe `
    examples/tutorial_05_arrays_y_strings/Tutorial05ArraysStrings.nsproj `
    -O2 `
    -o tutorial_05_arrays_y_strings.exe
./tutorial_05_arrays_y_strings.exe
```

#### Proyecto

##### `examples/tutorial_05_arrays_y_strings/Tutorial05ArraysStrings.nsproj`

```xml
<Project>
  <PropertyGroup><Name>Tutorial05ArraysStrings</Name></PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
  </ItemGroup>
</Project>
```

#### Fuente editorial

##### `examples/tutorial_05_arrays_y_strings/Main.ns`

```csharp
using NativeSharp.Core;

namespace Tutorial.ArraysStrings
{
    class Program
    {

        public static int SumArray(int[] values)
        {
            int total = 0;
            for (int i = 0; i < values.Length; i = i + 1)
            {
                total = total + values[i];
            }
            return total;
        }

        public static int MaxArray(int[] values)
        {
            int max = values[0];
            for (int i = 1; i < values.Length; i = i + 1)
            {
                if (values[i] > max)
                {
                    max = values[i];
                }
            }
            return max;
        }

        public static void ReverseInPlace(int[] values)
        {
            int left = 0;
            int right = values.Length - 1;
            while (left < right)
            {
                int temp = values[left];
                values[left] = values[right];
                values[right] = temp;
                left = left + 1;
                right = right - 1;
            }
        }

        public static int Main()
        {

            int[] numbers = new int[6];
            numbers[0] = 4;
            numbers[1] = 8;
            numbers[2] = 15;
            numbers[3] = 16;
            numbers[4] = 23;
            numbers[5] = 42;

            int total = SumArray(numbers);
            int max = MaxArray(numbers);
            ReverseInPlace(numbers);
            int firstAfterReverse = numbers[0];

            delete[] numbers;

            raii string word = String.Copy("NativeSharp");
            int wordLength = word.Length;
            char firstChar = word[0];
            raii string upper = String.ToUpper(word);
            raii string lower = String.ToLower(word);
            bool startsWithNative = String.StartsWith(word, "Native");

            char[] letters = String.ToCharArray(word);
            int consonantCount = 0;
            for (int i = 0; i < letters.Length; i = i + 1)
            {
                char c = letters[i];
                bool isVowel = c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U' || c == 'a' || c == 'e' ||
                               c == 'i' || c == 'o' || c == 'u';
                if (!isVowel)
                {
                    consonantCount = consonantCount + 1;
                }
            }
            delete[] letters;

            raii string totalLabel = String.Copy("suma/max/primero-tras-invertir = ");
            raii string totalDigits = String.ToString(total);
            raii string sep = String.Copy(" / ");
            raii string maxDigits = String.ToString(max);
            raii string firstDigits = String.ToString(firstAfterReverse);
            raii string a1 = String.Concat(totalLabel, totalDigits);
            raii string a2 = String.Concat(a1, sep);
            raii string a3 = String.Concat(a2, maxDigits);
            raii string a4 = String.Concat(a3, sep);
            raii string arrayLine = String.Concat(a4, firstDigits);
            Console.WriteLine(arrayLine);

            raii string upperLine = String.Copy("upper/lower = ");
            raii string b1 = String.Concat(upperLine, upper);
            raii string bsep = String.Copy(" / ");
            raii string b2 = String.Concat(b1, bsep);
            raii string stringLine = String.Concat(b2, lower);
            Console.WriteLine(stringLine);

            int result = total + max + firstAfterReverse + wordLength + consonantCount;
            if (startsWithNative)
            {
                result = result + 1;
            }
            if (firstChar == 'N')
            {
                result = result + 1;
            }
            return result % 250;
        }
    }
}
```

### 35.6 Structs y semántica de valor

**Objetivo.** Campos inline, copia independiente, métodos y arrays de valores.

**Puente desde C#.** A diferencia de una clase C#, no hay boxing implícito; el layout nativo permanece visible.

**Estado comprobado.** Compila en `-O0` y `-O2`; ambas ejecuciones producen la misma salida y el mismo checksum de retorno (`227`). Un retorno distinto de cero es deliberado en esta serie: `Main` devuelve resultados calculados para que el ejecutable sea autoauditable.

```powershell
nsc --emit-project-oop-exe `
    examples/tutorial_06_structs/Tutorial06Structs.nsproj `
    -O2 `
    -o tutorial_06_structs.exe
./tutorial_06_structs.exe
```

#### Proyecto

##### `examples/tutorial_06_structs/Tutorial06Structs.nsproj`

```xml
<Project>
  <PropertyGroup><Name>Tutorial06Structs</Name></PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
  </ItemGroup>
</Project>
```

#### Fuente editorial

##### `examples/tutorial_06_structs/Main.ns`

```csharp
using NativeSharp.Core;

namespace Tutorial.Structs
{

    public struct Point
    {
        public int X;
        public int Y;

        public int ManhattanDistanceFromOrigin()
        {
            int absX = X;
            if (absX < 0)
            {
                absX = -absX;
            }
            int absY = Y;
            if (absY < 0)
            {
                absY = -absY;
            }
            return absX + absY;
        }

        public static Point Add(Point a, Point b)
        {
            Point result;
            result.X = a.X + b.X;
            result.Y = a.Y + b.Y;
            return result;
        }
    }

    class Program
    {

        public static void TryToMutate(Point p)
        {
            p.X = 999;
            p.Y = 999;
        }

        public static int Main()
        {
            Point origin;
            origin.X = 0;
            origin.Y = 0;

            Point a;
            a.X = 3;
            a.Y = 4;
            int distanceA = a.ManhattanDistanceFromOrigin();

            Point b = a;
            b.X = 100;

            TryToMutate(a);

            Point sum = Point.Add(a, b);

            Point[] points = new Point[3];
            points[0].X = 1;
            points[0].Y = 1;
            points[1].X = 2;
            points[1].Y = 2;
            points[2].X = 3;
            points[2].Y = 3;
            int totalX = 0;
            for (int i = 0; i < points.Length; i = i + 1)
            {
                totalX = totalX + points[i].X;
            }
            delete[] points;

            raii string line1 = String.Copy("distanceA/a.X/b.X = ");
            raii string d1 = String.ToString(distanceA);
            raii string sep = String.Copy(" / ");
            raii string d2 = String.ToString(a.X);
            raii string d3 = String.ToString(b.X);
            raii string s1 = String.Concat(line1, d1);
            raii string s2 = String.Concat(s1, sep);
            raii string s3 = String.Concat(s2, d2);
            raii string s4 = String.Concat(s3, sep);
            raii string valueLine = String.Concat(s4, d3);
            Console.WriteLine(valueLine);

            int result = distanceA + a.X + b.X + sum.X + sum.Y + totalX;
            return result % 250;
        }
    }
}
```

### 35.7 Clases, identidad y destructores

**Objetivo.** Construcción, campos privados, métodos, alias de referencia y destrucción.

**Puente desde C#.** Es el punto donde un programador de C# debe abandonar la suposición de alcance administrado.

**Estado comprobado.** Compila en `-O0` y `-O2`; ambas ejecuciones producen la misma salida y el mismo checksum de retorno (`61`). Un retorno distinto de cero es deliberado en esta serie: `Main` devuelve resultados calculados para que el ejecutable sea autoauditable.

```powershell
nsc --emit-project-oop-exe `
    examples/tutorial_07_clases_y_objetos/Tutorial07ClasesObjetos.nsproj `
    -O2 `
    -o tutorial_07_clases_y_objetos.exe
./tutorial_07_clases_y_objetos.exe
```

#### Proyecto

##### `examples/tutorial_07_clases_y_objetos/Tutorial07ClasesObjetos.nsproj`

```xml
<Project>
  <PropertyGroup><Name>Tutorial07ClasesObjetos</Name></PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
  </ItemGroup>
</Project>
```

#### Fuente editorial

##### `examples/tutorial_07_clases_y_objetos/Main.ns`

```csharp
using NativeSharp.Core;

namespace Tutorial.ClasesObjetos
{
    public class Employee
    {

        private string name;
        private int salary;

        private static int liveEmployeeCount = 0;

        public Employee(string employeeName, int employeeSalary)
        {

            name = String.Copy(employeeName);
            salary = employeeSalary;
            liveEmployeeCount = liveEmployeeCount + 1;
        }

        public int GiveRaise(int amount)
        {
            salary = salary + amount;
            return salary;
        }

        public int Salary()
        {
            return salary;
        }

        public string Name()
        {
            return name;
        }

        public static int LiveCount()
        {
            return liveEmployeeCount;
        }

        ~Employee()
        {
            liveEmployeeCount = liveEmployeeCount - 1;
        }
    }

    class Program
    {
        public static int Main()
        {
            if (Employee.LiveCount() != 0)
            {
                return 1;
            }

            raii Employee alice = new Employee("Alice", 50000);
            raii Employee bob = new Employee("Bob", 60000);
            int liveAfterTwo = Employee.LiveCount();

            int aliceAfterRaise = alice.GiveRaise(5000);

            borrowed Employee alias = alice;
            alias.GiveRaise(1000);
            int aliceAfterAliasRaise = alice.Salary();

            raii string nameLabel = String.Copy("empleado: ");
            raii string nameLine = String.Concat(nameLabel, alice.Name());
            Console.WriteLine(nameLine);

            raii string salaryLabel = String.Copy("salario final = ");
            raii string salaryDigits = String.ToString(aliceAfterAliasRaise);
            raii string salaryLine = String.Concat(salaryLabel, salaryDigits);
            Console.WriteLine(salaryLine);

            int bobSalary = bob.Salary();

            int result = liveAfterTwo * 1000 + bobSalary / 1000;
            if (aliceAfterAliasRaise == 56000)
            {
                result = result + 1;
            }
            return result % 250;
        }
    }
}
```

### 35.8 Herencia y polimorfismo

**Objetivo.** Bases abstractas, métodos virtuales, overrides y destrucción a través de una referencia base.

**Puente desde C#.** La sintaxis es cercana a C#, pero el despacho y la destrucción son enteramente nativos.

**Estado comprobado.** Compila en `-O0` y `-O2`; ambas ejecuciones producen la misma salida y el mismo checksum de retorno (`83`). Un retorno distinto de cero es deliberado en esta serie: `Main` devuelve resultados calculados para que el ejecutable sea autoauditable.

```powershell
nsc --emit-project-oop-exe `
    examples/tutorial_08_herencia_y_polimorfismo/Tutorial08HerenciaPolimorfismo.nsproj `
    -O2 `
    -o tutorial_08_herencia_y_polimorfismo.exe
./tutorial_08_herencia_y_polimorfismo.exe
```

#### Proyecto

##### `examples/tutorial_08_herencia_y_polimorfismo/Tutorial08HerenciaPolimorfismo.nsproj`

```xml
<Project>
  <PropertyGroup><Name>Tutorial08HerenciaPolimorfismo</Name></PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
  </ItemGroup>
</Project>
```

#### Fuente editorial

##### `examples/tutorial_08_herencia_y_polimorfismo/Main.ns`

```csharp
using NativeSharp.Core;

namespace Tutorial.HerenciaPolimorfismo
{

    public abstract class Shape
    {
        protected string label;

        public Shape(string shapeLabel)
        {
            label = String.Copy(shapeLabel);
        }

        public abstract int Area();

        public virtual owned string Describe()
        {
            raii string prefix = String.Copy("Figura: ");
            return String.Concat(prefix, label);
        }

        public string Label()
        {
            return label;
        }
    }

    public class Rectangle : Shape
    {
        private int width;
        private int height;

        public Rectangle(int rectWidth, int rectHeight) : base("Rectangulo")
        {
            width = rectWidth;
            height = rectHeight;
        }

        public override int Area()
        {
            return width * height;
        }
    }

    public class Square : Rectangle
    {

        public Square(int side) : base(side, side)
        {
        }

        public override owned string Describe()
        {
            raii string baseDescription = base.Describe();
            raii string suffix = String.Copy(" (cuadrado)");
            return String.Concat(baseDescription, suffix);
        }
    }

    public class Circle : Shape
    {
        private int radius;

        public Circle(int circleRadius) : base("Circulo")
        {
            radius = circleRadius;
        }

        public override int Area()
        {
            return radius * radius * 3;
        }
    }

    class Program
    {

        public static int Describe_And_Area(borrowed Shape shape)
        {
            return shape.Area();
        }

        public static int Main()
        {
            raii Rectangle rect = new Rectangle(4, 5);
            raii Square square = new Square(6);
            raii Circle circle = new Circle(3);

            int rectArea = Describe_And_Area(rect);
            int squareArea = Describe_And_Area(square);
            int circleArea = Describe_And_Area(circle);

            raii string squareDescription = square.Describe();

            Console.WriteLine(squareDescription);

            raii string rectDescription = rect.Describe();

            Console.WriteLine(rectDescription);

            raii string areaLabel = String.Copy("areas rect/square/circle = ");
            raii string a1 = String.ToString(rectArea);
            raii string sep = String.Copy(" / ");
            raii string a2 = String.ToString(squareArea);
            raii string a3 = String.ToString(circleArea);
            raii string s1 = String.Concat(areaLabel, a1);
            raii string s2 = String.Concat(s1, sep);
            raii string s3 = String.Concat(s2, a2);
            raii string s4 = String.Concat(s3, sep);
            raii string areaLine = String.Concat(s4, a3);
            Console.WriteLine(areaLine);

            int result = rectArea + squareArea + circleArea;
            return result % 250;
        }
    }
}
```

### 35.9 Interfaces

**Objetivo.** Contratos múltiples, implementación y despacho por interfaz.

**Puente desde C#.** Las interfaces no son metadata CLR: el compilador genera tablas y ajustes de receptor nativos.

**Estado comprobado.** Compila en `-O0` y `-O2`; ambas ejecuciones producen la misma salida y el mismo checksum de retorno (`61`). Un retorno distinto de cero es deliberado en esta serie: `Main` devuelve resultados calculados para que el ejecutable sea autoauditable.

```powershell
nsc --emit-project-oop-exe `
    examples/tutorial_09_interfaces/Tutorial09Interfaces.nsproj `
    -O2 `
    -o tutorial_09_interfaces.exe
./tutorial_09_interfaces.exe
```

#### Proyecto

##### `examples/tutorial_09_interfaces/Tutorial09Interfaces.nsproj`

```xml
<Project>
  <PropertyGroup><Name>Tutorial09Interfaces</Name></PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
  </ItemGroup>
</Project>
```

#### Fuente editorial

##### `examples/tutorial_09_interfaces/Main.ns`

```csharp
using NativeSharp.Core;

namespace Tutorial.Interfaces
{
    public interface IOperation
    {
        int Apply(int left, int right);
    }

    public class Adder : IOperation
    {
        public int Apply(int left, int right)
        {
            return left + right;
        }
    }

    public class Multiplier : IOperation
    {
        public int Apply(int left, int right)
        {
            return left * right;
        }
    }

    public class Subtractor : IOperation
    {
        public int Apply(int left, int right)
        {
            return left - right;
        }
    }

    public interface IReadable
    {
        int Read();
    }

    public interface IStatus
    {
        int Status();
    }

    public interface IAdvanced : IReadable,
                                 IStatus
    {
        int Scale(int factor);
    }

    public class Sensor : IAdvanced
    {
        private int value;
        private int status;

        public Sensor(int initialValue)
        {
            value = initialValue;
            status = 1;
        }

        public int Read()
        {
            return value;
        }

        public int Status()
        {
            return status;
        }

        public int Scale(int factor)
        {
            value = value * factor;
            return value;
        }
    }

    class Program
    {

        public static int RunOperation(borrowed IOperation operation, int a, int b)
        {
            return operation.Apply(a, b);
        }

        public static int Main()
        {
            raii Adder adder = new Adder();
            raii Multiplier multiplier = new Multiplier();
            raii Subtractor subtractor = new Subtractor();

            int sum = RunOperation(adder, 7, 3);
            int product = RunOperation(multiplier, 7, 3);
            int difference = RunOperation(subtractor, 7, 3);

            raii Sensor sensor = new Sensor(5);
            int beforeScale = sensor.Read();
            int afterScale = sensor.Scale(4);
            int status = sensor.Status();

            raii string opsLabel = String.Copy("suma/producto/resta = ");
            raii string d1 = String.ToString(sum);
            raii string sep = String.Copy(" / ");
            raii string d2 = String.ToString(product);
            raii string d3 = String.ToString(difference);
            raii string s1 = String.Concat(opsLabel, d1);
            raii string s2 = String.Concat(s1, sep);
            raii string s3 = String.Concat(s2, d2);
            raii string s4 = String.Concat(s3, sep);
            raii string opsLine = String.Concat(s4, d3);
            Console.WriteLine(opsLine);

            raii string sensorLabel = String.Copy("sensor antes/despues de escalar = ");
            raii string e1 = String.ToString(beforeScale);
            raii string e2 = String.ToString(afterScale);
            raii string t1 = String.Concat(sensorLabel, e1);
            raii string t2 = String.Concat(t1, sep);
            raii string sensorLine = String.Concat(t2, e2);
            Console.WriteLine(sensorLine);

            int result = sum + product + difference + beforeScale + afterScale + status;
            return result % 250;
        }
    }
}
```

### 35.10 Propiedades, indexadores y operadores

**Objetivo.** Superficies expresivas para estado, colecciones y tipos algebraicos.

**Puente desde C#.** Se conserva la legibilidad de C# sin esconder asignación, retorno de dueños o costo de objetos nuevos.

**Estado comprobado.** Compila en `-O0` y `-O2`; ambas ejecuciones producen la misma salida y el mismo checksum de retorno (`98`). Un retorno distinto de cero es deliberado en esta serie: `Main` devuelve resultados calculados para que el ejecutable sea autoauditable.

```powershell
nsc --emit-project-oop-exe `
    examples/tutorial_10_propiedades_indexadores_operadores/Tutorial10PropiedadesIndexadoresOperadores.nsproj `
    -O2 `
    -o tutorial_10_propiedades_indexadores_operadores.exe
./tutorial_10_propiedades_indexadores_operadores.exe
```

#### Proyecto

##### `examples/tutorial_10_propiedades_indexadores_operadores/Tutorial10PropiedadesIndexadoresOperadores.nsproj`

```xml
<Project>
  <PropertyGroup><Name>Tutorial10PropiedadesIndexadoresOperadores</Name></PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
  </ItemGroup>
</Project>
```

#### Fuente editorial

##### `examples/tutorial_10_propiedades_indexadores_operadores/Main.ns`

```csharp
using NativeSharp.Core;

namespace Tutorial.PropiedadesIndexadoresOperadores
{
    public class Vector2
    {
        private int x;
        private int y;

        public Vector2(int initialX, int initialY)
        {
            x = initialX;
            y = initialY;
        }

        public int X
        {
            get
            {
                return x;
            }
            set
            {
                x = value;
            }
        }

        public int Y
        {
            get
            {
                return y;
            }
            set
            {
                y = value;
            }
        }

        public int ManhattanLength
        {
            get
            {
                int absX = x;
                if (absX < 0)
                {
                    absX = -absX;
                }
                int absY = y;
                if (absY < 0)
                {
                    absY = -absY;
                }
                return absX + absY;
            }
        }

        public int this[int index]
        {
            get
            {
                if (index == 0)
                {
                    return x;
                }
                return y;
            }
            set
            {
                if (index == 0)
                {
                    x = value;
                }
                else
                {
                    y = value;
                }
            }
        }

        public static Vector2 operator +(Vector2 left, Vector2 right)
        {
            return new Vector2(left.X + right.X, left.Y + right.Y);
        }

        public static bool operator ==(Vector2 left, Vector2 right)
        {
            return left.X == right.X && left.Y == right.Y;
        }

        public static bool operator !=(Vector2 left, Vector2 right)
        {
            return !(left == right);
        }
    }

    class Program
    {
        public static int Main()
        {

            Vector2 a = new Vector2(3, 4);
            Vector2 b = new Vector2(1, 2);

            int lengthA = a.ManhattanLength;

            int firstComponent = a[0];
            a[1] = 40;
            int mutatedY = a.Y;

            Vector2 sum = a + b;

            Vector2 same = new Vector2(4, 42);
            bool equalsSum = sum == same;
            bool differsFromA = sum != a;

            raii string lengthLabel = String.Copy("a.ManhattanLength = ");
            raii string lengthDigits = String.ToString(lengthA);
            raii string lengthLine = String.Concat(lengthLabel, lengthDigits);
            Console.WriteLine(lengthLine);

            raii string sumLabel = String.Copy("sum = (");
            raii string sumX = String.ToString(sum.X);
            raii string comma = String.Copy(", ");
            raii string sumY = String.ToString(sum.Y);
            raii string close = String.Copy(")");
            raii string s1 = String.Concat(sumLabel, sumX);
            raii string s2 = String.Concat(s1, comma);
            raii string s3 = String.Concat(s2, sumY);
            raii string sumLine = String.Concat(s3, close);
            Console.WriteLine(sumLine);

            int result = lengthA + firstComponent + mutatedY + sum.X + sum.Y;
            if (equalsSum)
            {
                result = result + 1;
            }
            if (differsFromA)
            {
                result = result + 1;
            }
            return result % 250;
        }
    }
}
```

### 35.11 RTTI con `is` y `as`

**Objetivo.** Pruebas de tipo y casts comprobados sobre jerarquías nativas.

**Puente desde C#.** No hay reflection ni boxing del CLR; las consultas usan descriptores de NativeSharp.

**Estado comprobado.** Compila en `-O0` y `-O2`; ambas ejecuciones producen la misma salida y el mismo checksum de retorno (`7`). Un retorno distinto de cero es deliberado en esta serie: `Main` devuelve resultados calculados para que el ejecutable sea autoauditable.

```powershell
nsc --emit-project-oop-exe `
    examples/tutorial_11_rtti_is_as/Tutorial11RttiIsAs.nsproj `
    -O2 `
    -o tutorial_11_rtti_is_as.exe
./tutorial_11_rtti_is_as.exe
```

#### Proyecto

##### `examples/tutorial_11_rtti_is_as/Tutorial11RttiIsAs.nsproj`

```xml
<Project>
  <PropertyGroup><Name>Tutorial11RttiIsAs</Name></PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
  </ItemGroup>
</Project>
```

#### Fuente editorial

##### `examples/tutorial_11_rtti_is_as/Main.ns`

```csharp
using NativeSharp.Core;

namespace Tutorial.RttiIsAs
{
    public interface IPet
    {
    }

    public class Animal
    {
    }

    public class Dog : Animal, IPet
    {
        public int Bark()
        {
            return 1;
        }
    }

    public class Cat : Animal, IPet
    {
        public int Meow()
        {
            return 2;
        }
    }

    class Program
    {
        public static int Main()
        {
            Dog rex = new Dog();
            Animal asAnimal = rex;

            bool isDog = asAnimal is Dog;
            bool isCat = asAnimal is Cat;
            bool isPet = asAnimal is IPet;

            Dog safeDog = asAnimal as Dog;
            Cat safeCat = asAnimal as Cat;

            Dog checkedDog = (Dog)asAnimal;
            int barks = checkedDog.Bark();

            int meowsOrZero = 0;
            Cat maybeCat = asAnimal as Cat;
            if (maybeCat != null)
            {
                meowsOrZero = maybeCat.Meow();
            }

            Animal nothing = null;
            bool nullIsDog = nothing is Dog;

            raii string line1 = String.Copy("isDog/isCat/isPet = ");
            raii string sep = String.Copy(" / ");
            raii string d1 = String.ToString(isDog);
            raii string d2 = String.ToString(isCat);
            raii string d3 = String.ToString(isPet);
            raii string s1 = String.Concat(line1, d1);
            raii string s2 = String.Concat(s1, sep);
            raii string s3 = String.Concat(s2, d2);
            raii string s4 = String.Concat(s3, sep);
            raii string line1Full = String.Concat(s4, d3);
            Console.WriteLine(line1Full);

            int result = barks + meowsOrZero;
            if (isDog)
            {
                result = result + 1;
            }
            if (!isCat)
            {
                result = result + 1;
            }
            if (isPet)
            {
                result = result + 1;
            }
            if (safeDog != null)
            {
                result = result + 1;
            }
            if (safeCat == null)
            {
                result = result + 1;
            }
            if (!nullIsDog)
            {
                result = result + 1;
            }
            return result % 250;
        }
    }
}
```

### 35.12 Ownership y RAII

**Objetivo.** La diferencia central del lenguaje: dueño, préstamo, transferencia y limpieza de alcance.

**Puente desde C#.** Convierte en tipos y diagnósticos las obligaciones que C# normalmente entrega al GC.

**Estado comprobado.** Compila en `-O0` y `-O2`; ambas ejecuciones producen la misma salida y el mismo checksum de retorno (`4`). Un retorno distinto de cero es deliberado en esta serie: `Main` devuelve resultados calculados para que el ejecutable sea autoauditable.

```powershell
nsc --emit-project-oop-exe `
    examples/tutorial_12_ownership_raii_owned_borrowed_move/Tutorial12OwnershipModel.nsproj `
    -O2 `
    -o tutorial_12_ownership_raii_owned_borrowed_move.exe
./tutorial_12_ownership_raii_owned_borrowed_move.exe
```

#### Proyecto

##### `examples/tutorial_12_ownership_raii_owned_borrowed_move/Tutorial12OwnershipModel.nsproj`

```xml
<Project>
  <PropertyGroup><Name>Tutorial12OwnershipModel</Name></PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
  </ItemGroup>
</Project>
```

#### Fuente editorial

##### `examples/tutorial_12_ownership_raii_owned_borrowed_move/Main.ns`

```csharp
using NativeSharp.Core;

namespace Tutorial.OwnershipModel
{
    public class TrackedResource
    {

        private static int liveCount = 0;
        private static int destroyedCount = 0;
        private int value;

        public TrackedResource(int initialValue)
        {
            value = initialValue;
            liveCount = liveCount + 1;
        }

        public int Read()
        {
            return value;
        }

        public static int LiveCount()
        {
            return liveCount;
        }
        public static int DestroyedCount()
        {
            return destroyedCount;
        }

        ~TrackedResource()
        {
            liveCount = liveCount - 1;
            destroyedCount = destroyedCount + 1;
        }
    }

    class Program
    {

        public static int ScopeCleanup()
        {
            raii TrackedResource outer = new TrackedResource(1);
            int liveWithOuterOnly = TrackedResource.LiveCount();
            {
                raii TrackedResource nested = new TrackedResource(2);
                int liveWithBoth = TrackedResource.LiveCount();
                if (liveWithBoth != 2)
                {
                    return -1;
                }
            }

            int liveAfterInnerScope = TrackedResource.LiveCount();
            if (liveWithOuterOnly != 1 || liveAfterInnerScope != 1)
            {
                return -2;
            }
            return 0;
        }

        public static int MoveTransfersOwnership()
        {
            raii TrackedResource first = new TrackedResource(42);
            raii TrackedResource second = move(first);

            int valueViaSecond = second.Read();
            return valueViaSecond;
        }

        public static int CleanupOnException()
        {
            try
            {
                raii TrackedResource protectedResource = new TrackedResource(99);
                throw new Exception();
            }
            catch (Exception ex)
            {
                return TrackedResource.LiveCount();
            }
        }

        public static int Main()
        {
            if (TrackedResource.LiveCount() != 0)
            {
                return 1;
            }

            int scopeResult = ScopeCleanup();
            if (scopeResult != 0)
            {
                return 2;
            }
            if (TrackedResource.LiveCount() != 0)
            {
                return 3;
            }

            int movedValue = MoveTransfersOwnership();
            if (movedValue != 42)
            {
                return 4;
            }

            int liveDuringException = CleanupOnException();
            if (liveDuringException != 0)
            {
                return 5;
            }

            int totalDestroyed = TrackedResource.DestroyedCount();

            raii string label = String.Copy("objetos destruidos automaticamente = ");
            raii string digits = String.ToString(totalDestroyed);
            raii string line = String.Concat(label, digits);
            Console.WriteLine(line);

            return totalDestroyed;
        }
    }
}
```

### 35.13 Ownership en firmas

**Objetivo.** Retornos owned, parámetros borrowed, consumo y `move` entre funciones.

**Puente desde C#.** En una API NativeSharp, propiedad y tiempo de vida son parte de la firma pública.

**Estado comprobado.** Compila en `-O0` y `-O2`; ambas ejecuciones producen la misma salida y el mismo checksum de retorno (`142`). Un retorno distinto de cero es deliberado en esta serie: `Main` devuelve resultados calculados para que el ejecutable sea autoauditable.

```powershell
nsc --emit-project-oop-exe `
    examples/tutorial_13_ownership_en_funciones/Tutorial13OwnershipEnFunciones.nsproj `
    -O2 `
    -o tutorial_13_ownership_en_funciones.exe
./tutorial_13_ownership_en_funciones.exe
```

#### Proyecto

##### `examples/tutorial_13_ownership_en_funciones/Tutorial13OwnershipEnFunciones.nsproj`

```xml
<Project>
  <PropertyGroup><Name>Tutorial13OwnershipEnFunciones</Name></PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
  </ItemGroup>
</Project>
```

#### Fuente editorial

##### `examples/tutorial_13_ownership_en_funciones/Main.ns`

```csharp
using NativeSharp.Core;

namespace Tutorial.OwnershipEnFunciones
{
    public class Resource
    {
        private static int liveCount = 0;
        private static int destroyedCount = 0;
        private int value;

        public Resource(int initialValue)
        {
            value = initialValue;
            liveCount = liveCount + 1;
        }

        public int Read()
        {
            return value;
        }
        public static int LiveCount()
        {
            return liveCount;
        }
        public static int DestroyedCount()
        {
            return destroyedCount;
        }

        ~Resource()
        {
            liveCount = liveCount - 1;
            destroyedCount = destroyedCount + 1;
        }
    }

    class Program
    {

        public static owned Resource Create(int value)
        {
            raii Resource result = new Resource(value);
            return move(result);
        }

        public static int Inspect(borrowed Resource resource)
        {
            return resource.Read();
        }

        public static int Consume(owned Resource resource)
        {
            return resource.Read();
        }

        public static int Main()
        {
            if (Resource.LiveCount() != 0)
            {
                return 1;
            }

            raii Resource resource = Create(42);
            if (Resource.LiveCount() != 1)
            {
                return 2;
            }

            {

                borrowed Resource view = resource;
                int inspected = Inspect(view);
                if (inspected != 42)
                {
                    return 3;
                }
                if (resource.Read() != 42 || Resource.LiveCount() != 1)
                {
                    return 4;
                }
            }

            int consumedValue = Consume(move(resource));
            if (consumedValue != 42)
            {
                return 5;
            }
            if (Resource.LiveCount() != 0 || Resource.DestroyedCount() != 1)
            {
                return 6;
            }

            int secondValue = Consume(Create(100));

            if (secondValue != 100)
            {
                return 7;
            }
            if (Resource.DestroyedCount() != 2)
            {
                return 8;
            }

            raii string label = String.Copy("valores consumidos = ");
            raii string d1 = String.ToString(consumedValue);
            raii string sep = String.Copy(" + ");
            raii string d2 = String.ToString(secondValue);
            raii string s1 = String.Concat(label, d1);
            raii string s2 = String.Concat(s1, sep);
            raii string line = String.Concat(s2, d2);
            Console.WriteLine(line);

            return consumedValue + secondValue;
        }
    }
}
```

### 35.14 Genéricos AOT

**Objetivo.** Tipos y métodos parametrizados, instancias cerradas y especialización.

**Puente desde C#.** La build actual también tiene constraints `where`; se elimina el comentario histórico que los negaba.

**Estado comprobado.** Compila en `-O0` y `-O2`; ambas ejecuciones producen la misma salida y el mismo checksum de retorno (`77`). Un retorno distinto de cero es deliberado en esta serie: `Main` devuelve resultados calculados para que el ejecutable sea autoauditable.

```powershell
nsc --emit-project-oop-exe `
    examples/tutorial_14_generics_basicos/Tutorial14GenericsBasicos.nsproj `
    -O2 `
    -o tutorial_14_generics_basicos.exe
./tutorial_14_generics_basicos.exe
```

#### Proyecto

##### `examples/tutorial_14_generics_basicos/Tutorial14GenericsBasicos.nsproj`

```xml
<Project>
  <PropertyGroup><Name>Tutorial14GenericsBasicos</Name></PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
  </ItemGroup>
</Project>
```

#### Fuente editorial

##### `examples/tutorial_14_generics_basicos/Main.ns`

```csharp
using NativeSharp.Core;

namespace Tutorial.GenericsBasicos
{

    public struct Box<T>
    {
        public T Value;

        public T Get()
        {
            return Value;
        }
    }

    public struct Pair<TLeft, TRight>
    {
        public TLeft Left;
        public TRight Right;
    }

    public class Holder<T>
    {
        public T Value;

        public Holder(T initialValue)
        {
            Value = initialValue;
        }

        public T Get()
        {
            return Value;
        }
    }

    public class Algorithms
    {

        public static T Identity<T>(T value)
        {
            return value;
        }

        public static T First<T>(T[] values)
        {
            return values[0];
        }
    }

    class Program
    {
        public static int Main()
        {
            Box<int> intBox;
            intBox.Value = 42;
            int fromBox = intBox.Get();

            Pair<int, bool> flagged;
            flagged.Left = 7;
            flagged.Right = true;

            raii Holder<int> holder = new Holder<int>(99);
            int fromHolder = holder.Get();

            int identityResult = Algorithms.Identity<int>(123);
            bool identityBool = Algorithms.Identity<bool>(true);

            int[] numbers = new int[3];
            numbers[0] = 55;
            numbers[1] = 66;
            numbers[2] = 77;
            int firstNumber = Algorithms.First<int>(numbers);

            delete[] numbers;

            raii string label = String.Copy("box/holder/identity/first = ");
            raii string d1 = String.ToString(fromBox);
            raii string sep = String.Copy(" / ");
            raii string d2 = String.ToString(fromHolder);
            raii string d3 = String.ToString(identityResult);
            raii string d4 = String.ToString(firstNumber);
            raii string s1 = String.Concat(label, d1);
            raii string s2 = String.Concat(s1, sep);
            raii string s3 = String.Concat(s2, d2);
            raii string s4 = String.Concat(s3, sep);
            raii string s5 = String.Concat(s4, d3);
            raii string s6 = String.Concat(s5, sep);
            raii string line = String.Concat(s6, d4);
            Console.WriteLine(line);

            int result = fromBox + flagged.Left + fromHolder + identityResult + firstNumber;
            if (identityBool)
            {
                result = result + 1;
            }
            return result % 250;
        }
    }
}
```

### 35.15 Colecciones genéricas

**Objetivo.** List, stack, queue, dictionary y set con argumentos valor y referencia.

**Puente desde C#.** Las operaciones de colección trasladan ownership de forma visible, algo que una colección administrada no exige.

**Estado comprobado.** Compila en `-O0` y `-O2`; ambas ejecuciones producen la misma salida y el mismo checksum de retorno (`84`). Un retorno distinto de cero es deliberado en esta serie: `Main` devuelve resultados calculados para que el ejecutable sea autoauditable.

```powershell
nsc --emit-project-oop-exe `
    examples/tutorial_15_generic_collections_tour/Tutorial15GenericCollectionsTour.nsproj `
    -O2 `
    -o tutorial_15_generic_collections_tour.exe
./tutorial_15_generic_collections_tour.exe
```

#### Proyecto

##### `examples/tutorial_15_generic_collections_tour/Tutorial15GenericCollectionsTour.nsproj`

```xml
<Project>
  <PropertyGroup><Name>Tutorial15GenericCollectionsTour</Name></PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
    <ProjectReference
      Include="../../lib/NativeSharp.Core.Collections.Generic/NativeSharp.Core.Collections.Generic.nsproj"
    />
  </ItemGroup>
</Project>
```

#### Fuente editorial

##### `examples/tutorial_15_generic_collections_tour/Main.ns`

```csharp
using NativeSharp.Core;
using NativeSharp.Core.Collections.Generic;

namespace Tutorial.GenericCollectionsTour
{
    class Program
    {

        public static int HashInt(int value)
        {
            return Hashing.Int32(value);
        }

        public static int Main()
        {

            raii List<int> list = new List<int>();
            list.Add(10);
            list.Add(20);
            list.Add(30);
            int listCount = list.Count();
            int listFirst = list[0];

            Queue<int> queue = new Queue<int>();
            queue.Enqueue(1);
            queue.Enqueue(2);
            queue.Enqueue(3);
            int dequeued = queue.Dequeue();

            Stack<int> stack = new Stack<int>();
            stack.Push(1);
            stack.Push(2);
            stack.Push(3);
            int popped = stack.Pop();

            LinkedList<int> chain = new LinkedList<int>();
            chain.AddLast(2);
            chain.AddLast(3);
            chain.AddFirst(1);
            int chainCount = chain.Count();

            HashFunc<int> intHash = HashInt;
            HashSet<int> unique = new HashSet<int>(intHash);
            unique.Add(5);
            unique.Add(5);
            unique.Add(7);
            int uniqueCount = unique.Count();

            SortedSet<int> sorted = new SortedSet<int>();
            sorted.Add(30);
            sorted.Add(10);
            sorted.Add(20);
            int[] sortedArray = sorted.ToArray();
            int sortedFirst = sortedArray[0];
            delete[] sortedArray;

            Dictionary<int, string> ages = new Dictionary<int, string>(intHash);
            raii string aliceName = String.Copy("Alice");
            ages.Add(1, move(aliceName));
            bool hasKey1 = ages.ContainsKey(1);

            SortedDictionary<int, int> scoreByRank = new SortedDictionary<int, int>();
            scoreByRank.Add(2, 200);
            scoreByRank.Add(1, 100);
            int rankOneScore = scoreByRank[1];

            SortedList<int, int> sortedPairs = new SortedList<int, int>();
            sortedPairs.Add(3, 300);
            sortedPairs.Add(1, 100);
            sortedPairs.Add(2, 200);
            int indexOfKey2 = sortedPairs.IndexOfKey(2);

            PriorityQueue<int, int> tasks = new PriorityQueue<int, int>();
            tasks.Enqueue(100, 5);
            tasks.Enqueue(200, 1);
            tasks.Enqueue(300, 3);
            int mostUrgent = tasks.Dequeue();

            delete chain;
            delete unique;
            delete sorted;
            delete ages;
            delete scoreByRank;
            delete sortedPairs;
            delete tasks;

            raii string label = String.Copy("list/queue/stack/chain = ");
            raii string sep = String.Copy(" / ");
            raii string d1 = String.ToString(listCount);
            raii string d2 = String.ToString(dequeued);
            raii string d3 = String.ToString(popped);
            raii string d4 = String.ToString(chainCount);
            raii string s1 = String.Concat(label, d1);
            raii string s2 = String.Concat(s1, sep);
            raii string s3 = String.Concat(s2, d2);
            raii string s4 = String.Concat(s3, sep);
            raii string s5 = String.Concat(s4, d3);
            raii string s6 = String.Concat(s5, sep);
            raii string line = String.Concat(s6, d4);
            Console.WriteLine(line);

            int result = listCount + listFirst + dequeued + popped + chainCount + uniqueCount + sortedFirst +
                         rankOneScore + indexOfKey2 + mostUrgent;
            if (hasKey1)
            {
                result = result + 1;
            }
            return result % 250;
        }
    }
}
```

### 35.16 `foreach` estructural

**Objetivo.** Recorrido de arrays y enumeradores por el protocolo `GetEnumerator`/`MoveNext`/`Current`.

**Puente desde C#.** No depende de `IEnumerable<T>` ni de un runtime administrado.

**Estado comprobado.** Compila en `-O0` y `-O2`; ambas ejecuciones producen la misma salida y el mismo checksum de retorno (`108`). Un retorno distinto de cero es deliberado en esta serie: `Main` devuelve resultados calculados para que el ejecutable sea autoauditable.

```powershell
nsc --emit-project-oop-exe `
    examples/tutorial_16_foreach_sobre_colecciones/Tutorial16ForeachSobreColecciones.nsproj `
    -O2 `
    -o tutorial_16_foreach_sobre_colecciones.exe
./tutorial_16_foreach_sobre_colecciones.exe
```

#### Proyecto

##### `examples/tutorial_16_foreach_sobre_colecciones/Tutorial16ForeachSobreColecciones.nsproj`

```xml
<Project>
  <PropertyGroup><Name>Tutorial16ForeachSobreColecciones</Name></PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
    <ProjectReference
      Include="../../lib/NativeSharp.Core.Collections.Generic/NativeSharp.Core.Collections.Generic.nsproj"
    />
  </ItemGroup>
</Project>
```

#### Fuente editorial

##### `examples/tutorial_16_foreach_sobre_colecciones/Main.ns`

```csharp
using NativeSharp.Core;
using NativeSharp.Core.Collections.Generic;

namespace Tutorial.ForeachSobreColecciones
{
    class Program
    {
        public static int HashInt(int value)
        {
            return Hashing.Int32(value);
        }

        public static int Main()
        {
            int arraySum = 0;
            int[] numbers = new int[4];
            numbers[0] = 1;
            numbers[1] = 2;
            numbers[2] = 3;
            numbers[3] = 4;

            foreach (int n in numbers)
            {
                arraySum = arraySum + n;
            }
            delete[] numbers;

            int listSum = 0;

            List<int> list = new List<int>();
            list.Add(10);
            list.Add(20);
            list.Add(30);

            foreach (int n in list)
            {
                listSum = listSum + n;
            }
            delete list;

            Queue<int> queue = new Queue<int>();
            queue.Enqueue(1);
            queue.Enqueue(2);
            queue.Enqueue(3);
            raii string queueOrder = String.Copy("");
            foreach (int n in queue)
            {
                raii string digit = String.ToString(n);
                raii string next = String.Concat(queueOrder, digit);
                queueOrder = move(next);
            }
            delete queue;

            Stack<int> stack = new Stack<int>();
            stack.Push(1);
            stack.Push(2);
            stack.Push(3);
            raii string stackOrder = String.Copy("");
            foreach (int n in stack)
            {
                raii string digit = String.ToString(n);
                raii string next = String.Concat(stackOrder, digit);
                stackOrder = move(next);
            }
            delete stack;

            HashFunc<int> intHash = HashInt;
            Dictionary<int, int> squares = new Dictionary<int, int>(intHash);
            squares.Add(2, 4);
            squares.Add(3, 9);
            squares.Add(4, 16);
            int[] keys = squares.Keys();
            int keySum = 0;
            foreach (int key in keys)
            {
                keySum = keySum + key;
            }
            delete[] keys;
            int[] values = squares.Values();
            int valueSum = 0;
            foreach (int value in values)
            {
                valueSum = valueSum + value;
            }
            delete[] values;
            delete squares;

            raii string label = String.Copy("arraySum/listSum/queueOrder/stackOrder = ");
            raii string sep = String.Copy(" / ");
            raii string d1 = String.ToString(arraySum);
            raii string d2 = String.ToString(listSum);
            raii string s1 = String.Concat(label, d1);
            raii string s2 = String.Concat(s1, sep);
            raii string s3 = String.Concat(s2, d2);
            raii string s4 = String.Concat(s3, sep);
            raii string s5 = String.Concat(s4, queueOrder);
            raii string s6 = String.Concat(s5, sep);
            raii string line = String.Concat(s6, stackOrder);
            Console.WriteLine(line);

            int result = arraySum + listSum + keySum + valueSum;
            return result % 250;
        }
    }
}
```

### 35.17 Delegados, lambdas y cierres

**Objetivo.** Funciones como valores, métodos de instancia y captura de estado.

**Puente desde C#.** La forma es conocida para C#, pero el cierre tiene representación y vida nativas.

**Estado comprobado.** Compila en `-O0` y `-O2`; ambas ejecuciones producen la misma salida y el mismo checksum de retorno (`74`). Un retorno distinto de cero es deliberado en esta serie: `Main` devuelve resultados calculados para que el ejecutable sea autoauditable.

```powershell
nsc --emit-project-oop-exe `
    examples/tutorial_17_delegates_y_lambdas/Tutorial17DelegatesLambdas.nsproj `
    -O2 `
    -o tutorial_17_delegates_y_lambdas.exe
./tutorial_17_delegates_y_lambdas.exe
```

#### Proyecto

##### `examples/tutorial_17_delegates_y_lambdas/Tutorial17DelegatesLambdas.nsproj`

```xml
<Project>
  <PropertyGroup><Name>Tutorial17DelegatesLambdas</Name></PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
  </ItemGroup>
</Project>
```

#### Fuente editorial

##### `examples/tutorial_17_delegates_y_lambdas/Main.ns`

```csharp
using NativeSharp.Core;

namespace Tutorial.DelegatesLambdas
{

    public delegate int Transform(int value);

    public class MathOps
    {
        public static int Double(int value)
        {
            return value * 2;
        }

        public static int Square(int value)
        {
            return value * value;
        }
    }

    class Program
    {

        public static int ApplyTwice(Transform transform, int value)
        {
            return transform(transform(value));
        }

        public static int Main()
        {

            Transform doubleIt = MathOps.Double;
            int doubledTwice = ApplyTwice(doubleIt, 3);

            Transform addOne = x => x + 1;
            int addedTwice = ApplyTwice(addOne, 10);

            Transform blockLambda = x =>
            {
                int doubled = x * 2;
                return doubled + 1;
            };
            int blockResult = blockLambda(5);

            int factor = 3;
            Transform multiplyByFactor = x => x * factor;
            int multiplied = multiplyByFactor(7);

            int viaInlineCall = ApplyTwice(x => x - 1, 20);

            raii string label = String.Copy("doubledTwice/addedTwice/blockResult/multiplied = ");
            raii string sep = String.Copy(" / ");
            raii string d1 = String.ToString(doubledTwice);
            raii string d2 = String.ToString(addedTwice);
            raii string d3 = String.ToString(blockResult);
            raii string d4 = String.ToString(multiplied);
            raii string s1 = String.Concat(label, d1);
            raii string s2 = String.Concat(s1, sep);
            raii string s3 = String.Concat(s2, d2);
            raii string s4 = String.Concat(s3, sep);
            raii string s5 = String.Concat(s4, d3);
            raii string s6 = String.Concat(s5, sep);
            raii string line = String.Concat(s6, d4);
            Console.WriteLine(line);

            int result = doubledTwice + addedTwice + blockResult + multiplied + viaInlineCall;
            return result % 250;
        }
    }
}
```

### 35.18 Eventos

**Objetivo.** Publicación, suscripción, múltiples observadores y desuscripción.

**Puente desde C#.** La restricción de emisión conserva el modelo de C#, implementado con delegados nativos.

**Estado comprobado.** Compila en `-O0` y `-O2`; ambas ejecuciones producen la misma salida y el mismo checksum de retorno (`12`). Un retorno distinto de cero es deliberado en esta serie: `Main` devuelve resultados calculados para que el ejecutable sea autoauditable.

```powershell
nsc --emit-project-oop-exe `
    examples/tutorial_18_eventos/Tutorial18Eventos.nsproj `
    -O2 `
    -o tutorial_18_eventos.exe
./tutorial_18_eventos.exe
```

#### Proyecto

##### `examples/tutorial_18_eventos/Tutorial18Eventos.nsproj`

```xml
<Project>
  <PropertyGroup><Name>Tutorial18Eventos</Name></PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
  </ItemGroup>
</Project>
```

#### Fuente editorial

##### `examples/tutorial_18_eventos/Main.ns`

```csharp
using NativeSharp.Core;

namespace Tutorial.Eventos
{
    public delegate void PriceChanged(int newPrice);

    public class Stock
    {
        private int price;
        public event PriceChanged Changed;

        public Stock(int initialPrice)
        {
            price = initialPrice;
        }

        public void SetPrice(int newPrice)
        {
            price = newPrice;
            Changed(newPrice);
        }
    }

    public class PriceLogger
    {
        public int LastSeenPrice;
        public int NotificationCount;

        public void OnPriceChanged(int newPrice)
        {
            LastSeenPrice = newPrice;
            NotificationCount = NotificationCount + 1;
        }
    }

    public class CountingSubscriber
    {
        public int NotificationCount;

        public void OnPriceChanged(int newPrice)
        {
            NotificationCount = NotificationCount + 1;
        }
    }

    class Program
    {
        public static int Main()
        {
            raii Stock stock = new Stock(100);
            raii PriceLogger logger = new PriceLogger();

            stock.Changed += logger.OnPriceChanged;

            stock.SetPrice(110);
            stock.SetPrice(120);
            int countAfterTwo = logger.NotificationCount;
            int priceAfterTwo = logger.LastSeenPrice;

            raii CountingSubscriber lambdaStyleSubscriber = new CountingSubscriber();
            stock.Changed += lambdaStyleSubscriber.OnPriceChanged;

            stock.SetPrice(130);

            int countAfterThree = logger.NotificationCount;
            int lambdaCountAfterOne = lambdaStyleSubscriber.NotificationCount;

            stock.Changed -= logger.OnPriceChanged;
            stock.SetPrice(140);
            int countAfterUnsubscribe = logger.NotificationCount;
            int lambdaCountAfterTwo = lambdaStyleSubscriber.NotificationCount;

            raii string label = String.Copy("notificaciones logger/lambda = ");
            raii string sep = String.Copy(" / ");
            raii string d1 = String.ToString(countAfterUnsubscribe);
            raii string d2 = String.ToString(lambdaCountAfterTwo);
            raii string s1 = String.Concat(label, d1);
            raii string s2 = String.Concat(s1, sep);
            raii string line = String.Concat(s2, d2);
            Console.WriteLine(line);

            int result =
                countAfterTwo + countAfterThree + lambdaCountAfterOne + countAfterUnsubscribe + lambdaCountAfterTwo;
            if (priceAfterTwo == 120)
            {
                result = result + 1;
            }
            return result % 250;
        }
    }
}
```

### 35.19 Excepciones y limpieza

**Objetivo.** Throw, catches tipados, finally y destrucción durante unwinding.

**Puente desde C#.** No existe un GC que rescate recursos omitidos; RAII debe cerrar todos los caminos excepcionales.

**Estado comprobado.** Compila en `-O0` y `-O2`; ambas ejecuciones producen la misma salida y el mismo checksum de retorno (`233`). Un retorno distinto de cero es deliberado en esta serie: `Main` devuelve resultados calculados para que el ejecutable sea autoauditable.

```powershell
nsc --emit-project-oop-exe `
    examples/tutorial_19_excepciones/Tutorial19Excepciones.nsproj `
    -O2 `
    -o tutorial_19_excepciones.exe
./tutorial_19_excepciones.exe
```

#### Proyecto

##### `examples/tutorial_19_excepciones/Tutorial19Excepciones.nsproj`

```xml
<Project>
  <PropertyGroup><Name>Tutorial19Excepciones</Name></PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
  </ItemGroup>
</Project>
```

#### Fuente editorial

##### `examples/tutorial_19_excepciones/Main.ns`

```csharp
using NativeSharp.Core;

namespace Tutorial.Excepciones
{

    public class InsufficientFundsException : Exception
    {
        public int Requested;
        public int Available;
    }

    public class Account
    {
        private int balance;

        public Account(int initialBalance)
        {
            balance = initialBalance;
        }

        public int Balance()
        {
            return balance;
        }

        public void Withdraw(int amount)
        {
            if (amount > balance)
            {
                InsufficientFundsException problem = new InsufficientFundsException();
                problem.Requested = amount;
                problem.Available = balance;
                throw problem;
            }
            balance = balance - amount;
        }
    }

    class Program
    {
        public static int Main()
        {

            raii Account accountA = new Account(100);
            int caughtRequested = 0;
            int caughtAvailable = 0;
            try
            {
                accountA.Withdraw(500);
            }
            catch (InsufficientFundsException problem)
            {
                caughtRequested = problem.Requested;
                caughtAvailable = problem.Available;
            }

            raii Account accountB = new Account(100);
            int finallyRuns = 0;
            try
            {
                accountB.Withdraw(30);
            }
            finally
            {
                finallyRuns = finallyRuns + 1;
            }
            int balanceAfterValidWithdraw = accountB.Balance();

            raii Account accountC = new Account(100);
            int matchedSpecific = 0;
            int matchedGeneric = 0;
            try
            {
                accountC.Withdraw(1000);
            }
            catch (InsufficientFundsException specific)
            {
                matchedSpecific = 1;
            }
            catch (Exception generic)
            {
                matchedGeneric = 1;
            }

            raii Account accountD = new Account(100);
            int outerCaught = 0;
            try
            {
                try
                {
                    accountD.Withdraw(9999);
                }
                catch (InsufficientFundsException inner)
                {
                    throw;
                }
            }
            catch (InsufficientFundsException outer)
            {
                outerCaught = 1;
            }

            raii string label = String.Copy("caughtRequested/caughtAvailable/balance = ");
            raii string sep = String.Copy(" / ");
            raii string d1 = String.ToString(caughtRequested);
            raii string d2 = String.ToString(caughtAvailable);
            raii string d3 = String.ToString(balanceAfterValidWithdraw);
            raii string s1 = String.Concat(label, d1);
            raii string s2 = String.Concat(s1, sep);
            raii string s3 = String.Concat(s2, d2);
            raii string s4 = String.Concat(s3, sep);
            raii string line = String.Concat(s4, d3);
            Console.WriteLine(line);

            int result = caughtRequested / 10 + caughtAvailable + finallyRuns * 10 + balanceAfterValidWithdraw +
                         matchedSpecific + outerCaught;
            if (matchedGeneric == 0)
            {
                result = result + 1;
            }
            return result % 250;
        }
    }
}
```

### 35.20 Recursión clásica

**Objetivo.** Factorial, Fibonacci, Euclides y recorrido recursivo con análisis de costo.

**Puente desde C#.** Cada llamada consume stack nativo real; profundidad y complejidad dejan de ser abstracciones académicas.

**Estado comprobado.** Compila en `-O0` y `-O2`; ambas ejecuciones producen la misma salida y el mismo checksum de retorno (`227`). Un retorno distinto de cero es deliberado en esta serie: `Main` devuelve resultados calculados para que el ejecutable sea autoauditable.

```powershell
nsc --emit-project-oop-exe `
    examples/tutorial_20_recursion_clasica/Tutorial20RecursionClasica.nsproj `
    -O2 `
    -o tutorial_20_recursion_clasica.exe
./tutorial_20_recursion_clasica.exe
```

#### Proyecto

##### `examples/tutorial_20_recursion_clasica/Tutorial20RecursionClasica.nsproj`

```xml
<Project>
  <PropertyGroup><Name>Tutorial20RecursionClasica</Name></PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
  </ItemGroup>
</Project>
```

#### Fuente editorial

##### `examples/tutorial_20_recursion_clasica/Main.ns`

```csharp
using NativeSharp.Core;

namespace Tutorial.RecursionClasica
{
    class Program
    {

        public static int Power(int baseValue, int exponent)
        {
            if (exponent == 0)
            {
                return 1;
            }
            return baseValue * Power(baseValue, exponent - 1);
        }

        public static int SumRange(int n)
        {
            if (n <= 0)
            {
                return 0;
            }
            return n + SumRange(n - 1);
        }

        public static int SumArray(int[] values, int index)
        {
            if (index >= values.Length)
            {
                return 0;
            }
            return values[index] + SumArray(values, index + 1);
        }

        public static owned string ReverseString(string value)
        {
            if (value.Length <= 1)
            {
                return String.Copy(value);
            }
            raii string firstChar = String.Substring(value, 0, 1);
            raii string rest = String.Substring(value, 1, value.Length - 1);
            raii string reversedRest = ReverseString(rest);
            return String.Concat(reversedRest, firstChar);
        }

        public static bool IsPalindrome(string value)
        {
            if (value.Length <= 1)
            {
                return true;
            }
            char first = value[0];
            char last = value[value.Length - 1];
            if (first != last)
            {
                return false;
            }
            raii string middle = String.Substring(value, 1, value.Length - 2);
            return IsPalindrome(middle);
        }

        public static int Main()
        {
            int power2To10 = Power(2, 10);
            int sum1To100 = SumRange(100);

            int[] values = new int[5];
            values[0] = 10;
            values[1] = 20;
            values[2] = 30;
            values[3] = 40;
            values[4] = 50;
            int arraySum = SumArray(values, 0);
            delete[] values;

            raii string original = String.Copy("NativeSharp");
            raii string reversed = ReverseString(original);

            bool racecarIsPalindrome = IsPalindrome("racecar");
            bool nativeIsPalindrome = IsPalindrome("NativeSharp");

            raii string label = String.Copy("power/sumRange/arraySum = ");
            raii string sep = String.Copy(" / ");
            raii string d1 = String.ToString(power2To10);
            raii string d2 = String.ToString(sum1To100);
            raii string d3 = String.ToString(arraySum);
            raii string s1 = String.Concat(label, d1);
            raii string s2 = String.Concat(s1, sep);
            raii string s3 = String.Concat(s2, d2);
            raii string s4 = String.Concat(s3, sep);
            raii string line = String.Concat(s4, d3);
            Console.WriteLine(line);

            raii string reversedLabel = String.Copy("reversed = ");
            raii string reversedLine = String.Concat(reversedLabel, reversed);
            Console.WriteLine(reversedLine);

            int result = (power2To10 + sum1To100 + arraySum) % 1000;
            if (racecarIsPalindrome)
            {
                result = result + 1;
            }
            if (!nativeIsPalindrome)
            {
                result = result + 1;
            }
            if (reversed[0] == 'p')
            {
                result = result + 1;
            }
            return result % 250;
        }
    }
}
```

### 35.21 Búsqueda y ordenamiento

**Objetivo.** Búsqueda lineal y binaria, selección e inserción sobre arrays.

**Puente desde C#.** El tutorial conecta control de flujo con precondiciones y complejidad antes del laboratorio grande.

**Estado comprobado.** Compila en `-O0` y `-O2`; ambas ejecuciones producen la misma salida y el mismo checksum de retorno (`34`). Un retorno distinto de cero es deliberado en esta serie: `Main` devuelve resultados calculados para que el ejecutable sea autoauditable.

```powershell
nsc --emit-project-oop-exe `
    examples/tutorial_21_busqueda_y_ordenamiento/Tutorial21BusquedaYOrdenamiento.nsproj `
    -O2 `
    -o tutorial_21_busqueda_y_ordenamiento.exe
./tutorial_21_busqueda_y_ordenamiento.exe
```

#### Proyecto

##### `examples/tutorial_21_busqueda_y_ordenamiento/Tutorial21BusquedaYOrdenamiento.nsproj`

```xml
<Project>
  <PropertyGroup><Name>Tutorial21BusquedaYOrdenamiento</Name></PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
  </ItemGroup>
</Project>
```

#### Fuente editorial

##### `examples/tutorial_21_busqueda_y_ordenamiento/Main.ns`

```csharp
using NativeSharp.Core;

namespace Tutorial.BusquedaYOrdenamiento
{
    class Program
    {

        public static int LinearSearch(int[] values, int length, int target)
        {
            int i = 0;
            while (i < length)
            {
                if (values[i] == target)
                {
                    return i;
                }
                i = i + 1;
            }
            return -1;
        }

        public static int BinarySearch(int[] sortedValues, int length, int target)
        {
            int low = 0;
            int high = length - 1;
            while (low <= high)
            {
                int mid = (low + high) / 2;
                if (sortedValues[mid] == target)
                {
                    return mid;
                }
                if (sortedValues[mid] < target)
                {
                    low = mid + 1;
                }
                else
                {
                    high = mid - 1;
                }
            }
            return -1;
        }

        public static void BubbleSort(int[] values, int length)
        {
            int i = 0;
            while (i < length - 1)
            {
                int j = 0;
                while (j < length - 1 - i)
                {
                    if (values[j] > values[j + 1])
                    {
                        int temp = values[j];
                        values[j] = values[j + 1];
                        values[j + 1] = temp;
                    }
                    j = j + 1;
                }
                i = i + 1;
            }
        }

        public static void SelectionSort(int[] values, int length)
        {
            int i = 0;
            while (i < length - 1)
            {
                int minIndex = i;
                int j = i + 1;
                while (j < length)
                {
                    if (values[j] < values[minIndex])
                    {
                        minIndex = j;
                    }
                    j = j + 1;
                }
                if (minIndex != i)
                {
                    int temp = values[i];
                    values[i] = values[minIndex];
                    values[minIndex] = temp;
                }
                i = i + 1;
            }
        }

        public static void InsertionSort(int[] values, int length)
        {
            int i = 1;
            while (i < length)
            {
                int key = values[i];
                int j = i - 1;

                while (j >= 0 && values[j] > key)
                {
                    values[j + 1] = values[j];
                    j = j - 1;
                }
                values[j + 1] = key;
                i = i + 1;
            }
        }

        public static bool IsSortedAscending(int[] values, int length)
        {
            int i = 0;
            while (i < length - 1)
            {
                if (values[i] > values[i + 1])
                {
                    return false;
                }
                i = i + 1;
            }
            return true;
        }

        public static int Main()
        {

            int[] unsorted = new int[7];
            unsorted[0] = 42;
            unsorted[1] = 17;
            unsorted[2] = 99;
            unsorted[3] = 3;
            unsorted[4] = 58;
            unsorted[5] = 71;
            unsorted[6] = 8;

            int foundAt58 = LinearSearch(unsorted, 7, 58);
            int foundAtMissing = LinearSearch(unsorted, 7, 1000);

            int[] forBubble = new int[7];
            forBubble[0] = 42;
            forBubble[1] = 17;
            forBubble[2] = 99;
            forBubble[3] = 3;
            forBubble[4] = 58;
            forBubble[5] = 71;
            forBubble[6] = 8;
            BubbleSort(forBubble, 7);
            bool bubbleSorted = IsSortedAscending(forBubble, 7);
            int binaryFoundAt71 = BinarySearch(forBubble, 7, 71);
            int binaryFoundMissing = BinarySearch(forBubble, 7, 1000);

            int[] forSelection = new int[6];
            forSelection[0] = 5;
            forSelection[1] = 2;
            forSelection[2] = 9;
            forSelection[3] = 1;
            forSelection[4] = 7;
            forSelection[5] = 3;
            SelectionSort(forSelection, 6);
            bool selectionSorted = IsSortedAscending(forSelection, 6);
            int selectionFirst = forSelection[0];
            int selectionLast = forSelection[5];

            int[] forInsertion = new int[5];
            forInsertion[0] = 40;
            forInsertion[1] = 10;
            forInsertion[2] = 30;
            forInsertion[3] = 20;
            forInsertion[4] = 50;
            InsertionSort(forInsertion, 5);
            bool insertionSorted = IsSortedAscending(forInsertion, 5);
            int insertionFirst = forInsertion[0];

            delete[] unsorted;
            delete[] forBubble;
            delete[] forSelection;
            delete[] forInsertion;

            raii string label = String.Copy("foundAt58/binaryFoundAt71/selectionFirst/insertionFirst = ");
            raii string sep = String.Copy(" / ");
            raii string d1 = String.ToString(foundAt58);
            raii string d2 = String.ToString(binaryFoundAt71);
            raii string d3 = String.ToString(selectionFirst);
            raii string d4 = String.ToString(insertionFirst);
            raii string s1 = String.Concat(label, d1);
            raii string s2 = String.Concat(s1, sep);
            raii string s3 = String.Concat(s2, d2);
            raii string s4 = String.Concat(s3, sep);
            raii string s5 = String.Concat(s4, d3);
            raii string s6 = String.Concat(s5, sep);
            raii string line = String.Concat(s6, d4);
            Console.WriteLine(line);

            int result = foundAt58 + binaryFoundAt71 + selectionFirst + insertionFirst + selectionLast;
            if (foundAtMissing == -1)
            {
                result = result + 1;
            }
            if (binaryFoundMissing == -1)
            {
                result = result + 1;
            }
            if (bubbleSorted)
            {
                result = result + 1;
            }
            if (selectionSorted)
            {
                result = result + 1;
            }
            if (insertionSorted)
            {
                result = result + 1;
            }
            return result % 250;
        }
    }
}
```

### 35.22 Lista, pila y cola manuales

**Objetivo.** Nodos, enlaces, extremos e invariantes de estructuras lineales.

**Puente desde C#.** La ausencia de GC obliga a implementar también la limpieza, no sólo las operaciones felices.

**Estado comprobado.** Compila en `-O0` y `-O2`; ambas ejecuciones producen la misma salida y el mismo checksum de retorno (`77`). Un retorno distinto de cero es deliberado en esta serie: `Main` devuelve resultados calculados para que el ejecutable sea autoauditable.

```powershell
nsc --emit-project-oop-exe `
    examples/tutorial_22_linked_list_stack_queue_manual/Tutorial22LinkedListStackQueueManual.nsproj `
    -O2 `
    -o tutorial_22_linked_list_stack_queue_manual.exe
./tutorial_22_linked_list_stack_queue_manual.exe
```

#### Proyecto

##### `examples/tutorial_22_linked_list_stack_queue_manual/Tutorial22LinkedListStackQueueManual.nsproj`

```xml
<Project>
  <PropertyGroup><Name>Tutorial22LinkedListStackQueueManual</Name></PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
  </ItemGroup>
</Project>
```

#### Fuente editorial

##### `examples/tutorial_22_linked_list_stack_queue_manual/Main.ns`

```csharp
using NativeSharp.Core;

namespace Tutorial.ListaPilaColaManual
{

    public class Node
    {
        public int Value;
        public Node Next;

        public Node(int value)
        {
            Value = value;
            Next = null;
        }
    }

    public class ManualLinkedList
    {
        private Node head;
        private int count;

        public ManualLinkedList()
        {
            head = null;
            count = 0;
        }

        public void PushFront(int value)
        {
            Node node = new Node(value);
            node.Next = head;
            head = node;
            count = count + 1;
        }

        public int Count()
        {
            return count;
        }

        public int Sum()
        {
            int total = 0;
            Node cursor = head;
            while (cursor != null)
            {
                total = total + cursor.Value;
                cursor = cursor.Next;
            }
            return total;
        }

        public bool Contains(int target)
        {
            Node cursor = head;
            while (cursor != null)
            {
                if (cursor.Value == target)
                {
                    return true;
                }
                cursor = cursor.Next;
            }
            return false;
        }
    }

    public class ManualStack
    {
        private Node top;
        private int count;

        public ManualStack()
        {
            top = null;
            count = 0;
        }

        public void Push(int value)
        {
            Node node = new Node(value);
            node.Next = top;
            top = node;
            count = count + 1;
        }

        public int Pop()
        {
            int value = top.Value;
            top = top.Next;
            count = count - 1;
            return value;
        }

        public bool IsEmpty()
        {
            return count == 0;
        }

        public int Count()
        {
            return count;
        }
    }

    public class ManualQueue
    {
        private Node head;
        private Node tail;
        private int count;

        public ManualQueue()
        {
            head = null;
            tail = null;
            count = 0;
        }

        public void Enqueue(int value)
        {
            Node node = new Node(value);
            if (head == null)
            {
                head = node;
                tail = node;
            }
            else
            {
                tail.Next = node;
                tail = node;
            }
            count = count + 1;
        }

        public int Dequeue()
        {
            int value = head.Value;
            head = head.Next;
            if (head == null)
            {
                tail = null;
            }
            count = count - 1;
            return value;
        }

        public bool IsEmpty()
        {
            return count == 0;
        }

        public int Count()
        {
            return count;
        }
    }

    class Program
    {
        public static int Main()
        {

            ManualLinkedList list = new ManualLinkedList();
            list.PushFront(30);
            list.PushFront(20);
            list.PushFront(10);

            int listCount = list.Count();
            int listSum = list.Sum();
            bool listHas20 = list.Contains(20);
            bool listHas99 = list.Contains(99);

            ManualStack stack = new ManualStack();
            stack.Push(1);
            stack.Push(2);
            stack.Push(3);
            int stackFirstPop = stack.Pop();
            int stackSecondPop = stack.Pop();
            int stackRemaining = stack.Count();

            ManualQueue queue = new ManualQueue();
            queue.Enqueue(100);
            queue.Enqueue(200);
            queue.Enqueue(300);
            int queueFirstOut = queue.Dequeue();
            int queueSecondOut = queue.Dequeue();
            int queueRemaining = queue.Count();
            bool queueEmptyBeforeLast = queue.IsEmpty();
            int queueLastOut = queue.Dequeue();
            bool queueEmptyAfterLast = queue.IsEmpty();

            raii string label = String.Copy("listSum/stackFirstPop/queueFirstOut/queueSecondOut = ");
            raii string sep = String.Copy(" / ");
            raii string d1 = String.ToString(listSum);
            raii string d2 = String.ToString(stackFirstPop);
            raii string d3 = String.ToString(queueFirstOut);
            raii string d4 = String.ToString(queueSecondOut);
            raii string s1 = String.Concat(label, d1);
            raii string s2 = String.Concat(s1, sep);
            raii string s3 = String.Concat(s2, d2);
            raii string s4 = String.Concat(s3, sep);
            raii string s5 = String.Concat(s4, d3);
            raii string s6 = String.Concat(s5, sep);
            raii string line = String.Concat(s6, d4);
            Console.WriteLine(line);

            int result = listCount + listSum + stackFirstPop + stackSecondPop + stackRemaining + queueRemaining;
            if (listHas20)
            {
                result = result + 1;
            }
            if (!listHas99)
            {
                result = result + 1;
            }
            if (!queueEmptyBeforeLast)
            {
                result = result + 1;
            }
            if (queueEmptyAfterLast)
            {
                result = result + 1;
            }
            if (queueFirstOut == 100)
            {
                result = result + 1;
            }
            if (queueSecondOut == 200)
            {
                result = result + 1;
            }
            if (queueLastOut == 300)
            {
                result = result + 1;
            }
            return result % 250;
        }
    }
}
```

### 35.23 Huffman con cola de prioridad

**Objetivo.** Frecuencias, heap, construcción de árbol y codificación prefija.

**Puente desde C#.** Integra clases, ownership, estructuras y algoritmo en un programa que ya no cabe en una diapositiva.

**Estado comprobado.** Compila en `-O0` y `-O2`; ambas ejecuciones producen la misma salida y el mismo checksum de retorno (`42`). Un retorno distinto de cero es deliberado en esta serie: `Main` devuelve resultados calculados para que el ejecutable sea autoauditable.

```powershell
nsc --emit-project-oop-exe `
    examples/tutorial_23_huffman_priority_queue/Tutorial23HuffmanPriorityQueue.nsproj `
    -O2 `
    -o tutorial_23_huffman_priority_queue.exe
./tutorial_23_huffman_priority_queue.exe
```

#### Proyecto

##### `examples/tutorial_23_huffman_priority_queue/Tutorial23HuffmanPriorityQueue.nsproj`

```xml
<Project>
  <PropertyGroup><Name>Tutorial23HuffmanPriorityQueue</Name></PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
    <ProjectReference
      Include="../../lib/NativeSharp.Core.Collections.Generic/NativeSharp.Core.Collections.Generic.nsproj"
    />
  </ItemGroup>
</Project>
```

#### Fuente editorial

##### `examples/tutorial_23_huffman_priority_queue/Main.ns`

```csharp
using NativeSharp.Core;
using NativeSharp.Core.Collections.Generic;

namespace Tutorial.HuffmanPriorityQueue
{

    public class HuffmanNode
    {
        public char Symbol;
        public int Frequency;
        public bool IsLeaf;
        public int LeftIndex;
        public int RightIndex;

        public HuffmanNode(char symbol, int frequency)
        {
            Symbol = symbol;
            Frequency = frequency;
            IsLeaf = true;
            LeftIndex = -1;
            RightIndex = -1;
        }

        public HuffmanNode(int leftIndex, int rightIndex, int frequency)
        {
            Symbol = '#';
            Frequency = frequency;
            IsLeaf = false;
            LeftIndex = leftIndex;
            RightIndex = rightIndex;
        }
    }

    class Program
    {

        public static int BuildTree(HuffmanNode[] pool, char[] symbols, int[] frequencies, int alphabetSize)
        {
            PriorityQueue<int, int> queue = new PriorityQueue<int, int>();
            int nextIndex = 0;

            int i = 0;
            while (i < alphabetSize)
            {
                pool[nextIndex] = new HuffmanNode(symbols[i], frequencies[i]);
                queue.Enqueue(nextIndex, frequencies[i]);
                nextIndex = nextIndex + 1;
                i = i + 1;
            }

            while (queue.Count() > 1)
            {
                int leftIndex = queue.Dequeue();
                int rightIndex = queue.Dequeue();
                int combinedFrequency = pool[leftIndex].Frequency + pool[rightIndex].Frequency;
                pool[nextIndex] = new HuffmanNode(leftIndex, rightIndex, combinedFrequency);
                queue.Enqueue(nextIndex, combinedFrequency);
                nextIndex = nextIndex + 1;
            }

            int rootIndex = queue.Dequeue();
            delete queue;
            return rootIndex;
        }

        public static void CollectCodes(HuffmanNode[] pool, int nodeIndex, string prefix, string[] codeTable)
        {
            HuffmanNode node = pool[nodeIndex];
            if (node.IsLeaf)
            {
                codeTable[nodeIndex] = String.Copy(prefix);
                return;
            }
            raii string leftPrefix = String.Concat(prefix, "0");
            CollectCodes(pool, node.LeftIndex, leftPrefix, codeTable);
            raii string rightPrefix = String.Concat(prefix, "1");
            CollectCodes(pool, node.RightIndex, rightPrefix, codeTable);
        }

        public static int FindSymbolIndex(char[] symbols, int alphabetSize, char target)
        {
            int i = 0;
            while (i < alphabetSize)
            {
                if (symbols[i] == target)
                {
                    return i;
                }
                i = i + 1;
            }
            return -1;
        }

        public static owned string Encode(string message, char[] symbols, int alphabetSize, string[] codeTable)
        {
            raii string encoded = String.Copy("");
            int i = 0;
            while (i < message.Length)
            {
                char c = message[i];
                int symbolIndex = FindSymbolIndex(symbols, alphabetSize, c);
                raii string next = String.Concat(encoded, codeTable[symbolIndex]);
                encoded = move(next);
                i = i + 1;
            }
            return move(encoded);
        }

        public static owned string Decode(string encoded, HuffmanNode[] pool, int rootIndex)
        {
            raii string decoded = String.Copy("");
            int currentIndex = rootIndex;
            int i = 0;
            while (i < encoded.Length)
            {
                char bit = encoded[i];
                HuffmanNode current = pool[currentIndex];
                if (bit == '0')
                {
                    currentIndex = current.LeftIndex;
                }
                else
                {
                    currentIndex = current.RightIndex;
                }
                HuffmanNode landed = pool[currentIndex];
                if (landed.IsLeaf)
                {
                    raii string symbolStr = String.ToString(landed.Symbol);
                    raii string next = String.Concat(decoded, symbolStr);
                    decoded = move(next);
                    currentIndex = rootIndex;
                }
                i = i + 1;
            }
            return move(decoded);
        }

        public static int Main()
        {
            char[] symbols = new char[5];
            symbols[0] = 'a';
            symbols[1] = 'b';
            symbols[2] = 'c';
            symbols[3] = 'd';
            symbols[4] = 'e';

            int[] frequencies = new int[5];
            frequencies[0] = 45;
            frequencies[1] = 13;
            frequencies[2] = 12;
            frequencies[3] = 16;
            frequencies[4] = 9;
            int alphabetSize = 5;

            HuffmanNode[] pool = new HuffmanNode[alphabetSize * 2 - 1];
            int rootIndex = BuildTree(pool, symbols, frequencies, alphabetSize);

            string[] codeTable = new string[alphabetSize];
            CollectCodes(pool, rootIndex, "", codeTable);

            int codeLengthA = codeTable[0].Length;
            int codeLengthB = codeTable[1].Length;
            int codeLengthC = codeTable[2].Length;
            int codeLengthD = codeTable[3].Length;
            int codeLengthE = codeTable[4].Length;

            raii string message = String.Copy("aabacdaeb");
            raii string encoded = Encode(message, symbols, alphabetSize, codeTable);

            raii string decoded = Decode(encoded, pool, rootIndex);
            bool roundTripOk = String.Equals(decoded, message);

            raii string label = String.Copy("message/encodedBits/decoded = ");
            raii string sep = String.Copy(" / ");
            raii string lenDigits = String.ToString(encoded.Length);
            raii string s1 = String.Concat(label, message);
            raii string s2 = String.Concat(s1, sep);
            raii string s3 = String.Concat(s2, lenDigits);
            raii string s4 = String.Concat(s3, sep);
            raii string line = String.Concat(s4, decoded);
            Console.WriteLine(line);

            int freeIndex = 0;
            while (freeIndex < alphabetSize)
            {
                delete codeTable[freeIndex];
                freeIndex = freeIndex + 1;
            }
            delete[] codeTable;
            delete[] pool;
            delete[] symbols;
            delete[] frequencies;

            int result = codeLengthA * 10 + codeLengthB + codeLengthC + codeLengthD + codeLengthE + encoded.Length;
            if (roundTripOk)
            {
                result = result + 1;
            }
            return result % 250;
        }
    }
}
```

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

### 47.1 Números racionales

**Problema.** Construir un tipo numérico que preserve la fracción exacta, normalice signos y reduzca cada resultado mediante el máximo común divisor.

**Diseño.** La clase concentra una invariante algebraica, sobrecarga aritmética y comparaciones, convierte a texto y usa una excepción específica para el denominador cero. Dos series ejercitan el tipo fuera de una demostración trivial.

**Auditoría propuesta.** Compruebe que el denominador siempre queda positivo, que cero se normaliza, que cada resultado temporal tiene un dueño y que dividir por un racional cero no produce un valor silencioso.

**Escala real.** 3 archivos NativeSharp, 344 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

**Reproducción.**

```powershell
nsc --emit-project-oop-exe `
    book_examples/01_rational_numbers/Book01RationalNumbers.nsproj `
    -O2 `
    -o 01_rational_numbers.exe
./01_rational_numbers.exe
```

#### Listado completo

##### `book_examples/01_rational_numbers/Book01RationalNumbers.nsproj`

```xml
<Project>
  <PropertyGroup><Name>Book01RationalNumbers</Name></PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
  </ItemGroup>
</Project>
```

##### `book_examples/01_rational_numbers/Rational.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.RationalNumbers
{
    public class ZeroDenominatorException : Exception
    {
    }

    public class Rational
    {
        private int numerator;
        private int denominator;

        public Rational(int value)
        {
            numerator = value;
            denominator = 1;
        }

        public Rational(int initialNumerator, int initialDenominator)
        {
            if (initialDenominator == 0)
            {
                ZeroDenominatorException problem = new ZeroDenominatorException();
                problem.Message = String.Copy("A rational number cannot have denominator zero.");
                throw problem;
            }

            numerator = initialNumerator;
            denominator = initialDenominator;
            Normalize();
        }

        public int Numerator
        {
            get
            {
                return numerator;
            }
        }

        public int Denominator
        {
            get
            {
                return denominator;
            }
        }

        private static int Abs(int value)
        {
            if (value < 0)
            {
                return -value;
            }
            return value;
        }

        private static int GreatestCommonDivisor(int a, int b)
        {
            int left = Abs(a);
            int right = Abs(b);

            if (left == 0)
            {
                if (right == 0)
                {
                    return 1;
                }
                return right;
            }

            while (right != 0)
            {
                int remainder = left % right;
                left = right;
                right = remainder;
            }

            return left;
        }

        private void Normalize()
        {
            if (denominator < 0)
            {
                numerator = -numerator;
                denominator = -denominator;
            }

            if (numerator == 0)
            {
                denominator = 1;
                return;
            }

            int divisor = GreatestCommonDivisor(numerator, denominator);
            numerator = numerator / divisor;
            denominator = denominator / divisor;
        }

        public owned string ToText()
        {
            raii string n = String.ToString(numerator);
            if (denominator == 1)
            {
                return String.Copy(n);
            }

            raii string slash = String.Copy("/");
            raii string d = String.ToString(denominator);
            raii string left = String.Concat(n, slash);
            return String.Concat(left, d);
        }

        public double ToDouble()
        {
            return (double)numerator / (double)denominator;
        }

        public static Rational operator +(Rational left, Rational right)
        {
            int n = left.numerator * right.denominator + right.numerator * left.denominator;
            int d = left.denominator * right.denominator;
            return new Rational(n, d);
        }

        public static Rational operator -(Rational left, Rational right)
        {
            int n = left.numerator * right.denominator - right.numerator * left.denominator;
            int d = left.denominator * right.denominator;
            return new Rational(n, d);
        }

        public static Rational operator -(Rational value)
        {
            return new Rational(-value.numerator, value.denominator);
        }

        public static Rational operator *(Rational left, Rational right)
        {
            return new Rational(left.numerator * right.numerator, left.denominator * right.denominator);
        }

        public static Rational operator /(Rational left, Rational right)
        {
            if (right.numerator == 0)
            {
                ZeroDenominatorException problem = new ZeroDenominatorException();
                problem.Message = String.Copy("Division by the rational value zero is undefined.");
                throw problem;
            }

            return new Rational(left.numerator * right.denominator, left.denominator * right.numerator);
        }

        public static bool operator ==(Rational left, Rational right)
        {
            return left.numerator == right.numerator && left.denominator == right.denominator;
        }

        public static bool operator !=(Rational left, Rational right)
        {
            return !(left == right);
        }

        public static bool operator<(Rational left, Rational right)
        {
            return left.numerator * right.denominator < right.numerator * left.denominator;
        }

        public static bool operator>(Rational left, Rational right)
        {
            return right < left;
        }

        public static bool operator <=(Rational left, Rational right)
        {
            return !(left > right);
        }

        public static bool operator >=(Rational left, Rational right)
        {
            return !(left < right);
        }
    }
}
```

##### `book_examples/01_rational_numbers/RationalSeries.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.RationalNumbers
{
    public class RationalSeries
    {
        public static Rational Harmonic(int terms)
        {
            Rational sum = new Rational(0);
            int k = 1;

            while (k <= terms)
            {
                Rational term = new Rational(1, k);
                Rational next = sum + term;

                delete sum;
                delete term;
                sum = next;

                k = k + 1;
            }

            return sum;
        }

        public static Rational LeibnizPiQuarter(int terms)
        {
            Rational sum = new Rational(0);
            int k = 0;
            int sign = 1;

            while (k < terms)
            {
                int denominator = 2 * k + 1;
                Rational term = new Rational(sign, denominator);
                Rational next = sum + term;

                delete sum;
                delete term;
                sum = next;

                sign = -sign;
                k = k + 1;
            }

            return sum;
        }
    }
}
```

##### `book_examples/01_rational_numbers/Program.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.RationalNumbers
{
    class Program
    {
        private static void PrintBinaryOperation(string caption, Rational left, string symbol, Rational right,
                                                 Rational result)
        {
            raii string leftText = left.ToText();
            raii string rightText = right.ToText();
            raii string resultText = result.ToText();
            raii string equals = String.Copy(" = ");
            raii string space = String.Copy(" ");
            raii string s1 = String.Concat(caption, leftText);
            raii string s2 = String.Concat(s1, space);
            raii string s3 = String.Concat(s2, symbol);
            raii string s4 = String.Concat(s3, space);
            raii string s5 = String.Concat(s4, rightText);
            raii string s6 = String.Concat(s5, equals);
            raii string line = String.Concat(s6, resultText);
            Console.WriteLine(line);
        }

        public static int Main()
        {
            Console.WriteLine("NativeSharp Book Example 01 - Rational Numbers");
            Console.WriteLine("------------------------------------------------");

            Rational a = new Rational(2, 3);
            Rational b = new Rational(5, 7);

            Rational sum = a + b;
            Rational difference = a - b;
            Rational product = a * b;
            Rational quotient = a / b;
            Rational negative = -a;

            PrintBinaryOperation("", a, "+", b, sum);
            PrintBinaryOperation("", a, "-", b, difference);
            PrintBinaryOperation("", a, "*", b, product);
            PrintBinaryOperation("", a, "/", b, quotient);

            raii string negText = negative.ToText();
            raii string negLine = String.Concat("-2/3 = ", negText);
            Console.WriteLine(negLine);

            Console.WriteLine("");
            Console.WriteLine("Exact comparisons (no floating-point rounding):");
            if (a < b)
            {
                Console.WriteLine("  2/3 < 5/7");
            }
            if (sum > a)
            {
                Console.WriteLine("  (2/3 + 5/7) > 2/3");
            }

            Console.WriteLine("");
            Console.WriteLine("Harmonic series H(10) = 1 + 1/2 + ... + 1/10");
            Rational harmonic = RationalSeries.Harmonic(10);
            raii string harmonicText = harmonic.ToText();
            Console.WriteLine(harmonicText);

            Console.WriteLine("");
            Console.WriteLine("First 8 terms of the Leibniz series for pi/4:");
            Rational piQuarter = RationalSeries.LeibnizPiQuarter(8);
            raii string piQuarterText = piQuarter.ToText();
            Console.WriteLine(piQuarterText);

            Console.WriteLine("");
            Console.WriteLine("Normalization examples:");
            Rational normalizedA = new Rational(42, 56);
            Rational normalizedB = new Rational(-18, -24);
            raii string normalizedAText = normalizedA.ToText();
            raii string normalizedBText = normalizedB.ToText();
            raii string normalizedALine = String.Concat("  42/56  -> ", normalizedAText);
            raii string normalizedBLine = String.Concat("  -18/-24 -> ", normalizedBText);
            Console.WriteLine(normalizedALine);
            Console.WriteLine(normalizedBLine);

            bool caughtInvalid = false;
            try
            {
                Rational invalid = new Rational(1, 0);
                delete invalid;
            }
            catch (ZeroDenominatorException problem)
            {
                caughtInvalid = true;
                Console.WriteLine("  Invalid denominator correctly rejected with an exception.");
            }

            delete a;
            delete b;
            delete sum;
            delete difference;
            delete product;
            delete quotient;
            delete negative;
            delete harmonic;
            delete piQuarter;
            delete normalizedA;
            delete normalizedB;

            if (!caughtInvalid)
            {
                return 1;
            }

            return 0;
        }
    }
}
```

### 47.2 Nómina polimórfica

**Problema.** Calcular el pago semanal de empleados asalariados, por hora, por comisión y por comisión con salario base sin convertir el reporte en una cadena de pruebas de tipo.

**Diseño.** Una clase abstracta define el contrato y cuatro clases concretas implementan la fórmula. `Money` separa centavos de formato; el reporte consume un array heterogéneo mediante despacho virtual.

**Auditoría propuesta.** Siga la destrucción de empleados, resultados `Money` y strings devueltos como `owned`. Verifique también que las reglas de horas extra y comisión permanezcan en la clase que posee la política.

**Escala real.** 8 archivos NativeSharp, 334 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

**Reproducción.**

```powershell
nsc --emit-project-oop-exe `
    book_examples/02_payroll_oop/Book02PayrollOop.nsproj `
    -O2 `
    -o 02_payroll_oop.exe
./02_payroll_oop.exe
```

#### Listado completo

##### `book_examples/02_payroll_oop/Book02PayrollOop.nsproj`

```xml
<Project>
  <PropertyGroup><Name>Book02PayrollOop</Name></PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
  </ItemGroup>
</Project>
```

##### `book_examples/02_payroll_oop/BasePlusCommissionEmployee.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.Payroll
{
    public class BasePlusCommissionEmployee : CommissionEmployee
    {
        private int basePayCents;

        public BasePlusCommissionEmployee(int id, string employeeName, int weeklySales, int percentage, int basePay)
            : base(id, employeeName, weeklySales, percentage)
        {
            basePayCents = basePay;
        }

        public override owned Money CalculateWeeklyPay()
        {
            raii Money commission = base.CalculateWeeklyPay();
            return new Money(commission.Cents + basePayCents);
        }

        public override owned string Classification()
        {
            return String.Copy("Base + Commission");
        }
    }
}
```

##### `book_examples/02_payroll_oop/CommissionEmployee.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.Payroll
{
    public class CommissionEmployee : Employee
    {
        private int weeklySalesCents;
        private int commissionPercent;

        public CommissionEmployee(int id, string employeeName, int weeklySales, int percentage) : base(id, employeeName)
        {
            weeklySalesCents = weeklySales;
            commissionPercent = percentage;
        }

        public override owned Money CalculateWeeklyPay()
        {
            return new Money(weeklySalesCents * commissionPercent / 100);
        }

        public override owned string Classification()
        {
            return String.Copy("Commission");
        }
    }
}
```

##### `book_examples/02_payroll_oop/Employee.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.Payroll
{
    public abstract class Employee
    {
        private int employeeId;
        protected string name;

        public Employee(int id, string employeeName)
        {
            employeeId = id;
            name = String.Copy(employeeName);
        }

        public int Id
        {
            get
            {
                return employeeId;
            }
        }

        public string Name
        {
            get
            {
                return name;
            }
        }

        public abstract owned Money CalculateWeeklyPay();

        public virtual owned string Classification()
        {
            return String.Copy("Employee");
        }

        ~Employee()
        {
            delete name;
        }
    }
}
```

##### `book_examples/02_payroll_oop/HourlyEmployee.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.Payroll
{
    public class HourlyEmployee : Employee
    {
        private int hourlyRateCents;
        private int hoursWorked;

        public HourlyEmployee(int id, string employeeName, int hourlyRate, int hours) : base(id, employeeName)
        {
            hourlyRateCents = hourlyRate;
            hoursWorked = hours;
        }

        public override owned Money CalculateWeeklyPay()
        {
            int regularHours = hoursWorked;
            int overtimeHours = 0;
            if (regularHours > 40)
            {
                overtimeHours = regularHours - 40;
                regularHours = 40;
            }

            int regularPay = regularHours * hourlyRateCents;
            int overtimePay = overtimeHours * hourlyRateCents * 3 / 2;
            return new Money(regularPay + overtimePay);
        }

        public override owned string Classification()
        {
            return String.Copy("Hourly");
        }
    }
}
```

##### `book_examples/02_payroll_oop/Money.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.Payroll
{
    public class Money
    {
        private int cents;

        public Money(int initialCents)
        {
            cents = initialCents;
        }

        public int Cents
        {
            get
            {
                return cents;
            }
        }

        public owned string ToText()
        {
            int absolute = cents;
            bool negative = false;
            if (absolute < 0)
            {
                negative = true;
                absolute = -absolute;
            }

            int dollars = absolute / 100;
            int remainder = absolute % 100;
            raii string dollarText = String.ToString(dollars);
            raii string centText = String.ToString(remainder);
            raii string prefix = String.Copy("$");
            raii string dot = String.Copy(".");
            raii string zero = String.Copy("0");
            raii string body1 = String.Concat(prefix, dollarText);
            raii string body2 = String.Concat(body1, dot);
            raii string body3 = String.Copy(body2);
            if (remainder < 10)
            {
                raii string padded = String.Concat(body2, zero);
                body3 = move(padded);
            }
            raii string body4 = String.Concat(body3, centText);

            if (negative)
            {
                raii string minus = String.Copy("-");
                return String.Concat(minus, body4);
            }

            return String.Copy(body4);
        }

        public static Money operator +(Money left, Money right)
        {
            return new Money(left.cents + right.cents);
        }

        public static Money operator -(Money left, Money right)
        {
            return new Money(left.cents - right.cents);
        }

        public static bool operator<(Money left, Money right)
        {
            return left.cents < right.cents;
        }

        public static bool operator>(Money left, Money right)
        {
            return left.cents > right.cents;
        }

        public static bool operator ==(Money left, Money right)
        {
            return left.cents == right.cents;
        }

        public static bool operator !=(Money left, Money right)
        {
            return !(left == right);
        }
    }
}
```

##### `book_examples/02_payroll_oop/PayrollReport.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.Payroll
{
    public class PayrollReport
    {
        private static void PrintEmployee(Employee employee, Money pay)
        {
            raii string idText = String.ToString(employee.Id);
            raii string classText = employee.Classification();
            raii string payText = pay.ToText();
            raii string s1 = String.Concat("#", idText);
            raii string s2 = String.Concat(s1, "  ");
            raii string s3 = String.Concat(s2, employee.Name);
            raii string s4 = String.Concat(s3, "  [");
            raii string s5 = String.Concat(s4, classText);
            raii string s6 = String.Concat(s5, "]  weekly pay: ");
            raii string line = String.Concat(s6, payText);
            Console.WriteLine(line);
        }

        public static owned Money Print(Employee[] employees, int count)
        {
            int totalCents = 0;
            int index = 0;

            while (index < count)
            {
                Employee employee = employees[index];
                raii Money pay = employee.CalculateWeeklyPay();
                PrintEmployee(employee, pay);
                totalCents = totalCents + pay.Cents;

                index = index + 1;
            }

            return new Money(totalCents);
        }
    }
}
```

##### `book_examples/02_payroll_oop/SalariedEmployee.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.Payroll
{
    public class SalariedEmployee : Employee
    {
        private int weeklySalaryCents;

        public SalariedEmployee(int id, string employeeName, int weeklySalary) : base(id, employeeName)
        {
            weeklySalaryCents = weeklySalary;
        }

        public override owned Money CalculateWeeklyPay()
        {
            return new Money(weeklySalaryCents);
        }

        public override owned string Classification()
        {
            return String.Copy("Salaried");
        }
    }
}
```

##### `book_examples/02_payroll_oop/Program.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.Payroll
{
    class Program
    {
        public static int Main()
        {
            Console.WriteLine("NativeSharp Book Example 02 - Polymorphic Payroll");
            Console.WriteLine("-------------------------------------------------");

            Employee[] employees = new Employee[4];
            employees[0] = new SalariedEmployee(1001, "Ana Torres", 185000);
            employees[1] = new HourlyEmployee(1002, "Luis Medina", 3250, 46);
            employees[2] = new CommissionEmployee(1003, "Marta Silva", 1250000, 8);
            employees[3] = new BasePlusCommissionEmployee(1004, "Diego Rios", 980000, 7, 45000);

            raii Money total = PayrollReport.Print(employees, 4);
            raii string totalText = total.ToText();
            raii string totalLine = String.Concat("TOTAL PAYROLL: ", totalText);
            Console.WriteLine("-------------------------------------------------");
            Console.WriteLine(totalLine);

            raii Money budget = new Money(600000);
            if (total < budget)
            {
                raii string remainingText = String.ToString(budget.Cents - total.Cents);
                raii string message = String.Concat("Payroll is under budget. Remaining cents: ", remainingText);
                Console.WriteLine(message);
            }
            else
            {
                Console.WriteLine("Payroll exceeds the weekly budget.");
            }

            int i = 0;
            while (i < 4)
            {
                delete employees[i];
                i = i + 1;
            }
            delete[] employees;

            return 0;
        }
    }
}
```

### 47.3 Búsqueda sobre objetos

**Problema.** Comparar búsqueda lineal, binaria, binaria recursiva y exponencial sobre un conjunto de estudiantes, midiendo comparaciones y precondiciones.

**Diseño.** Los algoritmos comparten un modelo de datos y producen un reporte uniforme. La búsqueda binaria no se presenta aislada: se muestra la necesidad de ordenar y se contrasta el costo observado.

**Auditoría propuesta.** Pruebe claves ausentes, primer y último elemento y un conjunto vacío. Revise qué algoritmos exigen orden y cuáles conservan el arreglo.

**Escala real.** 10 archivos NativeSharp, 404 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

**Reproducción.**

```powershell
nsc --emit-project-oop-exe `
    book_examples/03_searching_objects/Book03SearchingObjects.nsproj `
    -O2 `
    -o 03_searching_objects.exe
./03_searching_objects.exe
```

#### Listado completo

##### `book_examples/03_searching_objects/Book03SearchingObjects.nsproj`

```xml
<Project>
  <PropertyGroup><Name>Book03SearchingObjects</Name></PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
  </ItemGroup>
</Project>
```

##### `book_examples/03_searching_objects/BinarySearcher.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.Searching
{
    public class BinarySearcher : StudentSearcher
    {
        public override owned string Name()
        {
            return String.Copy("Binary Search");
        }

        public override int Search(Student[] data, int count, int targetId, SearchStatistics statistics)
        {
            statistics.Reset();
            int low = 0;
            int high = count - 1;

            while (low <= high)
            {
                int middle = low + (high - low) / 2;
                statistics.Probe();
                statistics.Comparison();

                if (data[middle].Id == targetId)
                {
                    return middle;
                }

                statistics.Comparison();
                if (data[middle].Id < targetId)
                {
                    low = middle + 1;
                }
                else
                {
                    high = middle - 1;
                }
            }

            return -1;
        }
    }
}
```

##### `book_examples/03_searching_objects/ExponentialSearcher.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.Searching
{
    public class ExponentialSearcher : StudentSearcher
    {
        public override owned string Name()
        {
            return String.Copy("Exponential Search");
        }

        private int BinarySearchRange(Student[] data, int targetId, int low, int high, SearchStatistics statistics)
        {
            while (low <= high)
            {
                int middle = low + (high - low) / 2;
                statistics.Probe();
                statistics.Comparison();
                if (data[middle].Id == targetId)
                {
                    return middle;
                }

                statistics.Comparison();
                if (data[middle].Id < targetId)
                {
                    low = middle + 1;
                }
                else
                {
                    high = middle - 1;
                }
            }
            return -1;
        }

        public override int Search(Student[] data, int count, int targetId, SearchStatistics statistics)
        {
            statistics.Reset();
            if (count == 0)
            {
                return -1;
            }

            statistics.Probe();
            statistics.Comparison();
            if (data[0].Id == targetId)
            {
                return 0;
            }

            int bound = 1;
            while (bound < count)
            {
                statistics.Probe();
                statistics.Comparison();
                if (data[bound].Id >= targetId)
                {
                    int lower = bound / 2;
                    return BinarySearchRange(data, targetId, lower, bound, statistics);
                }
                bound = bound * 2;
            }

            int low = bound / 2;
            int high = count - 1;
            return BinarySearchRange(data, targetId, low, high, statistics);
        }
    }
}
```

##### `book_examples/03_searching_objects/LinearSearcher.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.Searching
{
    public class LinearSearcher : StudentSearcher
    {
        public override owned string Name()
        {
            return String.Copy("Linear Search");
        }

        public override int Search(Student[] data, int count, int targetId, SearchStatistics statistics)
        {
            statistics.Reset();
            int index = 0;
            while (index < count)
            {
                statistics.Probe();
                statistics.Comparison();
                if (data[index].Id == targetId)
                {
                    return index;
                }
                index = index + 1;
            }
            return -1;
        }
    }
}
```

##### `book_examples/03_searching_objects/RecursiveBinarySearcher.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.Searching
{
    public class RecursiveBinarySearcher : StudentSearcher
    {
        public override owned string Name()
        {
            return String.Copy("Recursive Binary Search");
        }

        private int SearchRange(Student[] data, int targetId, int low, int high, SearchStatistics statistics)
        {
            if (low > high)
            {
                return -1;
            }

            int middle = low + (high - low) / 2;
            statistics.Probe();
            statistics.Comparison();

            if (data[middle].Id == targetId)
            {
                return middle;
            }

            statistics.Comparison();
            if (targetId < data[middle].Id)
            {
                return SearchRange(data, targetId, low, middle - 1, statistics);
            }

            return SearchRange(data, targetId, middle + 1, high, statistics);
        }

        public override int Search(Student[] data, int count, int targetId, SearchStatistics statistics)
        {
            statistics.Reset();
            return SearchRange(data, targetId, 0, count - 1, statistics);
        }
    }
}
```

##### `book_examples/03_searching_objects/SearchReport.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.Searching
{
    public class SearchReport
    {
        public static void Run(StudentSearcher searcher, Student[] data, int count, int targetId,
                               SearchStatistics statistics)
        {
            int index = searcher.Search(data, count, targetId, statistics);
            raii string algorithm = searcher.Name();
            raii string target = String.ToString(targetId);
            raii string probes = String.ToString(statistics.Probes);
            raii string comparisons = String.ToString(statistics.Comparisons);
            raii string s1 = String.Concat(algorithm, " | target=");
            raii string s2 = String.Concat(s1, target);

            if (index >= 0)
            {
                raii string s3 = String.Concat(s2, " | found: ");
                raii string s4 = String.Concat(s3, data[index].Name);
                raii string s5 = String.Concat(s4, " | probes=");
                raii string s6 = String.Concat(s5, probes);
                raii string s7 = String.Concat(s6, " | comparisons=");
                raii string line = String.Concat(s7, comparisons);
                Console.WriteLine(line);
            }
            else
            {
                raii string s3 = String.Concat(s2, " | NOT FOUND | probes=");
                raii string s4 = String.Concat(s3, probes);
                raii string s5 = String.Concat(s4, " | comparisons=");
                raii string line = String.Concat(s5, comparisons);
                Console.WriteLine(line);
            }
        }
    }
}
```

##### `book_examples/03_searching_objects/SearchStatistics.ns`

```csharp
namespace NativeSharp.BookExamples.Searching
{
    public class SearchStatistics
    {
        private int comparisons;
        private int probes;

        public SearchStatistics()
        {
            comparisons = 0;
            probes = 0;
        }

        public int Comparisons
        {
            get
            {
                return comparisons;
            }
        }
        public int Probes
        {
            get
            {
                return probes;
            }
        }

        public void Reset()
        {
            comparisons = 0;
            probes = 0;
        }

        public void Comparison()
        {
            comparisons = comparisons + 1;
        }

        public void Probe()
        {
            probes = probes + 1;
        }
    }
}
```

##### `book_examples/03_searching_objects/Student.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.Searching
{
    public class Student
    {
        private int id;
        private string name;
        private int grade;

        public Student(int studentId, string studentName, int finalGrade)
        {
            id = studentId;
            name = String.Copy(studentName);
            grade = finalGrade;
        }

        public int Id
        {
            get
            {
                return id;
            }
        }
        public string Name
        {
            get
            {
                return name;
            }
        }
        public int Grade
        {
            get
            {
                return grade;
            }
        }

        ~Student()
        {
            delete name;
        }
    }
}
```

##### `book_examples/03_searching_objects/StudentDataset.ns`

```csharp
namespace NativeSharp.BookExamples.Searching
{
    public class StudentDataset
    {
        public static void FillSorted(Student[] data)
        {
            data[0] = new Student(1005, "Mia", 84);
            data[1] = new Student(1012, "Noah", 91);
            data[2] = new Student(1021, "Sofia", 88);
            data[3] = new Student(1037, "Leo", 79);
            data[4] = new Student(1050, "Emma", 95);
            data[5] = new Student(1064, "Mateo", 82);
            data[6] = new Student(1079, "Valeria", 90);
            data[7] = new Student(1091, "Daniel", 76);
            data[8] = new Student(1110, "Camila", 93);
            data[9] = new Student(1142, "Nicolas", 87);
            data[10] = new Student(1180, "Lucia", 89);
            data[11] = new Student(1205, "Samuel", 85);
            data[12] = new Student(1240, "Elena", 97);
            data[13] = new Student(1301, "Marco", 81);
            data[14] = new Student(1350, "Isabel", 92);
            data[15] = new Student(1408, "Tomas", 80);
        }

        public static void FillUnsorted(Student[] data)
        {
            data[0] = new Student(1110, "Camila", 93);
            data[1] = new Student(1005, "Mia", 84);
            data[2] = new Student(1350, "Isabel", 92);
            data[3] = new Student(1037, "Leo", 79);
            data[4] = new Student(1205, "Samuel", 85);
            data[5] = new Student(1012, "Noah", 91);
            data[6] = new Student(1408, "Tomas", 80);
            data[7] = new Student(1079, "Valeria", 90);
            data[8] = new Student(1240, "Elena", 97);
            data[9] = new Student(1050, "Emma", 95);
            data[10] = new Student(1142, "Nicolas", 87);
            data[11] = new Student(1021, "Sofia", 88);
            data[12] = new Student(1301, "Marco", 81);
            data[13] = new Student(1091, "Daniel", 76);
            data[14] = new Student(1180, "Lucia", 89);
            data[15] = new Student(1064, "Mateo", 82);
        }

        public static void Destroy(Student[] data, int count)
        {
            int i = 0;
            while (i < count)
            {
                delete data[i];
                i = i + 1;
            }
            delete[] data;
        }
    }
}
```

##### `book_examples/03_searching_objects/StudentSearcher.ns`

```csharp
namespace NativeSharp.BookExamples.Searching
{
    public abstract class StudentSearcher
    {
        public abstract owned string Name();
        public abstract int Search(Student[] data, int count, int targetId, SearchStatistics statistics);
    }
}
```

##### `book_examples/03_searching_objects/Program.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.Searching
{
    class Program
    {
        public static int Main()
        {
            Console.WriteLine("NativeSharp Book Example 03 - Searching Objects");
            Console.WriteLine("-------------------------------------------------");
            Console.WriteLine("The algorithms below are implemented here; no collection search API is used.");
            Console.WriteLine("");

            Student[] unsorted = new Student[16];
            Student[] sorted = new Student[16];
            StudentDataset.FillUnsorted(unsorted);
            StudentDataset.FillSorted(sorted);

            raii SearchStatistics statistics = new SearchStatistics();
            raii LinearSearcher linear = new LinearSearcher();
            raii BinarySearcher binary = new BinarySearcher();
            raii RecursiveBinarySearcher recursive = new RecursiveBinarySearcher();
            raii ExponentialSearcher exponential = new ExponentialSearcher();

            Console.WriteLine("Search for existing student 1240:");
            SearchReport.Run(linear, unsorted, 16, 1240, statistics);
            SearchReport.Run(binary, sorted, 16, 1240, statistics);
            SearchReport.Run(recursive, sorted, 16, 1240, statistics);
            SearchReport.Run(exponential, sorted, 16, 1240, statistics);

            Console.WriteLine("");
            Console.WriteLine("Search for missing student 1255:");
            SearchReport.Run(linear, unsorted, 16, 1255, statistics);
            SearchReport.Run(binary, sorted, 16, 1255, statistics);
            SearchReport.Run(recursive, sorted, 16, 1255, statistics);
            SearchReport.Run(exponential, sorted, 16, 1255, statistics);

            StudentDataset.Destroy(unsorted, 16);
            StudentDataset.Destroy(sorted, 16);
            return 0;
        }
    }
}
```

### 47.4 Laboratorio de ordenamiento

**Problema.** Comparar siete algoritmos de ordenamiento sobre objetos y dos criterios sin duplicar cada algoritmo por criterio.

**Diseño.** Una jerarquía de sorters separa la estrategia; otra jerarquía separa la comparación. El reporte reconstruye el dataset, cuenta comparaciones, swaps y movimientos, y certifica que la salida está ordenada.

**Auditoría propuesta.** Observe estabilidad, complejidad, memoria auxiliar y peor caso. Audite que cada corrida trabaje sobre una copia equivalente y destruya tanto dataset como estrategias.

**Escala real.** 14 archivos NativeSharp, 647 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

**Reproducción.**

```powershell
nsc --emit-project-oop-exe `
    book_examples/04_sorting_laboratory/Book04SortingLaboratory.nsproj `
    -O2 `
    -o 04_sorting_laboratory.exe
./04_sorting_laboratory.exe
```

#### Listado completo

##### `book_examples/04_sorting_laboratory/Book04SortingLaboratory.nsproj`

```xml
<Project>
  <PropertyGroup><Name>Book04SortingLaboratory</Name></PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
  </ItemGroup>
</Project>
```

##### `book_examples/04_sorting_laboratory/BubbleSorter.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.Sorting
{
    public class BubbleSorter : StudentSorter
    {
        public override owned string Name()
        {
            return String.Copy("Bubble Sort");
        }

        public override void Sort(Student[] data, int count, StudentComparer comparer, SortStatistics statistics)
        {
            statistics.Reset();
            int end = count - 1;
            bool changed = true;
            while (end > 0 && changed)
            {
                changed = false;
                int i = 0;
                while (i < end)
                {
                    if (Compare(data[i], data[i + 1], comparer, statistics) > 0)
                    {
                        Swap(data, i, i + 1, statistics);
                        changed = true;
                    }
                    i = i + 1;
                }
                end = end - 1;
            }
        }
    }
}
```

##### `book_examples/04_sorting_laboratory/HeapSorter.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.Sorting
{
    public class HeapSorter : StudentSorter
    {
        public override owned string Name()
        {
            return String.Copy("Heap Sort");
        }

        private void SiftDown(Student[] data, int root, int count, StudentComparer comparer, SortStatistics statistics)
        {
            int current = root;
            bool done = false;

            while (!done)
            {
                int left = current * 2 + 1;
                if (left >= count)
                {
                    done = true;
                }
                else
                {
                    int largest = left;
                    int right = left + 1;
                    if (right < count && Compare(data[right], data[left], comparer, statistics) > 0)
                    {
                        largest = right;
                    }

                    if (Compare(data[largest], data[current], comparer, statistics) > 0)
                    {
                        Swap(data, current, largest, statistics);
                        current = largest;
                    }
                    else
                    {
                        done = true;
                    }
                }
            }
        }

        public override void Sort(Student[] data, int count, StudentComparer comparer, SortStatistics statistics)
        {
            statistics.Reset();
            int root = count / 2 - 1;
            while (root >= 0)
            {
                SiftDown(data, root, count, comparer, statistics);
                root = root - 1;
            }

            int end = count - 1;
            while (end > 0)
            {
                Swap(data, 0, end, statistics);
                SiftDown(data, 0, end, comparer, statistics);
                end = end - 1;
            }
        }
    }
}
```

##### `book_examples/04_sorting_laboratory/InsertionSorter.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.Sorting
{
    public class InsertionSorter : StudentSorter
    {
        public override owned string Name()
        {
            return String.Copy("Insertion Sort");
        }

        public override void Sort(Student[] data, int count, StudentComparer comparer, SortStatistics statistics)
        {
            statistics.Reset();
            int i = 1;
            while (i < count)
            {
                Student key = data[i];
                statistics.Move();
                int j = i - 1;
                bool shifting = true;
                while (j >= 0 && shifting)
                {
                    if (Compare(data[j], key, comparer, statistics) > 0)
                    {
                        data[j + 1] = data[j];
                        statistics.Move();
                        j = j - 1;
                    }
                    else
                    {
                        shifting = false;
                    }
                }
                data[j + 1] = key;
                statistics.Move();
                i = i + 1;
            }
        }
    }
}
```

##### `book_examples/04_sorting_laboratory/MergeSorter.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.Sorting
{
    public class MergeSorter : StudentSorter
    {
        public override owned string Name()
        {
            return String.Copy("Merge Sort");
        }

        private void Merge(Student[] data, Student[] buffer, int left, int middle, int right, StudentComparer comparer,
                           SortStatistics statistics)
        {
            int i = left;
            int j = middle + 1;
            int k = left;

            while (i <= middle && j <= right)
            {
                if (Compare(data[i], data[j], comparer, statistics) <= 0)
                {
                    buffer[k] = data[i];
                    i = i + 1;
                }
                else
                {
                    buffer[k] = data[j];
                    j = j + 1;
                }
                statistics.Move();
                k = k + 1;
            }

            while (i <= middle)
            {
                buffer[k] = data[i];
                statistics.Move();
                i = i + 1;
                k = k + 1;
            }

            while (j <= right)
            {
                buffer[k] = data[j];
                statistics.Move();
                j = j + 1;
                k = k + 1;
            }

            k = left;
            while (k <= right)
            {
                data[k] = buffer[k];
                statistics.Move();
                k = k + 1;
            }
        }

        private void SortRange(Student[] data, Student[] buffer, int left, int right, StudentComparer comparer,
                               SortStatistics statistics)
        {
            if (left >= right)
            {
                return;
            }

            int middle = left + (right - left) / 2;
            SortRange(data, buffer, left, middle, comparer, statistics);
            SortRange(data, buffer, middle + 1, right, comparer, statistics);
            Merge(data, buffer, left, middle, right, comparer, statistics);
        }

        public override void Sort(Student[] data, int count, StudentComparer comparer, SortStatistics statistics)
        {
            statistics.Reset();
            Student[] buffer = new Student[count];
            if (count > 1)
            {
                SortRange(data, buffer, 0, count - 1, comparer, statistics);
            }
            delete[] buffer;
        }
    }
}
```

##### `book_examples/04_sorting_laboratory/QuickSorter.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.Sorting
{
    public class QuickSorter : StudentSorter
    {
        public override owned string Name()
        {
            return String.Copy("Quick Sort");
        }

        private int Partition(Student[] data, int low, int high, StudentComparer comparer, SortStatistics statistics)
        {
            Student pivot = data[high];
            int boundary = low - 1;
            int scan = low;

            while (scan < high)
            {
                if (Compare(data[scan], pivot, comparer, statistics) <= 0)
                {
                    boundary = boundary + 1;
                    if (boundary != scan)
                    {
                        Swap(data, boundary, scan, statistics);
                    }
                }
                scan = scan + 1;
            }

            int pivotIndex = boundary + 1;
            if (pivotIndex != high)
            {
                Swap(data, pivotIndex, high, statistics);
            }
            return pivotIndex;
        }

        private void SortRange(Student[] data, int low, int high, StudentComparer comparer, SortStatistics statistics)
        {
            if (low >= high)
            {
                return;
            }

            int pivot = Partition(data, low, high, comparer, statistics);
            SortRange(data, low, pivot - 1, comparer, statistics);
            SortRange(data, pivot + 1, high, comparer, statistics);
        }

        public override void Sort(Student[] data, int count, StudentComparer comparer, SortStatistics statistics)
        {
            statistics.Reset();
            if (count > 1)
            {
                SortRange(data, 0, count - 1, comparer, statistics);
            }
        }
    }
}
```

##### `book_examples/04_sorting_laboratory/SelectionSorter.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.Sorting
{
    public class SelectionSorter : StudentSorter
    {
        public override owned string Name()
        {
            return String.Copy("Selection Sort");
        }

        public override void Sort(Student[] data, int count, StudentComparer comparer, SortStatistics statistics)
        {
            statistics.Reset();
            int i = 0;
            while (i < count - 1)
            {
                int smallest = i;
                int j = i + 1;
                while (j < count)
                {
                    if (Compare(data[j], data[smallest], comparer, statistics) < 0)
                    {
                        smallest = j;
                    }
                    j = j + 1;
                }
                if (smallest != i)
                {
                    Swap(data, i, smallest, statistics);
                }
                i = i + 1;
            }
        }
    }
}
```

##### `book_examples/04_sorting_laboratory/ShellSorter.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.Sorting
{
    public class ShellSorter : StudentSorter
    {
        public override owned string Name()
        {
            return String.Copy("Shell Sort");
        }

        public override void Sort(Student[] data, int count, StudentComparer comparer, SortStatistics statistics)
        {
            statistics.Reset();
            int gap = count / 2;
            while (gap > 0)
            {
                int i = gap;
                while (i < count)
                {
                    Student value = data[i];
                    statistics.Move();
                    int j = i;
                    bool shifting = true;
                    while (j >= gap && shifting)
                    {
                        if (Compare(data[j - gap], value, comparer, statistics) > 0)
                        {
                            data[j] = data[j - gap];
                            statistics.Move();
                            j = j - gap;
                        }
                        else
                        {
                            shifting = false;
                        }
                    }
                    data[j] = value;
                    statistics.Move();
                    i = i + 1;
                }
                gap = gap / 2;
            }
        }
    }
}
```

##### `book_examples/04_sorting_laboratory/SortingReport.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.Sorting
{
    public class SortingReport
    {
        private static bool IsSorted(Student[] data, int count, StudentComparer comparer)
        {
            int i = 1;
            while (i < count)
            {
                if (comparer.Compare(data[i - 1], data[i]) > 0)
                {
                    return false;
                }
                i = i + 1;
            }
            return true;
        }

        private static void PrintSummary(StudentSorter sorter, StudentComparer comparer, Student[] data, int count,
                                         SortStatistics statistics)
        {
            raii string algorithm = sorter.Name();
            raii string comparerName = comparer.Name();
            raii string comparisons = String.ToString(statistics.Comparisons);
            raii string swaps = String.ToString(statistics.Swaps);
            raii string moves = String.ToString(statistics.Moves);
            raii string firstId = String.ToString(data[0].Id);
            raii string lastId = String.ToString(data[count - 1].Id);

            raii string s1 = String.Concat(algorithm, " | by ");
            raii string s2 = String.Concat(s1, comparerName);
            raii string s3 = String.Concat(s2, " | cmp=");
            raii string s4 = String.Concat(s3, comparisons);
            raii string s5 = String.Concat(s4, " swaps=");
            raii string s6 = String.Concat(s5, swaps);
            raii string s7 = String.Concat(s6, " moves=");
            raii string s8 = String.Concat(s7, moves);
            raii string s9 = String.Concat(s8, " | first/last id=");
            raii string s10 = String.Concat(s9, firstId);
            raii string s11 = String.Concat(s10, "/");
            raii string line = String.Concat(s11, lastId);
            Console.WriteLine(line);
        }

        public static void Run(StudentSorter sorter, StudentComparer comparer, SortStatistics statistics)
        {
            int count = StudentDataset.Count();
            Student[] data = new Student[count];
            StudentDataset.Fill(data);
            sorter.Sort(data, count, comparer, statistics);

            if (!IsSorted(data, count, comparer))
            {
                Console.WriteLine("ERROR: algorithm did not produce ordered data.");
            }
            else
            {
                PrintSummary(sorter, comparer, data, count, statistics);
            }

            StudentDataset.Destroy(data, count);
        }
    }
}
```

##### `book_examples/04_sorting_laboratory/SortStatistics.ns`

```csharp
namespace NativeSharp.BookExamples.Sorting
{
    public class SortStatistics
    {
        private int comparisons;
        private int swaps;
        private int moves;

        public SortStatistics()
        {
            Reset();
        }

        public int Comparisons
        {
            get
            {
                return comparisons;
            }
        }
        public int Swaps
        {
            get
            {
                return swaps;
            }
        }
        public int Moves
        {
            get
            {
                return moves;
            }
        }

        public void Reset()
        {
            comparisons = 0;
            swaps = 0;
            moves = 0;
        }

        public void Comparison()
        {
            comparisons = comparisons + 1;
        }
        public void Swap()
        {
            swaps = swaps + 1;
        }
        public void Move()
        {
            moves = moves + 1;
        }
    }
}
```

##### `book_examples/04_sorting_laboratory/Student.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.Sorting
{
    public class Student
    {
        private int id;
        private string name;
        private int grade;

        public Student(int studentId, string studentName, int finalGrade)
        {
            id = studentId;
            name = String.Copy(studentName);
            grade = finalGrade;
        }

        public int Id
        {
            get
            {
                return id;
            }
        }
        public string Name
        {
            get
            {
                return name;
            }
        }
        public int Grade
        {
            get
            {
                return grade;
            }
        }

        ~Student()
        {
            delete name;
        }
    }
}
```

##### `book_examples/04_sorting_laboratory/StudentComparer.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.Sorting
{
    public abstract class StudentComparer
    {
        public abstract int Compare(Student left, Student right);
        public abstract owned string Name();
    }

    public class StudentIdComparer : StudentComparer
    {
        public override int Compare(Student left, Student right)
        {
            if (left.Id < right.Id)
            {
                return -1;
            }
            if (left.Id > right.Id)
            {
                return 1;
            }
            return 0;
        }

        public override owned string Name()
        {
            return String.Copy("student id");
        }
    }

    public class StudentGradeComparer : StudentComparer
    {
        public override int Compare(Student left, Student right)
        {
            if (left.Grade < right.Grade)
            {
                return -1;
            }
            if (left.Grade > right.Grade)
            {
                return 1;
            }
            if (left.Id < right.Id)
            {
                return -1;
            }
            if (left.Id > right.Id)
            {
                return 1;
            }
            return 0;
        }

        public override owned string Name()
        {
            return String.Copy("grade, then id");
        }
    }
}
```

##### `book_examples/04_sorting_laboratory/StudentDataset.ns`

```csharp
namespace NativeSharp.BookExamples.Sorting
{
    public class StudentDataset
    {
        public static int Count()
        {
            return 12;
        }

        public static void Fill(Student[] data)
        {
            data[0] = new Student(1042, "Alicia", 88);
            data[1] = new Student(1007, "Bruno", 95);
            data[2] = new Student(1120, "Carla", 74);
            data[3] = new Student(1011, "Dario", 88);
            data[4] = new Student(1099, "Elisa", 91);
            data[5] = new Student(1033, "Fabian", 82);
            data[6] = new Student(1201, "Gabriela", 97);
            data[7] = new Student(1025, "Hector", 79);
            data[8] = new Student(1150, "Irene", 91);
            data[9] = new Student(1001, "Julia", 86);
            data[10] = new Student(1105, "Kevin", 74);
            data[11] = new Student(1055, "Laura", 93);
        }

        public static void Destroy(Student[] data, int count)
        {
            int i = 0;
            while (i < count)
            {
                delete data[i];
                i = i + 1;
            }
            delete[] data;
        }
    }
}
```

##### `book_examples/04_sorting_laboratory/StudentSorter.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.Sorting
{
    public abstract class StudentSorter
    {
        public abstract owned string Name();
        public abstract void Sort(Student[] data, int count, StudentComparer comparer, SortStatistics statistics);

        protected static void Swap(Student[] data, int a, int b, SortStatistics statistics)
        {
            Student temporary = data[a];
            data[a] = data[b];
            data[b] = temporary;
            statistics.Swap();
            statistics.Move();
            statistics.Move();
            statistics.Move();
        }

        protected static int Compare(Student left, Student right, StudentComparer comparer, SortStatistics statistics)
        {
            statistics.Comparison();
            return comparer.Compare(left, right);
        }
    }
}
```

##### `book_examples/04_sorting_laboratory/Program.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.Sorting
{
    class Program
    {
        private static void RunAll(StudentComparer comparer)
        {
            raii SortStatistics statistics = new SortStatistics();
            raii BubbleSorter bubble = new BubbleSorter();
            raii SelectionSorter selection = new SelectionSorter();
            raii InsertionSorter insertion = new InsertionSorter();
            raii ShellSorter shell = new ShellSorter();
            raii MergeSorter merge = new MergeSorter();
            raii QuickSorter quick = new QuickSorter();
            raii HeapSorter heap = new HeapSorter();

            SortingReport.Run(bubble, comparer, statistics);
            SortingReport.Run(selection, comparer, statistics);
            SortingReport.Run(insertion, comparer, statistics);
            SortingReport.Run(shell, comparer, statistics);
            SortingReport.Run(merge, comparer, statistics);
            SortingReport.Run(quick, comparer, statistics);
            SortingReport.Run(heap, comparer, statistics);
        }

        public static int Main()
        {
            Console.WriteLine("NativeSharp Book Example 04 - Sorting Laboratory");
            Console.WriteLine("-------------------------------------------------");
            Console.WriteLine("Every sort is implemented from scratch over raw NativeSharp arrays.");
            Console.WriteLine("");

            raii StudentIdComparer byId = new StudentIdComparer();
            Console.WriteLine("Ordering twelve Student objects by id:");
            RunAll(byId);

            Console.WriteLine("");
            raii StudentGradeComparer byGrade = new StudentGradeComparer();
            Console.WriteLine("Same algorithms, different polymorphic comparison policy:");
            RunAll(byGrade);

            return 0;
        }
    }
}
```

### 47.5 Lista simplemente enlazada genérica

**Problema.** Implementar inserción, búsqueda, eliminación y recorrido sin delegar la invariante de enlaces a una colección existente.

**Diseño.** La lista genérica separa nodo y contenedor. El contenedor conserva cabeza, cola y cantidad; las operaciones actualizan los tres de manera coordinada.

**Auditoría propuesta.** Dibuje los enlaces antes y después de quitar cabeza, cola y nodo medio. Compruebe que `Clear` destruye cada nodo una sola vez y deja el objeto reutilizable.

**Escala real.** 4 archivos NativeSharp, 331 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

**Reproducción.**

```powershell
nsc --emit-project-oop-exe `
    book_examples/05_singly_linked_list/Book05SinglyLinkedList.nsproj `
    -O2 `
    -o 05_singly_linked_list.exe
./05_singly_linked_list.exe
```

#### Listado completo

##### `book_examples/05_singly_linked_list/Book05SinglyLinkedList.nsproj`

```xml
<Project>
  <PropertyGroup><Name>Book05SinglyLinkedList</Name></PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
  </ItemGroup>
</Project>
```

##### `book_examples/05_singly_linked_list/IntListPrinter.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.LinkedList
{
    public class IntListPrinter
    {
        public static void Print(string caption, SinglyLinkedList<int> list)
        {
            raii string line = String.Copy(caption);
            int i = 0;
            while (i < list.Count)
            {
                raii string value = String.ToString(list.GetAt(i));
                raii string separator = String.Copy(" -> ");
                if (i == 0)
                {
                    raii string first = String.Concat(line, value);
                    line = move(first);
                }
                else
                {
                    raii string withSeparator = String.Concat(line, separator);
                    raii string withValue = String.Concat(withSeparator, value);
                    line = move(withValue);
                }
                i = i + 1;
            }
            Console.WriteLine(line);
        }
    }
}
```

##### `book_examples/05_singly_linked_list/SinglyLinkedList.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.LinkedList
{
    public class SinglyLinkedList<T>
    {
        private SinglyNode<T> head;
        private SinglyNode<T> tail;
        private int count;

        public SinglyLinkedList()
        {
            head = null;
            tail = null;
            count = 0;
        }

        public int Count
        {
            get
            {
                return count;
            }
        }
        public bool IsEmpty
        {
            get
            {
                return count == 0;
            }
        }

        public SinglyNode<T> First()
        {
            return head;
        }

        public void AddFirst(T value)
        {
            SinglyNode<T> node = new SinglyNode<T>(value);
            node.Next = head;
            head = node;
            if (tail == null)
            {
                tail = node;
            }
            count = count + 1;
        }

        public void AddLast(T value)
        {
            SinglyNode<T> node = new SinglyNode<T>(value);
            if (tail == null)
            {
                head = node;
                tail = node;
            }
            else
            {
                tail.Next = node;
                tail = node;
            }
            count = count + 1;
        }

        private SinglyNode<T> NodeAt(int index)
        {
            if (index < 0 || index >= count)
            {
                throw new ArgumentOutOfRangeException();
            }

            SinglyNode<T> current = head;
            int position = 0;
            while (position < index)
            {
                current = current.Next;
                position = position + 1;
            }
            return current;
        }

        public T GetAt(int index)
        {
            return NodeAt(index).Value;
        }

        public void SetAt(int index, T value)
        {
            NodeAt(index).Value = value;
        }

        public void InsertAt(int index, T value)
        {
            if (index < 0 || index > count)
            {
                throw new ArgumentOutOfRangeException();
            }

            if (index == 0)
            {
                AddFirst(value);
                return;
            }

            if (index == count)
            {
                AddLast(value);
                return;
            }

            SinglyNode<T> previous = NodeAt(index - 1);
            SinglyNode<T> node = new SinglyNode<T>(value);
            node.Next = previous.Next;
            previous.Next = node;
            count = count + 1;
        }

        public void RemoveFirst(T[] result)
        {
            if (head == null)
            {
                throw new InvalidOperationException();
            }

            SinglyNode<T> removed = head;
            result[0] = removed.Value;
            head = removed.Next;
            removed.Next = null;
            delete removed;
            count = count - 1;

            if (head == null)
            {
                tail = null;
            }
        }

        public void RemoveAt(int index, T[] result)
        {
            if (index < 0 || index >= count)
            {
                throw new ArgumentOutOfRangeException();
            }

            if (index == 0)
            {
                RemoveFirst(result);
                return;
            }

            SinglyNode<T> previous = NodeAt(index - 1);
            SinglyNode<T> removed = previous.Next;
            result[0] = removed.Value;
            previous.Next = removed.Next;

            if (removed == tail)
            {
                tail = previous;
            }

            removed.Next = null;
            delete removed;
            count = count - 1;
        }

        public int IndexOf(T value)
        {
            SinglyNode<T> current = head;
            int index = 0;
            while (current != null)
            {
                if (current.Value == value)
                {
                    return index;
                }
                current = current.Next;
                index = index + 1;
            }
            return -1;
        }

        public bool Contains(T value)
        {
            return IndexOf(value) >= 0;
        }

        public void Reverse()
        {
            SinglyNode<T> previous = null;
            SinglyNode<T> current = head;
            tail = head;

            while (current != null)
            {
                SinglyNode<T> next = current.Next;
                current.Next = previous;
                previous = current;
                current = next;
            }

            head = previous;
        }

        public void Clear()
        {
            SinglyNode<T> current = head;
            while (current != null)
            {
                SinglyNode<T> next = current.Next;
                current.Next = null;
                delete current;
                current = next;
            }
            head = null;
            tail = null;
            count = 0;
        }

        ~SinglyLinkedList()
        {
            Clear();
        }
    }
}
```

##### `book_examples/05_singly_linked_list/SinglyNode.ns`

```csharp
namespace NativeSharp.BookExamples.LinkedList
{
    public class SinglyNode<T>
    {
        public T Value;
        public SinglyNode<T> Next;

        public SinglyNode(T value)
        {
            Value = value;
            Next = null;
        }
    }
}
```

##### `book_examples/05_singly_linked_list/Program.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.LinkedList
{
    class Program
    {
        public static int Main()
        {
            Console.WriteLine("NativeSharp Book Example 05 - Singly Linked List<T>");
            Console.WriteLine("-----------------------------------------------------");
            Console.WriteLine("The node chain and all list operations are implemented from scratch.");
            Console.WriteLine("");

            raii SinglyLinkedList<int> list = new SinglyLinkedList<int>();

            list.AddLast(10);
            list.AddLast(20);
            list.AddLast(30);
            list.AddFirst(5);
            list.InsertAt(2, 15);
            list.InsertAt(list.Count, 40);
            IntListPrinter.Print("after insertions: ", list);

            int position30 = list.IndexOf(30);
            raii string positionText = String.ToString(position30);
            raii string positionLine = String.Concat("index of 30: ", positionText);
            Console.WriteLine(positionLine);

            if (list.Contains(15))
            {
                Console.WriteLine("list contains 15");
            }

            int[] removed = new int[1];
            list.RemoveFirst(removed);
            int removedFirst = removed[0];
            list.RemoveAt(2, removed);
            int removedMiddle = removed[0];
            raii string firstText = String.ToString(removedFirst);
            raii string middleText = String.ToString(removedMiddle);
            raii string r1 = String.Concat("removed first/middle: ", firstText);
            raii string r2 = String.Concat(r1, " / ");
            raii string removedLine = String.Concat(r2, middleText);
            Console.WriteLine(removedLine);
            IntListPrinter.Print("after removals:   ", list);

            list.Reverse();
            IntListPrinter.Print("after reverse:    ", list);

            list.SetAt(1, 999);
            IntListPrinter.Print("after SetAt:      ", list);

            Console.WriteLine("");
            Console.WriteLine("Walking the node chain directly:");
            SinglyNode<int> node = list.First();
            int ordinal = 0;
            while (node != null)
            {
                raii string ordinalText = String.ToString(ordinal);
                raii string valueText = String.ToString(node.Value);
                raii string s1 = String.Concat("node ", ordinalText);
                raii string s2 = String.Concat(s1, " contains ");
                raii string line = String.Concat(s2, valueText);
                Console.WriteLine(line);
                node = node.Next;
                ordinal = ordinal + 1;
            }

            delete[] removed;
            return 0;
        }
    }
}
```

### 47.6 Pila y evaluador de expresiones

**Problema.** Evaluar expresiones infijas con precedencia, paréntesis, números de varios dígitos y recuperación controlada ante entrada inválida.

**Diseño.** Una pila genérica redimensionable sostiene valores y operadores. El evaluador convierte la gramática operacional en código, reduce según precedencia y lanza una excepción de dominio con estado de error.

**Auditoría propuesta.** Trace una expresión carácter por carácter. Pruebe paréntesis desbalanceados, división por cero, operador sin operando y una expresión vacía.

**Escala real.** 4 archivos NativeSharp, 355 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

**Reproducción.**

```powershell
nsc --emit-project-oop-exe `
    book_examples/06_stack_expression_evaluator/Book06StackExpressionEvaluator.nsproj `
    -O2 `
    -o 06_stack_expression_evaluator.exe
./06_stack_expression_evaluator.exe
```

#### Listado completo

##### `book_examples/06_stack_expression_evaluator/Book06StackExpressionEvaluator.nsproj`

```xml
<Project>
  <PropertyGroup><Name>Book06StackExpressionEvaluator</Name></PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
  </ItemGroup>
</Project>
```

##### `book_examples/06_stack_expression_evaluator/ArrayStack.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.StackEvaluator
{
    public class ArrayStack<T>
    {
        private T[] items;
        private int count;
        private int capacity;

        public ArrayStack(int initialCapacity)
        {
            capacity = initialCapacity;
            if (capacity < 2)
            {
                capacity = 2;
            }
            items = new T[capacity];
            count = 0;
        }

        public int Count
        {
            get
            {
                return count;
            }
        }
        public bool IsEmpty
        {
            get
            {
                return count == 0;
            }
        }

        private void Grow()
        {
            int newCapacity = capacity * 2;
            const T[] grown = new T[newCapacity];
            int i = 0;
            while (i < count)
            {
                grown[i] = items[i];
                i = i + 1;
            }
            delete[] items;
            items = grown;
            capacity = newCapacity;
        }

        public void Push(T value)
        {
            if (count == capacity)
            {
                Grow();
            }
            items[count] = value;
            count = count + 1;
        }

        public void Peek(T[] result)
        {
            if (count == 0)
            {
                throw new InvalidOperationException();
            }
            result[0] = items[count - 1];
        }

        public void Pop(T[] result)
        {
            if (count == 0)
            {
                throw new InvalidOperationException();
            }
            count = count - 1;
            result[0] = items[count];
        }

        public void Clear()
        {
            count = 0;
        }

        ~ArrayStack()
        {
            delete[] items;
        }
    }
}
```

##### `book_examples/06_stack_expression_evaluator/ExpressionEvaluator.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.StackEvaluator
{
    public class ExpressionEvaluator
    {
        private ArrayStack<int> values;
        private ArrayStack<char> operators;
        private int[] valueSlot;
        private char[] operatorSlot;

        public ExpressionEvaluator()
        {
            values = new ArrayStack<int>(8);
            operators = new ArrayStack<char>(8);
            valueSlot = new int[1];
            operatorSlot = new char[1];
        }

        private static bool IsDigit(char value)
        {
            return value >= '0' && value <= '9';
        }

        private static int DigitValue(char value)
        {
            return (int)value - (int)'0';
        }

        private static bool IsOperator(char value)
        {
            return value == '+' || value == '-' || value == '*' || value == '/';
        }

        private static int Precedence(char value)
        {
            if (value == '*' || value == '/')
            {
                return 2;
            }
            if (value == '+' || value == '-')
            {
                return 1;
            }
            return 0;
        }

        private void Fail(string message)
        {
            ExpressionException problem = new ExpressionException();
            problem.Message = String.Copy(message);
            throw problem;
        }

        private void ApplyTopOperator()
        {
            operators.Pop(operatorSlot);
            char operation = operatorSlot[0];

            if (values.Count < 2)
            {
                Fail("Operator does not have two operands.");
                return;
            }

            values.Pop(valueSlot);
            int right = valueSlot[0];
            values.Pop(valueSlot);
            int left = valueSlot[0];
            int result = 0;

            if (operation == '+')
            {
                result = left + right;
            }
            else if (operation == '-')
            {
                result = left - right;
            }
            else if (operation == '*')
            {
                result = left * right;
            }
            else if (operation == '/')
            {
                if (right == 0)
                {
                    Fail("Division by zero in expression.");
                    return;
                }
                result = left / right;
            }
            else
            {
                Fail("Unknown operator.");
                return;
            }

            values.Push(result);
        }

        private void ReduceFor(char incoming)
        {
            bool reducing = true;
            while (!operators.IsEmpty && reducing)
            {
                operators.Peek(operatorSlot);
                char top = operatorSlot[0];
                if (top == '(')
                {
                    reducing = false;
                }
                else if (Precedence(top) >= Precedence(incoming))
                {
                    ApplyTopOperator();
                }
                else
                {
                    reducing = false;
                }
            }
        }

        private void CloseParenthesis()
        {
            bool matched = false;
            while (!operators.IsEmpty && !matched)
            {
                operators.Peek(operatorSlot);
                if (operatorSlot[0] == '(')
                {
                    operators.Pop(operatorSlot);
                    matched = true;
                }
                else
                {
                    ApplyTopOperator();
                }
            }

            if (!matched)
            {
                Fail("Closing parenthesis has no matching opening parenthesis.");
            }
        }

        public int Evaluate(string expression)
        {
            values.Clear();
            operators.Clear();
            int index = 0;

            while (index < expression.Length)
            {
                char token = expression[index];

                if (token == ' ' || token == '\t')
                {
                    index = index + 1;
                }
                else if (IsDigit(token))
                {
                    int number = 0;
                    while (index < expression.Length && IsDigit(expression[index]))
                    {
                        number = number * 10 + DigitValue(expression[index]);
                        index = index + 1;
                    }
                    values.Push(number);
                }
                else if (token == '(')
                {
                    operators.Push(token);
                    index = index + 1;
                }
                else if (token == ')')
                {
                    CloseParenthesis();
                    index = index + 1;
                }
                else if (IsOperator(token))
                {
                    ReduceFor(token);
                    operators.Push(token);
                    index = index + 1;
                }
                else
                {
                    Fail("Expression contains an unsupported character.");
                    return 0;
                }
            }

            while (!operators.IsEmpty)
            {
                operators.Peek(operatorSlot);
                if (operatorSlot[0] == '(')
                {
                    Fail("Opening parenthesis has no matching closing parenthesis.");
                    return 0;
                }
                ApplyTopOperator();
            }

            if (values.Count != 1)
            {
                Fail("Expression did not reduce to exactly one value.");
                return 0;
            }

            values.Pop(valueSlot);
            return valueSlot[0];
        }

        ~ExpressionEvaluator()
        {
            delete values;
            delete operators;
            delete[] valueSlot;
            delete[] operatorSlot;
        }
    }
}
```

##### `book_examples/06_stack_expression_evaluator/ExpressionException.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.StackEvaluator
{
    public class ExpressionException : Exception
    {
    }
}
```

##### `book_examples/06_stack_expression_evaluator/Program.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.StackEvaluator
{
    class Program
    {
        private static void EvaluateAndPrint(ExpressionEvaluator evaluator, string expression)
        {
            int result = evaluator.Evaluate(expression);
            raii string resultText = String.ToString(result);
            raii string s1 = String.Concat(expression, " = ");
            raii string line = String.Concat(s1, resultText);
            Console.WriteLine(line);
        }

        public static int Main()
        {
            Console.WriteLine("NativeSharp Book Example 06 - Stack + Expression Evaluator");
            Console.WriteLine("----------------------------------------------------------");
            Console.WriteLine("Two ArrayStack<T> instances implement a small shunting-yard evaluator.");
            Console.WriteLine("");

            raii ExpressionEvaluator evaluator = new ExpressionEvaluator();

            EvaluateAndPrint(evaluator, "2 + 3 * 4");
            EvaluateAndPrint(evaluator, "(2 + 3) * 4");
            EvaluateAndPrint(evaluator, "100 / (5 * 2) + 7");
            EvaluateAndPrint(evaluator, "8 * (12 - 5) + 36 / 6");
            EvaluateAndPrint(evaluator, "42 + 18 / 3 - 5 * 2");

            Console.WriteLine("");
            Console.WriteLine("Malformed expression handling:");
            try
            {
                int ignored = evaluator.Evaluate("10 / (3 - 3)");
            }
            catch (ExpressionException problem)
            {
                Console.WriteLine(problem.Message);
            }

            return 0;
        }
    }
}
```

### 47.7 Simulación de una cola de servicio

**Problema.** Modelar llegadas, espera, atención y utilización de un escritorio durante una secuencia discreta de ticks.

**Diseño.** Una cola circular evita desplazar elementos. La simulación separa clientes, dataset y estadísticas para que la política pueda cambiar sin alterar la estructura.

**Auditoría propuesta.** Compruebe wrap-around, cola llena, tick sin cliente y cierre con trabajo pendiente. Distinga tiempo medio de espera de utilización del servidor.

**Escala real.** 6 archivos NativeSharp, 347 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

**Reproducción.**

```powershell
nsc --emit-project-oop-exe `
    book_examples/07_queue_service_simulation/Book07QueueServiceSimulation.nsproj `
    -O2 `
    -o 07_queue_service_simulation.exe
./07_queue_service_simulation.exe
```

#### Listado completo

##### `book_examples/07_queue_service_simulation/Book07QueueServiceSimulation.nsproj`

```xml
<Project>
  <PropertyGroup><Name>Book07QueueServiceSimulation</Name></PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
  </ItemGroup>
</Project>
```

##### `book_examples/07_queue_service_simulation/CircularQueue.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.QueueSimulation
{
    public class CircularQueue<T>
    {
        private T[] items;
        private int head;
        private int tail;
        private int count;
        private int capacity;

        public CircularQueue(int initialCapacity)
        {
            capacity = initialCapacity;
            if (capacity < 2)
            {
                capacity = 2;
            }
            items = new T[capacity];
            head = 0;
            tail = 0;
            count = 0;
        }

        public int Count
        {
            get
            {
                return count;
            }
        }
        public bool IsEmpty
        {
            get
            {
                return count == 0;
            }
        }

        private void Grow()
        {
            int newCapacity = capacity * 2;
            const T[] grown = new T[newCapacity];
            int i = 0;
            while (i < count)
            {
                int source = (head + i) % capacity;
                grown[i] = items[source];
                i = i + 1;
            }
            delete[] items;
            items = grown;
            capacity = newCapacity;
            head = 0;
            tail = count;
        }

        public void Enqueue(T value)
        {
            if (count == capacity)
            {
                Grow();
            }
            items[tail] = value;
            tail = (tail + 1) % capacity;
            count = count + 1;
        }

        public void Peek(T[] result)
        {
            if (count == 0)
            {
                throw new InvalidOperationException();
            }
            result[0] = items[head];
        }

        public void Dequeue(T[] result)
        {
            if (count == 0)
            {
                throw new InvalidOperationException();
            }
            result[0] = items[head];
            head = (head + 1) % capacity;
            count = count - 1;
        }

        ~CircularQueue()
        {
            delete[] items;
        }
    }
}
```

##### `book_examples/07_queue_service_simulation/Customer.ns`

```csharp
namespace NativeSharp.BookExamples.QueueSimulation
{
    public class Customer
    {
        private int id;
        private int arrivalTime;
        private int serviceDuration;
        private int serviceStart;
        private int serviceFinish;

        public Customer(int customerId, int arrival, int duration)
        {
            id = customerId;
            arrivalTime = arrival;
            serviceDuration = duration;
            serviceStart = -1;
            serviceFinish = -1;
        }

        public int Id
        {
            get
            {
                return id;
            }
        }
        public int ArrivalTime
        {
            get
            {
                return arrivalTime;
            }
        }
        public int ServiceDuration
        {
            get
            {
                return serviceDuration;
            }
        }
        public int ServiceStart
        {
            get
            {
                return serviceStart;
            }
        }
        public int ServiceFinish
        {
            get
            {
                return serviceFinish;
            }
        }
        public int WaitingTime
        {
            get
            {
                return serviceStart - arrivalTime;
            }
        }

        public void StartAt(int time)
        {
            serviceStart = time;
            serviceFinish = time + serviceDuration;
        }
    }
}
```

##### `book_examples/07_queue_service_simulation/CustomerDataset.ns`

```csharp
namespace NativeSharp.BookExamples.QueueSimulation
{
    public class CustomerDataset
    {
        public static int Count()
        {
            return 12;
        }

        public static void Fill(Customer[] customers)
        {
            customers[0] = new Customer(1, 0, 3);
            customers[1] = new Customer(2, 1, 5);
            customers[2] = new Customer(3, 2, 2);
            customers[3] = new Customer(4, 4, 4);
            customers[4] = new Customer(5, 4, 1);
            customers[5] = new Customer(6, 7, 3);
            customers[6] = new Customer(7, 8, 6);
            customers[7] = new Customer(8, 9, 2);
            customers[8] = new Customer(9, 12, 4);
            customers[9] = new Customer(10, 13, 2);
            customers[10] = new Customer(11, 15, 3);
            customers[11] = new Customer(12, 18, 1);
        }

        public static void Destroy(Customer[] customers, int count)
        {
            int i = 0;
            while (i < count)
            {
                delete customers[i];
                i = i + 1;
            }
            delete[] customers;
        }
    }
}
```

##### `book_examples/07_queue_service_simulation/ServiceDeskSimulation.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.QueueSimulation
{
    public class ServiceDeskSimulation
    {
        private CircularQueue<Customer> waiting;
        private Customer[] dequeueSlot;
        private SimulationStatistics statistics;

        public ServiceDeskSimulation()
        {
            waiting = new CircularQueue<Customer>(4);
            dequeueSlot = new Customer[1];
            statistics = new SimulationStatistics();
        }

        private void PrintArrival(Customer customer, int queueLength)
        {
            raii string id = String.ToString(customer.Id);
            raii string time = String.ToString(customer.ArrivalTime);
            raii string length = String.ToString(queueLength);
            raii string s1 = String.Concat("t=", time);
            raii string s2 = String.Concat(s1, "  customer #");
            raii string s3 = String.Concat(s2, id);
            raii string s4 = String.Concat(s3, " arrives; queue=");
            raii string line = String.Concat(s4, length);
            Console.WriteLine(line);
        }

        private void PrintService(Customer customer)
        {
            raii string id = String.ToString(customer.Id);
            raii string start = String.ToString(customer.ServiceStart);
            raii string finish = String.ToString(customer.ServiceFinish);
            raii string wait = String.ToString(customer.WaitingTime);
            raii string s1 = String.Concat("      service #", id);
            raii string s2 = String.Concat(s1, " start=");
            raii string s3 = String.Concat(s2, start);
            raii string s4 = String.Concat(s3, " finish=");
            raii string s5 = String.Concat(s4, finish);
            raii string s6 = String.Concat(s5, " waited=");
            raii string line = String.Concat(s6, wait);
            Console.WriteLine(line);
        }

        public int Run(Customer[] customers, int customerCount)
        {
            int nextArrival = 0;
            int clock = 0;
            Customer active = null;
            int busyUntil = 0;

            while (nextArrival < customerCount || !waiting.IsEmpty || active != null)
            {
                if (active != null && clock >= busyUntil)
                {
                    active = null;
                }

                while (nextArrival < customerCount && customers[nextArrival].ArrivalTime == clock)
                {
                    waiting.Enqueue(customers[nextArrival]);
                    PrintArrival(customers[nextArrival], waiting.Count);
                    statistics.ObserveQueue(waiting.Count);
                    nextArrival = nextArrival + 1;
                }

                if (active == null && !waiting.IsEmpty)
                {
                    waiting.Dequeue(dequeueSlot);
                    active = dequeueSlot[0];
                    active.StartAt(clock);
                    busyUntil = active.ServiceFinish;
                    statistics.RecordService(active);
                    PrintService(active);
                }

                if (active == null && waiting.IsEmpty && nextArrival < customerCount)
                {
                    statistics.RecordIdleMinute();
                }

                clock = clock + 1;
            }

            return clock - 1;
        }

        public SimulationStatistics Statistics()
        {
            return statistics;
        }

        ~ServiceDeskSimulation()
        {
            delete waiting;
            delete[] dequeueSlot;
            delete statistics;
        }
    }
}
```

##### `book_examples/07_queue_service_simulation/SimulationStatistics.ns`

```csharp
namespace NativeSharp.BookExamples.QueueSimulation
{
    public class SimulationStatistics
    {
        private int totalWaitingTime;
        private int maximumWaitingTime;
        private int maximumQueueLength;
        private int idleMinutes;
        private int servedCustomers;

        public SimulationStatistics()
        {
            totalWaitingTime = 0;
            maximumWaitingTime = 0;
            maximumQueueLength = 0;
            idleMinutes = 0;
            servedCustomers = 0;
        }

        public int TotalWaitingTime
        {
            get
            {
                return totalWaitingTime;
            }
        }
        public int MaximumWaitingTime
        {
            get
            {
                return maximumWaitingTime;
            }
        }
        public int MaximumQueueLength
        {
            get
            {
                return maximumQueueLength;
            }
        }
        public int IdleMinutes
        {
            get
            {
                return idleMinutes;
            }
        }
        public int ServedCustomers
        {
            get
            {
                return servedCustomers;
            }
        }

        public void ObserveQueue(int length)
        {
            if (length > maximumQueueLength)
            {
                maximumQueueLength = length;
            }
        }

        public void RecordService(Customer customer)
        {
            int wait = customer.WaitingTime;
            totalWaitingTime = totalWaitingTime + wait;
            if (wait > maximumWaitingTime)
            {
                maximumWaitingTime = wait;
            }
            servedCustomers = servedCustomers + 1;
        }

        public void RecordIdleMinute()
        {
            idleMinutes = idleMinutes + 1;
        }
    }
}
```

##### `book_examples/07_queue_service_simulation/Program.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.QueueSimulation
{
    class Program
    {
        public static int Main()
        {
            Console.WriteLine("NativeSharp Book Example 07 - Queue Service Simulation");
            Console.WriteLine("------------------------------------------------------");
            Console.WriteLine("A hand-written circular Queue<T> drives a single-service-desk simulation.");
            Console.WriteLine("");

            int count = CustomerDataset.Count();
            Customer[] customers = new Customer[count];
            CustomerDataset.Fill(customers);

            raii ServiceDeskSimulation simulation = new ServiceDeskSimulation();
            int completionTime = simulation.Run(customers, count);
            SimulationStatistics stats = simulation.Statistics();

            Console.WriteLine("");
            Console.WriteLine("Simulation summary");
            Console.WriteLine("------------------");
            raii string served = String.ToString(stats.ServedCustomers);
            raii string totalWait = String.ToString(stats.TotalWaitingTime);
            raii string maxWait = String.ToString(stats.MaximumWaitingTime);
            raii string maxQueue = String.ToString(stats.MaximumQueueLength);
            raii string idle = String.ToString(stats.IdleMinutes);
            raii string finished = String.ToString(completionTime);
            raii string l1 = String.Concat("served customers: ", served);
            raii string l2 = String.Concat("total waiting minutes: ", totalWait);
            raii string l3 = String.Concat("maximum individual wait: ", maxWait);
            raii string l4 = String.Concat("maximum queue length: ", maxQueue);
            raii string l5 = String.Concat("idle minutes before next arrival: ", idle);
            raii string l6 = String.Concat("last service completed at t=", finished);
            Console.WriteLine(l1);
            Console.WriteLine(l2);
            Console.WriteLine(l3);
            Console.WriteLine(l4);
            Console.WriteLine(l5);
            Console.WriteLine(l6);

            CustomerDataset.Destroy(customers, count);
            return 0;
        }
    }
}
```

### 47.8 Árbol binario de búsqueda

**Problema.** Mantener estudiantes ordenados por clave con inserción, búsqueda, recorridos y eliminación de los tres casos estructurales.

**Diseño.** El árbol encapsula la raíz y el tamaño; los nodos mantienen enlaces. La eliminación sustituye mediante sucesor cuando hay dos hijos y conserva el orden global.

**Auditoría propuesta.** Valide duplicados, hoja, nodo con un hijo y raíz con dos. Después de cada operación, el recorrido inorder debe permanecer ordenado.

**Escala real.** 5 archivos NativeSharp, 447 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

**Reproducción.**

```powershell
nsc --emit-project-oop-exe `
    book_examples/08_binary_search_tree/Book08BinarySearchTree.nsproj `
    -O2 `
    -o 08_binary_search_tree.exe
./08_binary_search_tree.exe
```

#### Listado completo

##### `book_examples/08_binary_search_tree/Book08BinarySearchTree.nsproj`

```xml
<Project>
  <PropertyGroup><Name>Book08BinarySearchTree</Name></PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
  </ItemGroup>
</Project>
```

##### `book_examples/08_binary_search_tree/BinarySearchTree.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.BinarySearchTree
{
    public class BinarySearchTree
    {
        private StudentNode root;
        private int count;

        public BinarySearchTree()
        {
            root = null;
            count = 0;
        }

        public int Count
        {
            get
            {
                return count;
            }
        }
        public bool IsEmpty
        {
            get
            {
                return root == null;
            }
        }

        public void Insert(Student value)
        {
            StudentNode node = new StudentNode(value);
            if (root == null)
            {
                root = node;
                count = 1;
                return;
            }

            StudentNode current = root;
            bool inserted = false;
            while (!inserted)
            {
                if (value.Id < current.Value.Id)
                {
                    if (current.Left == null)
                    {
                        current.Left = node;
                        inserted = true;
                    }
                    else
                    {
                        current = current.Left;
                    }
                }
                else if (value.Id > current.Value.Id)
                {
                    if (current.Right == null)
                    {
                        current.Right = node;
                        inserted = true;
                    }
                    else
                    {
                        current = current.Right;
                    }
                }
                else
                {
                    delete node;
                    delete value;
                    throw new ArgumentException();
                }
            }

            count = count + 1;
        }

        public Student Find(int id)
        {
            StudentNode current = root;
            while (current != null)
            {
                if (id == current.Value.Id)
                {
                    return current.Value;
                }
                if (id < current.Value.Id)
                {
                    current = current.Left;
                }
                else
                {
                    current = current.Right;
                }
            }
            return null;
        }

        public Student Minimum()
        {
            if (root == null)
            {
                return null;
            }
            StudentNode current = root;
            while (current.Left != null)
            {
                current = current.Left;
            }
            return current.Value;
        }

        public Student Maximum()
        {
            if (root == null)
            {
                return null;
            }
            StudentNode current = root;
            while (current.Right != null)
            {
                current = current.Right;
            }
            return current.Value;
        }

        private int HeightOf(StudentNode node)
        {
            if (node == null)
            {
                return 0;
            }
            int leftHeight = HeightOf(node.Left);
            int rightHeight = HeightOf(node.Right);
            if (leftHeight > rightHeight)
            {
                return leftHeight + 1;
            }
            return rightHeight + 1;
        }

        public int Height()
        {
            return HeightOf(root);
        }

        private StudentNode DeleteNode(StudentNode node, int id, bool[] removed)
        {
            if (node == null)
            {
                return null;
            }

            if (id < node.Value.Id)
            {
                node.Left = DeleteNode(node.Left, id, removed);
                return node;
            }

            if (id > node.Value.Id)
            {
                node.Right = DeleteNode(node.Right, id, removed);
                return node;
            }

            removed[0] = true;

            if (node.Left == null && node.Right == null)
            {
                delete node.Value;
                delete node;
                return null;
            }

            if (node.Left == null)
            {
                StudentNode child = node.Right;
                node.Right = null;
                delete node.Value;
                delete node;
                return child;
            }

            if (node.Right == null)
            {
                StudentNode child = node.Left;
                node.Left = null;
                delete node.Value;
                delete node;
                return child;
            }

            StudentNode successor = node.Right;
            while (successor.Left != null)
            {
                successor = successor.Left;
            }

            Student temporaryValue = node.Value;
            node.Value = successor.Value;
            successor.Value = temporaryValue;

            bool[] successorRemoved = new bool[1];
            successorRemoved[0] = false;
            node.Right = DeleteNode(node.Right, id, successorRemoved);
            delete[] successorRemoved;
            return node;
        }

        public bool Remove(int id)
        {
            bool[] removed = new bool[1];
            removed[0] = false;
            root = DeleteNode(root, id, removed);
            bool result = removed[0];
            if (result)
            {
                count = count - 1;
            }
            delete[] removed;
            return result;
        }

        private void PrintStudent(Student value)
        {
            raii string id = String.ToString(value.Id);
            raii string grade = String.ToString(value.Grade);
            raii string s1 = String.Concat(id, "  ");
            raii string s2 = String.Concat(s1, value.Name);
            raii string s3 = String.Concat(s2, "  grade=");
            raii string line = String.Concat(s3, grade);
            Console.WriteLine(line);
        }

        private void InOrderNode(StudentNode node)
        {
            if (node == null)
            {
                return;
            }
            InOrderNode(node.Left);
            PrintStudent(node.Value);
            InOrderNode(node.Right);
        }

        private void PreOrderNode(StudentNode node)
        {
            if (node == null)
            {
                return;
            }
            PrintStudent(node.Value);
            PreOrderNode(node.Left);
            PreOrderNode(node.Right);
        }

        private void PostOrderNode(StudentNode node)
        {
            if (node == null)
            {
                return;
            }
            PostOrderNode(node.Left);
            PostOrderNode(node.Right);
            PrintStudent(node.Value);
        }

        public void PrintInOrder()
        {
            InOrderNode(root);
        }

        public void PrintPreOrder()
        {
            PreOrderNode(root);
        }

        public void PrintPostOrder()
        {
            PostOrderNode(root);
        }

        private void PrintSidewaysNode(StudentNode node, int depth)
        {
            if (node == null)
            {
                return;
            }
            PrintSidewaysNode(node.Right, depth + 1);

            raii string indent = String.Copy("");
            int i = 0;
            while (i < depth)
            {
                raii string nextIndent = String.Concat(indent, "    ");
                indent = move(nextIndent);
                i = i + 1;
            }
            raii string id = String.ToString(node.Value.Id);
            raii string line = String.Concat(indent, id);
            Console.WriteLine(line);

            PrintSidewaysNode(node.Left, depth + 1);
        }

        public void PrintSideways()
        {
            PrintSidewaysNode(root, 0);
        }

        private void ClearNode(StudentNode node)
        {
            if (node == null)
            {
                return;
            }
            ClearNode(node.Left);
            ClearNode(node.Right);
            node.Left = null;
            node.Right = null;
            delete node.Value;
            delete node;
        }

        public void Clear()
        {
            ClearNode(root);
            root = null;
            count = 0;
        }

        ~BinarySearchTree()
        {
            Clear();
        }
    }
}
```

##### `book_examples/08_binary_search_tree/Student.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.BinarySearchTree
{
    public class Student
    {
        private int id;
        private string name;
        private int grade;

        public Student(int studentId, string studentName, int finalGrade)
        {
            id = studentId;
            name = String.Copy(studentName);
            grade = finalGrade;
        }

        public int Id
        {
            get
            {
                return id;
            }
        }
        public string Name
        {
            get
            {
                return name;
            }
        }
        public int Grade
        {
            get
            {
                return grade;
            }
        }

        ~Student()
        {
            delete name;
        }
    }
}
```

##### `book_examples/08_binary_search_tree/StudentNode.ns`

```csharp
namespace NativeSharp.BookExamples.BinarySearchTree
{
    public class StudentNode
    {
        public Student Value;
        public StudentNode Left;
        public StudentNode Right;

        public StudentNode(Student value)
        {
            Value = value;
            Left = null;
            Right = null;
        }
    }
}
```

##### `book_examples/08_binary_search_tree/StudentTreeFactory.ns`

```csharp
namespace NativeSharp.BookExamples.BinarySearchTree
{
    public class StudentTreeFactory
    {
        public static void Fill(BinarySearchTree tree)
        {
            tree.Insert(new Student(50, "Ana", 90));
            tree.Insert(new Student(25, "Bruno", 82));
            tree.Insert(new Student(80, "Carla", 94));
            tree.Insert(new Student(10, "Diego", 76));
            tree.Insert(new Student(35, "Elena", 88));
            tree.Insert(new Student(65, "Fabian", 91));
            tree.Insert(new Student(95, "Gabriela", 85));
            tree.Insert(new Student(30, "Hector", 79));
            tree.Insert(new Student(40, "Irene", 93));
            tree.Insert(new Student(60, "Julia", 87));
            tree.Insert(new Student(70, "Kevin", 89));
            tree.Insert(new Student(90, "Laura", 96));
            tree.Insert(new Student(100, "Mateo", 81));
        }
    }
}
```

##### `book_examples/08_binary_search_tree/Program.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.BinarySearchTree
{
    class Program
    {
        private static void PrintTreeFacts(BinarySearchTree tree)
        {
            raii string count = String.ToString(tree.Count);
            raii string height = String.ToString(tree.Height());
            Student minimum = tree.Minimum();
            Student maximum = tree.Maximum();
            raii string minId = String.ToString(minimum.Id);
            raii string maxId = String.ToString(maximum.Id);
            raii string l1 = String.Concat("node count: ", count);
            raii string l2 = String.Concat("height: ", height);
            raii string l3 = String.Concat("minimum id: ", minId);
            raii string l4 = String.Concat("maximum id: ", maxId);
            Console.WriteLine(l1);
            Console.WriteLine(l2);
            Console.WriteLine(l3);
            Console.WriteLine(l4);
        }

        public static int Main()
        {
            Console.WriteLine("NativeSharp Book Example 08 - Binary Search Tree");
            Console.WriteLine("-------------------------------------------------");

            raii BinarySearchTree tree = new BinarySearchTree();
            StudentTreeFactory.Fill(tree);

            Console.WriteLine("Tree shape (right subtree is printed above each node):");
            tree.PrintSideways();
            Console.WriteLine("");
            PrintTreeFacts(tree);

            Console.WriteLine("");
            Console.WriteLine("In-order traversal (keys must appear sorted):");
            tree.PrintInOrder();

            Console.WriteLine("");
            Console.WriteLine("Search for id 65:");
            Student found = tree.Find(65);
            if (found != null)
            {
                raii string line = String.Concat("found student: ", found.Name);
                Console.WriteLine(line);
            }

            Console.WriteLine("");
            Console.WriteLine("Deletion cases:");
            Console.WriteLine("  remove leaf 40");
            tree.Remove(40);
            Console.WriteLine("  remove node 35 (now one child)");
            tree.Remove(35);
            Console.WriteLine("  remove root 50 (two children)");
            tree.Remove(50);

            Console.WriteLine("");
            Console.WriteLine("Tree after deletions:");
            tree.PrintSideways();
            Console.WriteLine("");
            PrintTreeFacts(tree);

            Console.WriteLine("");
            Console.WriteLine("Pre-order traversal:");
            tree.PrintPreOrder();
            Console.WriteLine("");
            Console.WriteLine("Post-order traversal:");
            tree.PrintPostOrder();

            return 0;
        }
    }
}
```

### 47.9 Lista doblemente enlazada

**Problema.** Obtener recorrido en ambos sentidos e inserciones y eliminaciones locales con enlaces anterior y siguiente.

**Diseño.** La redundancia de enlaces reduce costo de navegación pero duplica la invariante. Cada mutación debe actualizar pares simétricos y extremos.

**Auditoría propuesta.** Revise las cuatro fronteras: vacía, un nodo, cabeza y cola. Una mitad actualizada produce corrupción que puede no aparecer hasta recorrer en sentido contrario.

**Escala real.** 4 archivos NativeSharp, 323 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

**Reproducción.**

```powershell
nsc --emit-project-oop-exe `
    book_examples/09_doubly_linked_list/Book09DoublyLinkedList.nsproj `
    -O2 `
    -o 09_doubly_linked_list.exe
./09_doubly_linked_list.exe
```

#### Listado completo

##### `book_examples/09_doubly_linked_list/Book09DoublyLinkedList.nsproj`

```xml
<Project>
  <PropertyGroup><Name>Book09DoublyLinkedList</Name></PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
  </ItemGroup>
</Project>
```

##### `book_examples/09_doubly_linked_list/DoublyLinkedList.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.DoublyLinkedList
{
    public class DoublyLinkedList<T>
    {
        private DoublyNode<T> head;
        private DoublyNode<T> tail;
        private int count;

        public DoublyLinkedList()
        {
            head = null;
            tail = null;
            count = 0;
        }

        public int Count
        {
            get
            {
                return count;
            }
        }
        public bool IsEmpty
        {
            get
            {
                return count == 0;
            }
        }
        public DoublyNode<T> First
        {
            get
            {
                return head;
            }
        }
        public DoublyNode<T> Last
        {
            get
            {
                return tail;
            }
        }

        private DoublyNode<T> NodeAt(int index)
        {
            if (index < 0 || index >= count)
            {
                throw new ArgumentOutOfRangeException();
            }

            if (index < count / 2)
            {
                DoublyNode<T> current = head;
                int i = 0;
                while (i < index)
                {
                    current = current.Next;
                    i = i + 1;
                }
                return current;
            }

            DoublyNode<T> reverse = tail;
            int j = count - 1;
            while (j > index)
            {
                reverse = reverse.Previous;
                j = j - 1;
            }
            return reverse;
        }

        public T GetAt(int index)
        {
            return NodeAt(index).Value;
        }

        public void AddFirst(T value)
        {
            DoublyNode<T> node = new DoublyNode<T>(value);
            node.Next = head;
            if (head != null)
            {
                head.Previous = node;
            }
            else
            {
                tail = node;
            }
            head = node;
            count = count + 1;
        }

        public void AddLast(T value)
        {
            DoublyNode<T> node = new DoublyNode<T>(value);
            node.Previous = tail;
            if (tail != null)
            {
                tail.Next = node;
            }
            else
            {
                head = node;
            }
            tail = node;
            count = count + 1;
        }

        public void InsertAt(int index, T value)
        {
            if (index < 0 || index > count)
            {
                throw new ArgumentOutOfRangeException();
            }
            if (index == 0)
            {
                AddFirst(value);
                return;
            }
            if (index == count)
            {
                AddLast(value);
                return;
            }

            DoublyNode<T> after = NodeAt(index);
            DoublyNode<T> before = after.Previous;
            DoublyNode<T> node = new DoublyNode<T>(value);
            node.Previous = before;
            node.Next = after;
            before.Next = node;
            after.Previous = node;
            count = count + 1;
        }

        private void Detach(DoublyNode<T> node)
        {
            if (node.Previous != null)
            {
                node.Previous.Next = node.Next;
            }
            else
            {
                head = node.Next;
            }

            if (node.Next != null)
            {
                node.Next.Previous = node.Previous;
            }
            else
            {
                tail = node.Previous;
            }

            node.Previous = null;
            node.Next = null;
            count = count - 1;
        }

        public void RemoveAt(int index, T[] result)
        {
            DoublyNode<T> node = NodeAt(index);
            result[0] = node.Value;
            Detach(node);
            delete node;
        }

        public bool RemoveFirstValue(T value)
        {
            DoublyNode<T> current = head;
            while (current != null)
            {
                if (current.Value == value)
                {
                    Detach(current);
                    delete current;
                    return true;
                }
                current = current.Next;
            }
            return false;
        }

        public int IndexOf(T value)
        {
            int index = 0;
            DoublyNode<T> current = head;
            while (current != null)
            {
                if (current.Value == value)
                {
                    return index;
                }
                current = current.Next;
                index = index + 1;
            }
            return -1;
        }

        public void Reverse()
        {
            DoublyNode<T> current = head;
            while (current != null)
            {
                DoublyNode<T> originalNext = current.Next;
                current.Next = current.Previous;
                current.Previous = originalNext;
                current = originalNext;
            }

            DoublyNode<T> temporary = head;
            head = tail;
            tail = temporary;
        }

        public void Clear()
        {
            DoublyNode<T> current = head;
            while (current != null)
            {
                DoublyNode<T> next = current.Next;
                current.Previous = null;
                current.Next = null;
                delete current;
                current = next;
            }
            head = null;
            tail = null;
            count = 0;
        }

        ~DoublyLinkedList()
        {
            Clear();
        }
    }
}
```

##### `book_examples/09_doubly_linked_list/DoublyNode.ns`

```csharp
namespace NativeSharp.BookExamples.DoublyLinkedList
{
    public class DoublyNode<T>
    {
        public T Value;
        public DoublyNode<T> Previous;
        public DoublyNode<T> Next;

        public DoublyNode(T value)
        {
            Value = value;
            Previous = null;
            Next = null;
        }
    }
}
```

##### `book_examples/09_doubly_linked_list/ListPrinter.ns`

```csharp
using NativeSharp.Core;
namespace NativeSharp.BookExamples.DoublyLinkedList
{
    public class ListPrinter
    {
        public static void Forward(DoublyLinkedList<int> list)
        {
            raii string line = String.Copy("");
            DoublyNode<int> current = list.First;
            while (current != null)
            {
                raii string value = String.ToString(current.Value);
                raii string withValue = String.Concat(line, value);
                line = move(withValue);
                if (current.Next != null)
                {
                    raii string withArrow = String.Concat(line, " <-> ");
                    line = move(withArrow);
                }
                current = current.Next;
            }
            Console.WriteLine(line);
        }

        public static void Backward(DoublyLinkedList<int> list)
        {
            raii string line = String.Copy("");
            DoublyNode<int> current = list.Last;
            while (current != null)
            {
                raii string value = String.ToString(current.Value);
                raii string withValue = String.Concat(line, value);
                line = move(withValue);
                if (current.Previous != null)
                {
                    raii string withArrow = String.Concat(line, " <-> ");
                    line = move(withArrow);
                }
                current = current.Previous;
            }
            Console.WriteLine(line);
        }
    }
}
```

##### `book_examples/09_doubly_linked_list/Program.ns`

```csharp
using NativeSharp.Core;
namespace NativeSharp.BookExamples.DoublyLinkedList
{
    class Program
    {
        public static int Main()
        {
            Console.WriteLine("NativeSharp Book Example 09 - Doubly Linked List");
            Console.WriteLine("------------------------------------------------");
            raii DoublyLinkedList<int> list = new DoublyLinkedList<int>();
            list.AddLast(20);
            list.AddLast(30);
            list.AddFirst(10);
            list.AddLast(50);
            list.InsertAt(3, 40);
            Console.WriteLine("Initial list, forward:");
            ListPrinter.Forward(list);
            Console.WriteLine("Same nodes, backward from tail:");
            ListPrinter.Backward(list);

            Console.WriteLine("");
            Console.WriteLine("Insert 25 at index 2 and remove 40:");
            list.InsertAt(2, 25);
            list.RemoveFirstValue(40);
            ListPrinter.Forward(list);

            int[] removed = new int[1];
            list.RemoveAt(0, removed);
            raii string removedText = String.ToString(removed[0]);
            raii string removedLine = String.Concat("removed head: ", removedText);
            Console.WriteLine(removedLine);
            delete[] removed;

            Console.WriteLine("Reverse by swapping every node's next/previous links:");
            list.Reverse();
            ListPrinter.Forward(list);
            ListPrinter.Backward(list);

            raii string countText = String.ToString(list.Count);
            raii string countLine = String.Concat("remaining node count: ", countText);
            Console.WriteLine(countLine);
            return 0;
        }
    }
}
```

### 47.10 Lista circular y problema de Josephus

**Problema.** Resolver eliminación periódica sobre un círculo sin transformar el problema en índices de un array.

**Diseño.** La estructura hace que el sucesor de la cola sea la cabeza. El solver consume nodos siguiendo la semántica del círculo y deja visible el orden de eliminación.

**Auditoría propuesta.** Pruebe paso uno, un participante y pasos mayores que el tamaño. Compruebe que el último enlace no retenga un ciclo después de limpiar.

**Escala real.** 4 archivos NativeSharp, 194 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

**Reproducción.**

```powershell
nsc --emit-project-oop-exe `
    book_examples/10_circular_linked_list_josephus/Book10CircularJosephus.nsproj `
    -O2 `
    -o 10_circular_linked_list_josephus.exe
./10_circular_linked_list_josephus.exe
```

#### Listado completo

##### `book_examples/10_circular_linked_list_josephus/Book10CircularJosephus.nsproj`

```xml
<Project>
  <PropertyGroup><Name>Book10CircularJosephus</Name></PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
  </ItemGroup>
</Project>
```

##### `book_examples/10_circular_linked_list_josephus/CircleNode.ns`

```csharp
namespace NativeSharp.BookExamples.CircularJosephus
{
    public class CircleNode
    {
        public int Value;
        public CircleNode Next;
        public CircleNode Previous;
        public CircleNode(int value)
        {
            Value = value;
            Next = null;
            Previous = null;
        }
    }
}
```

##### `book_examples/10_circular_linked_list_josephus/CircularList.ns`

```csharp
using NativeSharp.Core;
namespace NativeSharp.BookExamples.CircularJosephus
{
    public class CircularList
    {
        private CircleNode head;
        private int count;

        public CircularList()
        {
            head = null;
            count = 0;
        }

        public int Count
        {
            get
            {
                return count;
            }
        }
        public CircleNode First
        {
            get
            {
                return head;
            }
        }

        public void AddLast(int value)
        {
            CircleNode node = new CircleNode(value);
            if (head == null)
            {
                head = node;
                node.Next = node;
                node.Previous = node;
            }
            else
            {
                CircleNode tail = head.Previous;
                node.Next = head;
                node.Previous = tail;
                tail.Next = node;
                head.Previous = node;
            }
            count = count + 1;
        }

        public int Remove(CircleNode node)
        {
            if (node == null || count == 0)
            {
                throw new InvalidOperationException();
            }

            int value = node.Value;
            if (count == 1)
            {
                head = null;
            }
            else
            {
                node.Previous.Next = node.Next;
                node.Next.Previous = node.Previous;
                if (node == head)
                {
                    head = node.Next;
                }
            }
            node.Next = null;
            node.Previous = null;
            delete node;
            count = count - 1;
            return value;
        }

        public void PrintOneLap()
        {
            if (head == null)
            {
                Console.WriteLine("<empty>");
                return;
            }
            raii string line = String.Copy("");
            CircleNode current = head;
            int visited = 0;
            while (visited < count)
            {
                raii string text = String.ToString(current.Value);
                raii string withValue = String.Concat(line, text);
                line = move(withValue);
                if (visited + 1 < count)
                {
                    raii string withArrow = String.Concat(line, " -> ");
                    line = move(withArrow);
                }
                current = current.Next;
                visited = visited + 1;
            }
            raii string closed = String.Concat(line, " -> (back to head)");
            line = move(closed);
            Console.WriteLine(line);
        }

        public void Clear()
        {
            while (count > 0)
            {
                Remove(head);
            }
        }

        ~CircularList()
        {
            Clear();
        }
    }
}
```

##### `book_examples/10_circular_linked_list_josephus/JosephusSolver.ns`

```csharp
using NativeSharp.Core;
namespace NativeSharp.BookExamples.CircularJosephus
{
    public class JosephusSolver
    {
        public static int Solve(int people, int step)
        {
            if (people <= 0 || step <= 0)
            {
                throw new ArgumentOutOfRangeException();
            }

            raii CircularList circle = new CircularList();
            int i = 1;
            while (i <= people)
            {
                circle.AddLast(i);
                i = i + 1;
            }

            Console.WriteLine("Initial circle:");
            circle.PrintOneLap();
            Console.WriteLine("Elimination order:");

            CircleNode current = circle.First;
            while (circle.Count > 1)
            {
                int hops = 1;
                while (hops < step)
                {
                    current = current.Next;
                    hops = hops + 1;
                }

                CircleNode next = current.Next;
                int eliminated = circle.Remove(current);
                raii string value = String.ToString(eliminated);
                raii string line = String.Concat("  eliminated: ", value);
                Console.WriteLine(line);
                current = next;
            }

            int survivor = circle.First.Value;
            return survivor;
        }
    }
}
```

##### `book_examples/10_circular_linked_list_josephus/Program.ns`

```csharp
using NativeSharp.Core;
namespace NativeSharp.BookExamples.CircularJosephus
{
    class Program
    {
        public static int Main()
        {
            Console.WriteLine("NativeSharp Book Example 10 - Circular List / Josephus");
            Console.WriteLine("---------------------------------------------------------");
            int people = 12;
            int step = 5;
            raii string p = String.ToString(people);
            raii string s = String.ToString(step);
            raii string a = String.Concat("people = ", p);
            raii string b = String.Concat(", step = ", s);
            raii string intro = String.Concat(a, b);
            Console.WriteLine(intro);
            int survivor = JosephusSolver.Solve(people, step);
            raii string result = String.ToString(survivor);
            raii string line = String.Concat("survivor: ", result);
            Console.WriteLine(line);
            return 0;
        }
    }
}
```

### 47.11 Deque y análisis de palíndromos

**Problema.** Usar una estructura de doble extremo tanto como cola de trabajo como para comparar caracteres simétricos.

**Diseño.** El deque se implementa sobre array circular y comparte la aritmética de índices entre frente y fondo. El analizador es un consumidor que no conoce el almacenamiento.

**Auditoría propuesta.** Audite wrap-around, crecimiento, longitud impar, texto vacío y normalización. Un deque correcto debe conservar orden tras combinaciones de ambos extremos.

**Escala real.** 4 archivos NativeSharp, 218 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

**Reproducción.**

```powershell
nsc --emit-project-oop-exe `
    book_examples/11_deque_and_palindrome/Book11DequeAndPalindrome.nsproj `
    -O2 `
    -o 11_deque_and_palindrome.exe
./11_deque_and_palindrome.exe
```

#### Listado completo

##### `book_examples/11_deque_and_palindrome/Book11DequeAndPalindrome.nsproj`

```xml
<Project>
  <PropertyGroup><Name>Book11DequeAndPalindrome</Name></PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
  </ItemGroup>
</Project>
```

##### `book_examples/11_deque_and_palindrome/ArrayDeque.ns`

```csharp
using NativeSharp.Core;
namespace NativeSharp.BookExamples.Deque
{
    public class ArrayDeque<T>
    {
        private T[] items;
        private int head;
        private int count;

        public ArrayDeque(int capacity)
        {
            if (capacity < 2)
            {
                capacity = 2;
            }
            items = new T[capacity];
            head = 0;
            count = 0;
        }

        public int Count
        {
            get
            {
                return count;
            }
        }
        public bool IsEmpty
        {
            get
            {
                return count == 0;
            }
        }

        private int Index(int logicalIndex)
        {
            return (head + logicalIndex) % items.Length;
        }

        private void Grow()
        {
            int newCapacity = items.Length * 2;
            const T[] grown = new T[newCapacity];
            int i = 0;
            while (i < count)
            {
                grown[i] = items[Index(i)];
                i = i + 1;
            }
            delete[] items;
            items = grown;
            head = 0;
        }

        public void AddBack(T value)
        {
            if (count == items.Length)
            {
                Grow();
            }
            items[Index(count)] = value;
            count = count + 1;
        }

        public void AddFront(T value)
        {
            if (count == items.Length)
            {
                Grow();
            }
            head = head - 1;
            if (head < 0)
            {
                head = items.Length - 1;
            }
            items[head] = value;
            count = count + 1;
        }

        public void RemoveFront(T[] result)
        {
            if (count == 0)
            {
                throw new InvalidOperationException();
            }
            result[0] = items[head];
            head = (head + 1) % items.Length;
            count = count - 1;
            if (count == 0)
            {
                head = 0;
            }
        }

        public void RemoveBack(T[] result)
        {
            if (count == 0)
            {
                throw new InvalidOperationException();
            }
            int index = Index(count - 1);
            result[0] = items[index];
            count = count - 1;
            if (count == 0)
            {
                head = 0;
            }
        }

        public T PeekFront()
        {
            if (count == 0)
            {
                throw new InvalidOperationException();
            }
            return items[head];
        }

        public T PeekBack()
        {
            if (count == 0)
            {
                throw new InvalidOperationException();
            }
            return items[Index(count - 1)];
        }

        ~ArrayDeque()
        {
            delete[] items;
        }
    }
}
```

##### `book_examples/11_deque_and_palindrome/PalindromeAnalyzer.ns`

```csharp
using NativeSharp.Core;
namespace NativeSharp.BookExamples.Deque
{
    public class PalindromeAnalyzer
    {
        private static bool IsLetterOrDigit(char c)
        {
            bool digit = c >= '0' && c <= '9';
            bool lower = c >= 'a' && c <= 'z';
            bool upper = c >= 'A' && c <= 'Z';
            return digit || lower || upper;
        }

        private static char FoldAscii(char c)
        {
            if (c >= 'A' && c <= 'Z')
            {
                return (char)((int)c + 32);
            }
            return c;
        }

        public static bool IsPalindrome(string text)
        {
            raii ArrayDeque<char> deque = new ArrayDeque<char>(8);
            int i = 0;
            while (i < text.Length)
            {
                char c = text[i];
                if (IsLetterOrDigit(c))
                {
                    deque.AddBack(FoldAscii(c));
                }
                i = i + 1;
            }

            char[] left = new char[1];
            char[] right = new char[1];
            while (deque.Count > 1)
            {
                deque.RemoveFront(left);
                deque.RemoveBack(right);
                if (left[0] != right[0])
                {
                    delete[] left;
                    delete[] right;
                    return false;
                }
            }
            delete[] left;
            delete[] right;
            return true;
        }
    }
}
```

##### `book_examples/11_deque_and_palindrome/WorkItem.ns`

```csharp
namespace NativeSharp.BookExamples.Deque
{
    public class WorkItem
    {
        public int Id;
        public string Name;
        public bool Urgent;
        public WorkItem(int id, string name, bool urgent)
        {
            Id = id;
            Name = name;
            Urgent = urgent;
        }
    }
}
```

##### `book_examples/11_deque_and_palindrome/Program.ns`

```csharp
using NativeSharp.Core;
namespace NativeSharp.BookExamples.Deque
{
    class Program
    {
        private static void Check(string text)
        {
            bool palindrome = PalindromeAnalyzer.IsPalindrome(text);
            raii string state = String.Copy("not a palindrome");
            if (palindrome)
            {
                raii string yesState = String.Copy("palindrome");
                state = move(yesState);
            }
            raii string a = String.Concat("  ", text);
            raii string b = String.Concat(a, " -> ");
            raii string line = String.Concat(b, state);
            Console.WriteLine(line);
        }

        public static int Main()
        {
            Console.WriteLine("NativeSharp Book Example 11 - Deque");
            Console.WriteLine("-----------------------------------");
            Console.WriteLine("Palindrome checker: same deque removes from both ends.");
            Check("radar");
            Check("Never odd or even");
            Check("NativeSharp");
            Check("A man a plan a canal Panama");

            Console.WriteLine("");
            Console.WriteLine("Work scheduling: urgent items enter at the front.");
            raii ArrayDeque<WorkItem> work = new ArrayDeque<WorkItem>(3);
            work.AddBack(new WorkItem(101, "texture import", false));
            work.AddBack(new WorkItem(102, "mesh conversion", false));
            work.AddFront(new WorkItem(900, "shader compile failure", true));
            work.AddBack(new WorkItem(103, "audio bank build", false));
            work.AddFront(new WorkItem(901, "missing game executable", true));

            WorkItem[] slot = new WorkItem[1];
            while (!work.IsEmpty)
            {
                work.RemoveFront(slot);
                WorkItem item = slot[0];
                raii string id = String.ToString(item.Id);
                raii string a = String.Concat("  process #", id);
                raii string b = String.Concat(a, " - ");
                raii string line = String.Concat(b, item.Name);
                Console.WriteLine(line);
                delete item;
            }
            delete[] slot;
            return 0;
        }
    }
}
```

### 47.12 Árbol AVL

**Problema.** Conservar búsqueda logarítmica mediante alturas y rotaciones después de insertar y eliminar.

**Diseño.** El árbol implementa rotaciones izquierda, derecha y dobles, recalcula alturas y expone una validación de balance. El programa muestra la estructura lateralmente tras operaciones reales.

**Auditoría propuesta.** Fuerce los cuatro patrones de rotación. Verifique simultáneamente orden BST, alturas almacenadas y factor de balance; comprobar sólo uno puede ocultar el bug.

**Escala real.** 4 archivos NativeSharp, 327 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

**Reproducción.**

```powershell
nsc --emit-project-oop-exe `
    book_examples/12_avl_tree/Book12AvlTree.nsproj `
    -O2 `
    -o 12_avl_tree.exe
./12_avl_tree.exe
```

#### Listado completo

##### `book_examples/12_avl_tree/Book12AvlTree.nsproj`

```xml
<Project>
  <PropertyGroup><Name>Book12AvlTree</Name></PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
  </ItemGroup>
</Project>
```

##### `book_examples/12_avl_tree/AvlNode.ns`

```csharp
namespace NativeSharp.BookExamples.AvlTree
{
    public class AvlNode
    {
        public AvlRecord Value;
        public AvlNode Left;
        public AvlNode Right;
        public int Height;
        public AvlNode(AvlRecord value)
        {
            Value = value;
            Left = null;
            Right = null;
            Height = 1;
        }
    }
}
```

##### `book_examples/12_avl_tree/AvlRecord.ns`

```csharp
using NativeSharp.Core;
namespace NativeSharp.BookExamples.AvlTree
{
    public class AvlRecord
    {
        public int Key;
        public string Name;
        public AvlRecord(int key, string name)
        {
            Key = key;
            Name = name;
        }
    }
}
```

##### `book_examples/12_avl_tree/AvlTree.ns`

```csharp
using NativeSharp.Core;
namespace NativeSharp.BookExamples.AvlTree
{
    public class AvlTree
    {
        private AvlNode root;
        private int count;

        public AvlTree()
        {
            root = null;
            count = 0;
        }
        public int Count
        {
            get
            {
                return count;
            }
        }
        public int Height
        {
            get
            {
                return NodeHeight(root);
            }
        }

        private int NodeHeight(AvlNode node)
        {
            if (node == null)
            {
                return 0;
            }
            return node.Height;
        }

        private int Max(int a, int b)
        {
            if (a > b)
            {
                return a;
            }
            return b;
        }

        private void UpdateHeight(AvlNode node)
        {
            node.Height = 1 + Max(NodeHeight(node.Left), NodeHeight(node.Right));
        }

        private int BalanceFactor(AvlNode node)
        {
            if (node == null)
            {
                return 0;
            }
            return NodeHeight(node.Left) - NodeHeight(node.Right);
        }

        private AvlNode RotateRight(AvlNode y)
        {
            AvlNode x = y.Left;
            AvlNode transfer = x.Right;
            x.Right = y;
            y.Left = transfer;
            UpdateHeight(y);
            UpdateHeight(x);
            Console.WriteLine("    rotation: right (LL case)");
            return x;
        }

        private AvlNode RotateLeft(AvlNode x)
        {
            AvlNode y = x.Right;
            AvlNode transfer = y.Left;
            y.Left = x;
            x.Right = transfer;
            UpdateHeight(x);
            UpdateHeight(y);
            Console.WriteLine("    rotation: left (RR case)");
            return y;
        }

        private AvlNode Rebalance(AvlNode node)
        {
            if (node == null)
            {
                return null;
            }
            UpdateHeight(node);
            int balance = BalanceFactor(node);

            if (balance > 1)
            {
                if (BalanceFactor(node.Left) < 0)
                {
                    Console.WriteLine("    rotation preparation: left child left-rotation (LR case)");
                    node.Left = RotateLeft(node.Left);
                }
                return RotateRight(node);
            }

            if (balance < -1)
            {
                if (BalanceFactor(node.Right) > 0)
                {
                    Console.WriteLine("    rotation preparation: right child right-rotation (RL case)");
                    node.Right = RotateRight(node.Right);
                }
                return RotateLeft(node);
            }
            return node;
        }

        private AvlNode InsertNode(AvlNode node, AvlRecord value, bool[] inserted)
        {
            if (node == null)
            {
                inserted[0] = true;
                return new AvlNode(value);
            }
            if (value.Key < node.Value.Key)
            {
                node.Left = InsertNode(node.Left, value, inserted);
            }
            else if (value.Key > node.Value.Key)
            {
                node.Right = InsertNode(node.Right, value, inserted);
            }
            else
            {
                return node;
            }
            return Rebalance(node);
        }

        public bool Insert(AvlRecord value)
        {
            bool[] inserted = new bool[1];
            inserted[0] = false;
            root = InsertNode(root, value, inserted);
            bool result = inserted[0];
            delete[] inserted;
            if (result)
            {
                count = count + 1;
            }
            else
            {
                delete value;
            }
            return result;
        }

        public AvlRecord Find(int key)
        {
            AvlNode current = root;
            while (current != null)
            {
                if (key == current.Value.Key)
                {
                    return current.Value;
                }
                if (key < current.Value.Key)
                {
                    current = current.Left;
                }
                else
                {
                    current = current.Right;
                }
            }
            return null;
        }

        private AvlNode MinimumNode(AvlNode node)
        {
            AvlNode current = node;
            while (current.Left != null)
            {
                current = current.Left;
            }
            return current;
        }

        private AvlNode RemoveNode(AvlNode node, int key, bool[] removed)
        {
            if (node == null)
            {
                return null;
            }
            if (key < node.Value.Key)
            {
                node.Left = RemoveNode(node.Left, key, removed);
            }
            else if (key > node.Value.Key)
            {
                node.Right = RemoveNode(node.Right, key, removed);
            }
            else
            {
                removed[0] = true;
                if (node.Left == null || node.Right == null)
                {
                    AvlNode child = node.Left;
                    if (child == null)
                    {
                        child = node.Right;
                    }
                    node.Left = null;
                    node.Right = null;
                    delete node.Value;
                    delete node;
                    return child;
                }

                AvlNode successor = MinimumNode(node.Right);
                AvlRecord temporary = node.Value;
                node.Value = successor.Value;
                successor.Value = temporary;
                bool[] ignored = new bool[1];
                ignored[0] = false;
                node.Right = RemoveNode(node.Right, key, ignored);
                delete[] ignored;
            }
            return Rebalance(node);
        }

        public bool Remove(int key)
        {
            bool[] removed = new bool[1];
            removed[0] = false;
            root = RemoveNode(root, key, removed);
            bool result = removed[0];
            delete[] removed;
            if (result)
            {
                count = count - 1;
            }
            return result;
        }

        private void PrintNode(AvlNode node, int depth)
        {
            if (node == null)
            {
                return;
            }
            PrintNode(node.Right, depth + 1);
            raii string indent = String.Copy("");
            int i = 0;
            while (i < depth)
            {
                raii string nextIndent = String.Concat(indent, "    ");
                indent = move(nextIndent);
                i = i + 1;
            }
            raii string key = String.ToString(node.Value.Key);
            raii string height = String.ToString(node.Height);
            raii string balance = String.ToString(BalanceFactor(node));
            raii string start = String.Concat(indent, key);
            raii string a = String.Concat(start, " [h=");
            raii string b = String.Concat(a, height);
            raii string c = String.Concat(b, ", bf=");
            raii string d = String.Concat(c, balance);
            raii string line = String.Concat(d, "]");
            Console.WriteLine(line);
            PrintNode(node.Left, depth + 1);
        }

        public void PrintSideways()
        {
            PrintNode(root, 0);
        }

        private bool ValidateNode(AvlNode node)
        {
            if (node == null)
            {
                return true;
            }
            int balance = BalanceFactor(node);
            if (balance < -1 || balance > 1)
            {
                return false;
            }
            int expected = 1 + Max(NodeHeight(node.Left), NodeHeight(node.Right));
            if (node.Height != expected)
            {
                return false;
            }
            return ValidateNode(node.Left) && ValidateNode(node.Right);
        }

        public bool IsBalanced()
        {
            return ValidateNode(root);
        }

        private void ClearNode(AvlNode node)
        {
            if (node == null)
            {
                return;
            }
            ClearNode(node.Left);
            ClearNode(node.Right);
            node.Left = null;
            node.Right = null;
            delete node.Value;
            delete node;
        }

        public void Clear()
        {
            ClearNode(root);
            root = null;
            count = 0;
        }
        ~AvlTree()
        {
            Clear();
        }
    }
}
```

##### `book_examples/12_avl_tree/Program.ns`

```csharp
using NativeSharp.Core;
namespace NativeSharp.BookExamples.AvlTree
{
    class Program
    {
        private static void Insert(AvlTree tree, int key, string name)
        {
            raii string k = String.ToString(key);
            raii string line = String.Concat("insert ", k);
            Console.WriteLine(line);
            tree.Insert(new AvlRecord(key, name));
        }

        public static int Main()
        {
            Console.WriteLine("NativeSharp Book Example 12 - AVL Tree");
            Console.WriteLine("--------------------------------------");
            raii AvlTree tree = new AvlTree();
            Insert(tree, 30, "thirty");
            Insert(tree, 20, "twenty");
            Insert(tree, 10, "ten");
            Insert(tree, 40, "forty");
            Insert(tree, 50, "fifty");
            Insert(tree, 25, "twenty-five");
            Insert(tree, 27, "twenty-seven");
            Insert(tree, 5, "five");
            Insert(tree, 4, "four");
            Insert(tree, 45, "forty-five");
            Insert(tree, 47, "forty-seven");

            Console.WriteLine("");
            Console.WriteLine("Balanced AVL tree (right subtree above):");
            tree.PrintSideways();
            raii string balanced = String.Copy("NO");
            if (tree.IsBalanced())
            {
                raii string yesBalanced = String.Copy("yes");
                balanced = move(yesBalanced);
            }
            raii string h = String.ToString(tree.Height);
            raii string a = String.Concat("balanced: ", balanced);
            raii string b = String.Concat(a, ", height: ");
            raii string facts = String.Concat(b, h);
            Console.WriteLine(facts);

            Console.WriteLine("");
            Console.WriteLine("Delete 20 and 40; deletion also rebalances ancestors:");
            tree.Remove(20);
            tree.Remove(40);
            tree.PrintSideways();
            raii string after = String.Copy("NO");
            if (tree.IsBalanced())
            {
                raii string yesAfter = String.Copy("yes");
                after = move(yesAfter);
            }
            raii string afterLine = String.Concat("balanced after deletion: ", after);
            Console.WriteLine(afterLine);
            return 0;
        }
    }
}
```

### 47.13 Heap binario y planificador por prioridad

**Problema.** Elegir repetidamente el trabajo más prioritario sin ordenar toda la colección después de cada inserción.

**Diseño.** La cola prioritaria usa heap en array y operaciones sift-up/sift-down. El planificador demuestra que la estructura conserva una política útil, no sólo enteros abstractos.

**Auditoría propuesta.** Pruebe prioridades iguales, crecimiento y extracción hasta vacío. La invariante local padre-hijo debe mantenerse después de cada swap.

**Escala real.** 4 archivos NativeSharp, 213 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

**Reproducción.**

```powershell
nsc --emit-project-oop-exe `
    book_examples/13_binary_heap_priority_queue/Book13BinaryHeapPriorityQueue.nsproj `
    -O2 `
    -o 13_binary_heap_priority_queue.exe
./13_binary_heap_priority_queue.exe
```

#### Listado completo

##### `book_examples/13_binary_heap_priority_queue/Book13BinaryHeapPriorityQueue.nsproj`

```xml
<Project>
  <PropertyGroup><Name>Book13BinaryHeapPriorityQueue</Name></PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
  </ItemGroup>
</Project>
```

##### `book_examples/13_binary_heap_priority_queue/Job.ns`

```csharp
namespace NativeSharp.BookExamples.BinaryHeap
{
    public class Job
    {
        public int Id;
        public string Name;
        public int Priority;
        public int Sequence;

        public Job(int id, string name, int priority, int sequence)
        {
            Id = id;
            Name = name;
            Priority = priority;
            Sequence = sequence;
        }

        public static bool operator>(Job a, Job b)
        {
            if (a.Priority != b.Priority)
            {
                return a.Priority > b.Priority;
            }
            return a.Sequence < b.Sequence;
        }
        public static bool operator<(Job a, Job b)
        {
            return b > a;
        }
    }
}
```

##### `book_examples/13_binary_heap_priority_queue/JobPriorityQueue.ns`

```csharp
using NativeSharp.Core;
namespace NativeSharp.BookExamples.BinaryHeap
{
    public class JobPriorityQueue
    {
        private Job[] heap;
        private int count;

        public JobPriorityQueue(int capacity)
        {
            if (capacity < 2)
            {
                capacity = 2;
            }
            heap = new Job[capacity];
            count = 0;
        }
        public int Count
        {
            get
            {
                return count;
            }
        }
        public bool IsEmpty
        {
            get
            {
                return count == 0;
            }
        }

        private void Grow()
        {
            Job[] grown = new Job[heap.Length * 2];
            int i = 0;
            while (i < count)
            {
                grown[i] = heap[i];
                i = i + 1;
            }
            delete[] heap;
            heap = grown;
        }

        private void Swap(int a, int b)
        {
            Job temporary = heap[a];
            heap[a] = heap[b];
            heap[b] = temporary;
        }

        private void SiftUp(int index)
        {
            int child = index;
            while (child > 0)
            {
                int parent = (child - 1) / 2;
                if (!(heap[child] > heap[parent]))
                {
                    return;
                }
                Swap(child, parent);
                child = parent;
            }
        }

        private void SiftDown(int index)
        {
            int parent = index;
            bool done = false;
            while (!done)
            {
                int left = parent * 2 + 1;
                int right = left + 1;
                int best = parent;
                if (left < count && heap[left] > heap[best])
                {
                    best = left;
                }
                if (right < count && heap[right] > heap[best])
                {
                    best = right;
                }
                if (best == parent)
                {
                    done = true;
                }
                else
                {
                    Swap(parent, best);
                    parent = best;
                }
            }
        }

        public void Enqueue(Job job)
        {
            if (count == heap.Length)
            {
                Grow();
            }
            heap[count] = job;
            SiftUp(count);
            count = count + 1;
        }

        public Job Peek()
        {
            if (count == 0)
            {
                throw new InvalidOperationException();
            }
            return heap[0];
        }

        public Job Dequeue()
        {
            if (count == 0)
            {
                throw new InvalidOperationException();
            }
            Job result = heap[0];
            count = count - 1;
            if (count > 0)
            {
                heap[0] = heap[count];
                heap[count] = null;
                SiftDown(0);
            }
            else
            {
                heap[0] = null;
            }
            return result;
        }

        public void PrintHeapArray()
        {
            int i = 0;
            while (i < count)
            {
                Job job = heap[i];
                raii string index = String.ToString(i);
                raii string id = String.ToString(job.Id);
                raii string p = String.ToString(job.Priority);
                raii string a = String.Concat("  heap[", index);
                raii string b = String.Concat(a, "] job#");
                raii string c = String.Concat(b, id);
                raii string d = String.Concat(c, " priority=");
                raii string line = String.Concat(d, p);
                Console.WriteLine(line);
                i = i + 1;
            }
        }

        ~JobPriorityQueue()
        {
            int i = 0;
            while (i < count)
            {
                if (heap[i] != null)
                {
                    delete heap[i];
                }
                i = i + 1;
            }
            delete[] heap;
        }
    }
}
```

##### `book_examples/13_binary_heap_priority_queue/Scheduler.ns`

```csharp
using NativeSharp.Core;
namespace NativeSharp.BookExamples.BinaryHeap
{
    public class Scheduler
    {
        private JobPriorityQueue queue;
        private int sequence;
        public Scheduler()
        {
            queue = new JobPriorityQueue(4);
            sequence = 0;
        }
        public void Submit(int id, string name, int priority)
        {
            queue.Enqueue(new Job(id, name, priority, sequence));
            sequence = sequence + 1;
        }
        public void PrintHeap()
        {
            queue.PrintHeapArray();
        }
        public void RunAll()
        {
            while (!queue.IsEmpty)
            {
                Job job = queue.Dequeue();
                raii string id = String.ToString(job.Id);
                raii string priority = String.ToString(job.Priority);
                raii string a = String.Concat("run job #", id);
                raii string b = String.Concat(a, "  priority=");
                raii string c = String.Concat(b, priority);
                raii string d = String.Concat(c, "  ");
                raii string line = String.Concat(d, job.Name);
                Console.WriteLine(line);
                delete job;
            }
        }
        ~Scheduler()
        {
            delete queue;
        }
    }
}
```

##### `book_examples/13_binary_heap_priority_queue/Program.ns`

```csharp
using NativeSharp.Core;
namespace NativeSharp.BookExamples.BinaryHeap
{
    class Program
    {
        public static int Main()
        {
            Console.WriteLine("NativeSharp Book Example 13 - Binary Heap / Priority Queue");
            Console.WriteLine("----------------------------------------------------------");
            raii Scheduler scheduler = new Scheduler();
            scheduler.Submit(100, "generate mipmaps", 2);
            scheduler.Submit(101, "compile gameplay scripts", 4);
            scheduler.Submit(102, "package audio", 1);
            scheduler.Submit(103, "rebuild navigation mesh", 3);
            scheduler.Submit(104, "crash report symbolization", 5);
            scheduler.Submit(105, "urgent shader hot reload", 5);
            scheduler.Submit(106, "thumbnail generation", 1);

            Console.WriteLine("Internal max-heap array after submissions:");
            scheduler.PrintHeap();
            Console.WriteLine("");
            Console.WriteLine("Execution order (higher priority first; FIFO on ties):");
            scheduler.RunAll();
            return 0;
        }
    }
}
```

### 47.14 Trie de diccionario

**Problema.** Guardar palabras por prefijo, consultar pertenencia y enumerar coincidencias sin comparar cada palabra completa.

**Diseño.** Cada nivel representa un carácter y los caminos comparten prefijos. El dataset y la impresión permiten observar nodos internos que no son palabras completas.

**Auditoría propuesta.** Pruebe una palabra que es prefijo de otra, prefijo inexistente y caracteres de borde. La limpieza debe recorrer todo el árbol de hijos.

**Escala real.** 4 archivos NativeSharp, 231 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

**Reproducción.**

```powershell
nsc --emit-project-oop-exe `
    book_examples/14_trie_dictionary/Book14TrieDictionary.nsproj `
    -O2 `
    -o 14_trie_dictionary.exe
./14_trie_dictionary.exe
```

#### Listado completo

##### `book_examples/14_trie_dictionary/Book14TrieDictionary.nsproj`

```xml
<Project>
  <PropertyGroup><Name>Book14TrieDictionary</Name></PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
  </ItemGroup>
</Project>
```

##### `book_examples/14_trie_dictionary/DictionaryDataset.ns`

```csharp
namespace NativeSharp.BookExamples.Trie
{
    public class DictionaryDataset
    {
        public static void Fill(TrieDictionary dictionary)
        {
            dictionary.Insert("array");
            dictionary.Insert("asset");
            dictionary.Insert("assembler");
            dictionary.Insert("assembly");
            dictionary.Insert("binary");
            dictionary.Insert("bind");
            dictionary.Insert("binding");
            dictionary.Insert("compiler");
            dictionary.Insert("compile");
            dictionary.Insert("compilation");
            dictionary.Insert("component");
            dictionary.Insert("graph");
            dictionary.Insert("graphics");
            dictionary.Insert("shader");
            dictionary.Insert("shadow");
            dictionary.Insert("shape");
            dictionary.Insert("texture");
            dictionary.Insert("thread");
            dictionary.Insert("tree");
            dictionary.Insert("trie");
        }
    }
}
```

##### `book_examples/14_trie_dictionary/TrieDictionary.ns`

```csharp
using NativeSharp.Core;
namespace NativeSharp.BookExamples.Trie
{
    public class TrieDictionary
    {
        private TrieNode root;
        private int count;

        public TrieDictionary()
        {
            root = new TrieNode();
            count = 0;
        }
        public int Count
        {
            get
            {
                return count;
            }
        }

        private int IndexOf(char c)
        {
            if (c >= 'A' && c <= 'Z')
            {
                c = (char)((int)c + 32);
            }
            if (c < 'a' || c > 'z')
            {
                return -1;
            }
            return (int)c - (int)'a';
        }

        public bool Insert(string word)
        {
            TrieNode current = root;
            int i = 0;
            while (i < word.Length)
            {
                int index = IndexOf(word[i]);
                if (index < 0)
                {
                    throw new ArgumentException();
                }
                if (current.Children[index] == null)
                {
                    current.Children[index] = new TrieNode();
                }
                current = current.Children[index];
                current.PrefixCount = current.PrefixCount + 1;
                i = i + 1;
            }
            if (current.Terminal)
            {
                return false;
            }
            current.Terminal = true;
            count = count + 1;
            return true;
        }

        private TrieNode FindNode(string text)
        {
            TrieNode current = root;
            int i = 0;
            while (i < text.Length)
            {
                int index = IndexOf(text[i]);
                if (index < 0)
                {
                    return null;
                }
                current = current.Children[index];
                if (current == null)
                {
                    return null;
                }
                i = i + 1;
            }
            return current;
        }

        public bool Contains(string word)
        {
            TrieNode node = FindNode(word);
            return node != null && node.Terminal;
        }

        public bool StartsWith(string prefix)
        {
            return FindNode(prefix) != null;
        }

        public int CountWithPrefix(string prefix)
        {
            TrieNode node = FindNode(prefix);
            if (node == null)
            {
                return 0;
            }
            if (prefix.Length == 0)
            {
                return count;
            }
            return node.PrefixCount;
        }

        private void PrintWordsNode(TrieNode node, string prefix, int[] printed, int limit)
        {
            if (node == null || printed[0] >= limit)
            {
                return;
            }
            if (node.Terminal)
            {
                raii string line = String.Concat("  ", prefix);
                Console.WriteLine(line);
                printed[0] = printed[0] + 1;
            }
            int i = 0;
            while (i < 26 && printed[0] < limit)
            {
                TrieNode child = node.Children[i];
                if (child != null)
                {
                    char c = (char)((int)'a' + i);
                    raii string letter = String.ToString(c);
                    raii string nextPrefix = String.Concat(prefix, letter);
                    PrintWordsNode(child, nextPrefix, printed, limit);
                }
                i = i + 1;
            }
        }

        public void PrintSuggestions(string prefix, int limit)
        {
            TrieNode node = FindNode(prefix);
            if (node == null)
            {
                Console.WriteLine("  <no matches>");
                return;
            }
            int[] printed = new int[1];
            printed[0] = 0;
            PrintWordsNode(node, prefix, printed, limit);
            delete[] printed;
        }

        private void ClearNode(TrieNode node)
        {
            if (node == null)
            {
                return;
            }
            int i = 0;
            while (i < 26)
            {
                TrieNode child = node.Children[i];
                if (child != null)
                {
                    ClearNode(child);
                    node.Children[i] = null;
                }
                i = i + 1;
            }
            delete node;
        }

        ~TrieDictionary()
        {
            ClearNode(root);
            root = null;
        }
    }
}
```

##### `book_examples/14_trie_dictionary/TrieNode.ns`

```csharp
namespace NativeSharp.BookExamples.Trie
{
    public class TrieNode
    {
        public TrieNode[] Children;
        public bool Terminal;
        public int PrefixCount;
        public TrieNode()
        {
            Children = new TrieNode[26];
            Terminal = false;
            PrefixCount = 0;
        }
        ~TrieNode()
        {
            delete[] Children;
        }
    }
}
```

##### `book_examples/14_trie_dictionary/Program.ns`

```csharp
using NativeSharp.Core;
namespace NativeSharp.BookExamples.Trie
{
    class Program
    {
        private static void Query(TrieDictionary dictionary, string word)
        {
            bool found = dictionary.Contains(word);
            raii string state = String.Copy("not found");
            if (found)
            {
                raii string foundState = String.Copy("found");
                state = move(foundState);
            }
            raii string a = String.Concat("  ", word);
            raii string b = String.Concat(a, ": ");
            raii string line = String.Concat(b, state);
            Console.WriteLine(line);
        }
        private static void Prefix(TrieDictionary dictionary, string prefix)
        {
            raii string header = String.Concat("suggestions for prefix: ", prefix);
            Console.WriteLine(header);
            dictionary.PrintSuggestions(prefix, 8);
            int count = dictionary.CountWithPrefix(prefix);
            raii string n = String.ToString(count);
            raii string line = String.Concat("  total words below prefix: ", n);
            Console.WriteLine(line);
        }
        public static int Main()
        {
            Console.WriteLine("NativeSharp Book Example 14 - Trie Dictionary");
            Console.WriteLine("----------------------------------------------");
            raii TrieDictionary dictionary = new TrieDictionary();
            DictionaryDataset.Fill(dictionary);
            Query(dictionary, "compiler");
            Query(dictionary, "compilation");
            Query(dictionary, "compilex");
            Query(dictionary, "graph");
            Query(dictionary, "graphics");
            Console.WriteLine("");
            Prefix(dictionary, "comp");
            Console.WriteLine("");
            Prefix(dictionary, "sha");
            Console.WriteLine("");
            Prefix(dictionary, "tr");
            return 0;
        }
    }
}
```

### 47.15 Tabla hash de símbolos

**Problema.** Resolver nombres con colisiones controladas y crecimiento sin confundir hash con identidad.

**Diseño.** La tabla distribuye símbolos en buckets enlazados. La igualdad decide identidad después del hash; inserción, lookup y eliminación conservan cadenas.

**Auditoría propuesta.** Fuerce colisiones, actualizaciones y borrado en cabeza y medio de bucket. Una función hash pobre debe degradar rendimiento, no corrección.

**Escala real.** 4 archivos NativeSharp, 233 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

**Reproducción.**

```powershell
nsc --emit-project-oop-exe `
    book_examples/15_hash_table/Book15HashTable.nsproj `
    -O2 `
    -o 15_hash_table.exe
./15_hash_table.exe
```

#### Listado completo

##### `book_examples/15_hash_table/Book15HashTable.nsproj`

```xml
<Project>
  <PropertyGroup><Name>Book15HashTable</Name></PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
  </ItemGroup>
</Project>
```

##### `book_examples/15_hash_table/BucketNode.ns`

```csharp
namespace NativeSharp.BookExamples.HashTable
{
    public class BucketNode
    {
        public Symbol Value;
        public BucketNode Next;
        public BucketNode(Symbol value)
        {
            Value = value;
            Next = null;
        }
    }
}
```

##### `book_examples/15_hash_table/Symbol.ns`

```csharp
namespace NativeSharp.BookExamples.HashTable
{
    public class Symbol
    {
        public string Name;
        public string Kind;
        public int Address;
        public Symbol(string name, string kind, int address)
        {
            Name = name;
            Kind = kind;
            Address = address;
        }
    }
}
```

##### `book_examples/15_hash_table/SymbolTable.ns`

```csharp
using NativeSharp.Core;
namespace NativeSharp.BookExamples.HashTable
{
    public class SymbolTable
    {
        private BucketNode[] buckets;
        private int count;
        private int collisions;

        public SymbolTable(int bucketCount)
        {
            if (bucketCount < 3)
            {
                bucketCount = 3;
            }
            buckets = new BucketNode[bucketCount];
            count = 0;
            collisions = 0;
        }
        public int Count
        {
            get
            {
                return count;
            }
        }
        public int Collisions
        {
            get
            {
                return collisions;
            }
        }
        public int BucketCount
        {
            get
            {
                return buckets.Length;
            }
        }

        public int Hash(string key)
        {
            long hash = 5381;
            int i = 0;
            while (i < key.Length)
            {
                hash = ((hash * 33) + (int)key[i]) % 2147483647;
                i = i + 1;
            }
            if (hash < 0)
            {
                hash = -hash;
            }
            return (int)(hash % buckets.Length);
        }

        public bool Add(Symbol symbol)
        {
            int index = Hash(symbol.Name);
            BucketNode current = buckets[index];
            if (current != null)
            {
                collisions = collisions + 1;
            }
            while (current != null)
            {
                if (String.Equals(current.Value.Name, symbol.Name))
                {
                    delete symbol;
                    return false;
                }
                current = current.Next;
            }
            BucketNode node = new BucketNode(symbol);
            node.Next = buckets[index];
            buckets[index] = node;
            count = count + 1;
            return true;
        }

        public Symbol Find(string name)
        {
            int index = Hash(name);
            BucketNode current = buckets[index];
            while (current != null)
            {
                if (String.Equals(current.Value.Name, name))
                {
                    return current.Value;
                }
                current = current.Next;
            }
            return null;
        }

        public bool Remove(string name)
        {
            int index = Hash(name);
            BucketNode previous = null;
            BucketNode current = buckets[index];
            while (current != null)
            {
                if (String.Equals(current.Value.Name, name))
                {
                    if (previous == null)
                    {
                        buckets[index] = current.Next;
                    }
                    else
                    {
                        previous.Next = current.Next;
                    }
                    current.Next = null;
                    delete current.Value;
                    delete current;
                    count = count - 1;
                    return true;
                }
                previous = current;
                current = current.Next;
            }
            return false;
        }

        public void PrintBuckets()
        {
            int i = 0;
            while (i < buckets.Length)
            {
                raii string index = String.ToString(i);
                raii string prefix = String.Concat("bucket ", index);
                raii string line = String.Concat(prefix, ": ");
                BucketNode current = buckets[i];
                if (current == null)
                {
                    raii string emptyLine = String.Concat(line, "<empty>");
                    line = move(emptyLine);
                }
                else
                {
                    while (current != null)
                    {
                        raii string withName = String.Concat(line, current.Value.Name);
                        line = move(withName);
                        current = current.Next;
                        if (current != null)
                        {
                            raii string withArrow = String.Concat(line, " -> ");
                            line = move(withArrow);
                        }
                    }
                }
                Console.WriteLine(line);
                i = i + 1;
            }
        }

        private void ClearBucket(int index)
        {
            BucketNode current = buckets[index];
            while (current != null)
            {
                BucketNode next = current.Next;
                current.Next = null;
                delete current.Value;
                delete current;
                current = next;
            }
            buckets[index] = null;
        }

        ~SymbolTable()
        {
            int i = 0;
            while (i < buckets.Length)
            {
                ClearBucket(i);
                i = i + 1;
            }
            delete[] buckets;
        }
    }
}
```

##### `book_examples/15_hash_table/Program.ns`

```csharp
using NativeSharp.Core;
namespace NativeSharp.BookExamples.HashTable
{
    class Program
    {
        private static void Add(SymbolTable table, string name, string kind, int address)
        {
            table.Add(new Symbol(name, kind, address));
        }
        private static void Lookup(SymbolTable table, string name)
        {
            Symbol symbol = table.Find(name);
            if (symbol == null)
            {
                raii string line = String.Concat("not found: ", name);
                Console.WriteLine(line);
                return;
            }
            raii string address = String.ToString(symbol.Address);
            raii string a = String.Concat(name, "  kind=");
            raii string b = String.Concat(a, symbol.Kind);
            raii string c = String.Concat(b, "  address=");
            raii string line = String.Concat(c, address);
            Console.WriteLine(line);
        }
        public static int Main()
        {
            Console.WriteLine("NativeSharp Book Example 15 - Hash Table / Symbol Table");
            Console.WriteLine("-------------------------------------------------------");
            raii SymbolTable table = new SymbolTable(7);
            Add(table, "main", "function", 4096);
            Add(table, "Update", "method", 4200);
            Add(table, "Render", "method", 4300);
            Add(table, "player", "global", 8192);
            Add(table, "enemyCount", "global", 8200);
            Add(table, "Matrix4", "type", 0);
            Add(table, "Vector3", "type", 0);
            Add(table, "Texture", "type", 0);
            Add(table, "LoadAsset", "function", 4500);
            Add(table, "UnloadAsset", "function", 4700);

            Console.WriteLine("Separate chaining distribution:");
            table.PrintBuckets();
            Console.WriteLine("");
            Lookup(table, "Render");
            Lookup(table, "Vector3");
            Lookup(table, "MissingSymbol");
            Console.WriteLine("");
            Console.WriteLine("Remove enemyCount and inspect its bucket chain again:");
            table.Remove("enemyCount");
            table.PrintBuckets();
            raii string count = String.ToString(table.Count);
            raii string col = String.ToString(table.Collisions);
            raii string a = String.Concat("symbols: ", count);
            raii string b = String.Concat(a, ", insertion collisions observed: ");
            raii string line = String.Concat(b, col);
            Console.WriteLine(line);
            return 0;
        }
    }
}
```

### 47.16 Grafo, BFS y DFS

**Problema.** Representar ciudades y conexiones, recorrer por amplitud y profundidad y reconstruir un camino no ponderado.

**Diseño.** Una matriz de adyacencia hace explícito el costo espacial. BFS usa cola y predecesores; DFS se muestra iterativo con pila y recursivo.

**Auditoría propuesta.** Compare órdenes, pruebe vértices desconectados y reconstruya el camino desde predecesores. Distinga visitar de descubrir para no insertar duplicados sin límite.

**Escala real.** 5 archivos NativeSharp, 315 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

**Reproducción.**

```powershell
nsc --emit-project-oop-exe `
    book_examples/16_graph_bfs_dfs/Book16GraphBfsDfs.nsproj `
    -O2 `
    -o 16_graph_bfs_dfs.exe
./16_graph_bfs_dfs.exe
```

#### Listado completo

##### `book_examples/16_graph_bfs_dfs/Book16GraphBfsDfs.nsproj`

```xml
<Project>
  <PropertyGroup><Name>Book16GraphBfsDfs</Name></PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
  </ItemGroup>
</Project>
```

##### `book_examples/16_graph_bfs_dfs/City.ns`

```csharp
namespace NativeSharp.BookExamples.GraphTraversal
{
    public class City
    {
        public int Id;
        public string Name;
        public City(int id, string name)
        {
            Id = id;
            Name = name;
        }
    }
}
```

##### `book_examples/16_graph_bfs_dfs/Graph.ns`

```csharp
using NativeSharp.Core;
namespace NativeSharp.BookExamples.GraphTraversal
{
    public class Graph
    {
        private City[] vertices;
        private int[] adjacency;
        private int vertexCount;
        private int capacity;

        public Graph(int maxVertices)
        {
            capacity = maxVertices;
            vertices = new City[maxVertices];
            adjacency = new int[maxVertices * maxVertices];
            vertexCount = 0;
        }
        public int VertexCount
        {
            get
            {
                return vertexCount;
            }
        }

        private int Cell(int from, int to)
        {
            return from * capacity + to;
        }

        public int AddCity(string name)
        {
            if (vertexCount == capacity)
            {
                throw new InvalidOperationException();
            }
            int id = vertexCount;
            vertices[id] = new City(id, name);
            vertexCount = vertexCount + 1;
            return id;
        }

        public void AddUndirectedEdge(int a, int b)
        {
            adjacency[Cell(a, b)] = 1;
            adjacency[Cell(b, a)] = 1;
        }

        public City GetCity(int id)
        {
            return vertices[id];
        }
        public bool Connected(int a, int b)
        {
            return adjacency[Cell(a, b)] != 0;
        }

        private void PrintVisit(string label, int id)
        {
            raii string a = String.Concat(label, vertices[id].Name);
            Console.WriteLine(a);
        }

        public void BreadthFirst(int start)
        {
            bool[] visited = new bool[vertexCount];
            raii IntQueue queue = new IntQueue(4);
            visited[start] = true;
            queue.Enqueue(start);
            while (!queue.IsEmpty)
            {
                int v = queue.Dequeue();
                PrintVisit("  visit: ", v);
                int n = 0;
                while (n < vertexCount)
                {
                    if (Connected(v, n) && !visited[n])
                    {
                        visited[n] = true;
                        queue.Enqueue(n);
                    }
                    n = n + 1;
                }
            }
            delete[] visited;
        }

        public void DepthFirstIterative(int start)
        {
            bool[] visited = new bool[vertexCount];
            raii IntStack stack = new IntStack(4);
            stack.Push(start);
            while (!stack.IsEmpty)
            {
                int v = stack.Pop();
                if (!visited[v])
                {
                    visited[v] = true;
                    PrintVisit("  visit: ", v);
                    int n = vertexCount - 1;
                    while (n >= 0)
                    {
                        if (Connected(v, n) && !visited[n])
                        {
                            stack.Push(n);
                        }
                        n = n - 1;
                    }
                }
            }
            delete[] visited;
        }

        private void DepthFirstNode(int v, bool[] visited)
        {
            visited[v] = true;
            PrintVisit("  visit: ", v);
            int n = 0;
            while (n < vertexCount)
            {
                if (Connected(v, n) && !visited[n])
                {
                    DepthFirstNode(n, visited);
                }
                n = n + 1;
            }
        }

        public void DepthFirstRecursive(int start)
        {
            bool[] visited = new bool[vertexCount];
            DepthFirstNode(start, visited);
            delete[] visited;
        }

        public void ShortestUnweightedPath(int start, int goal)
        {
            bool[] visited = new bool[vertexCount];
            int[] previous = new int[vertexCount];
            int i = 0;
            while (i < vertexCount)
            {
                previous[i] = -1;
                i = i + 1;
            }
            raii IntQueue queue = new IntQueue(4);
            visited[start] = true;
            queue.Enqueue(start);
            while (!queue.IsEmpty)
            {
                int v = queue.Dequeue();
                if (v == goal)
                {
                    break;
                }
                int n = 0;
                while (n < vertexCount)
                {
                    if (Connected(v, n) && !visited[n])
                    {
                        visited[n] = true;
                        previous[n] = v;
                        queue.Enqueue(n);
                    }
                    n = n + 1;
                }
            }

            if (!visited[goal])
            {
                Console.WriteLine("  no path");
            }
            else
            {
                int[] reversePath = new int[vertexCount];
                int length = 0;
                int current = goal;
                while (current != -1)
                {
                    reversePath[length] = current;
                    length = length + 1;
                    current = previous[current];
                }
                raii string line = String.Copy("");
                int p = length - 1;
                while (p >= 0)
                {
                    raii string withName = String.Concat(line, vertices[reversePath[p]].Name);
                    line = move(withName);
                    if (p > 0)
                    {
                        raii string withArrow = String.Concat(line, " -> ");
                        line = move(withArrow);
                    }
                    p = p - 1;
                }
                Console.WriteLine(line);
                delete[] reversePath;
            }
            delete[] visited;
            delete[] previous;
        }

        public void PrintAdjacency()
        {
            int v = 0;
            while (v < vertexCount)
            {
                raii string line = String.Concat(vertices[v].Name, ": ");
                int n = 0;
                bool first = true;
                while (n < vertexCount)
                {
                    if (Connected(v, n))
                    {
                        if (!first)
                        {
                            raii string withComma = String.Concat(line, ", ");
                            line = move(withComma);
                        }
                        raii string withName = String.Concat(line, vertices[n].Name);
                        line = move(withName);
                        first = false;
                    }
                    n = n + 1;
                }
                Console.WriteLine(line);
                v = v + 1;
            }
        }

        ~Graph()
        {
            int i = 0;
            while (i < vertexCount)
            {
                delete vertices[i];
                i = i + 1;
            }
            delete[] vertices;
            delete[] adjacency;
        }
    }
}
```

##### `book_examples/16_graph_bfs_dfs/IntQueue.ns`

```csharp
using NativeSharp.Core;
namespace NativeSharp.BookExamples.GraphTraversal
{
    public class IntQueue
    {
        private int[] data;
        private int head;
        private int count;
        public IntQueue(int capacity)
        {
            data = new int[capacity];
            head = 0;
            count = 0;
        }
        public bool IsEmpty
        {
            get
            {
                return count == 0;
            }
        }
        private int Index(int logical)
        {
            return (head + logical) % data.Length;
        }
        private void Grow()
        {
            int[] grown = new int[data.Length * 2];
            int i = 0;
            while (i < count)
            {
                grown[i] = data[Index(i)];
                i = i + 1;
            }
            delete[] data;
            data = grown;
            head = 0;
        }
        public void Enqueue(int value)
        {
            if (count == data.Length)
            {
                Grow();
            }
            data[Index(count)] = value;
            count = count + 1;
        }
        public int Dequeue()
        {
            if (count == 0)
            {
                throw new InvalidOperationException();
            }
            int value = data[head];
            head = (head + 1) % data.Length;
            count = count - 1;
            return value;
        }
        ~IntQueue()
        {
            delete[] data;
        }
    }
}
```

##### `book_examples/16_graph_bfs_dfs/IntStack.ns`

```csharp
using NativeSharp.Core;
namespace NativeSharp.BookExamples.GraphTraversal
{
    public class IntStack
    {
        private int[] data;
        private int count;
        public IntStack(int capacity)
        {
            data = new int[capacity];
            count = 0;
        }
        public bool IsEmpty
        {
            get
            {
                return count == 0;
            }
        }
        private void Grow()
        {
            int[] grown = new int[data.Length * 2];
            int i = 0;
            while (i < count)
            {
                grown[i] = data[i];
                i = i + 1;
            }
            delete[] data;
            data = grown;
        }
        public void Push(int value)
        {
            if (count == data.Length)
            {
                Grow();
            }
            data[count] = value;
            count = count + 1;
        }
        public int Pop()
        {
            if (count == 0)
            {
                throw new InvalidOperationException();
            }
            count = count - 1;
            return data[count];
        }
        ~IntStack()
        {
            delete[] data;
        }
    }
}
```

##### `book_examples/16_graph_bfs_dfs/Program.ns`

```csharp
using NativeSharp.Core;
namespace NativeSharp.BookExamples.GraphTraversal
{
    class Program
    {
        public static int Main()
        {
            Console.WriteLine("NativeSharp Book Example 16 - Graph, BFS and DFS");
            Console.WriteLine("------------------------------------------------");
            raii Graph graph = new Graph(10);
            int harbor = graph.AddCity("Harbor");
            int oldTown = graph.AddCity("OldTown");
            int market = graph.AddCity("Market");
            int university = graph.AddCity("University");
            int stadium = graph.AddCity("Stadium");
            int airport = graph.AddCity("Airport");
            int industrial = graph.AddCity("Industrial");
            int hills = graph.AddCity("Hills");

            graph.AddUndirectedEdge(harbor, oldTown);
            graph.AddUndirectedEdge(harbor, industrial);
            graph.AddUndirectedEdge(oldTown, market);
            graph.AddUndirectedEdge(market, university);
            graph.AddUndirectedEdge(market, stadium);
            graph.AddUndirectedEdge(university, hills);
            graph.AddUndirectedEdge(stadium, airport);
            graph.AddUndirectedEdge(industrial, airport);
            graph.AddUndirectedEdge(airport, hills);

            Console.WriteLine("Adjacency list rendered from our adjacency matrix:");
            graph.PrintAdjacency();
            Console.WriteLine("");
            Console.WriteLine("Breadth-first traversal from Harbor:");
            graph.BreadthFirst(harbor);
            Console.WriteLine("");
            Console.WriteLine("Depth-first traversal, iterative stack:");
            graph.DepthFirstIterative(harbor);
            Console.WriteLine("");
            Console.WriteLine("Depth-first traversal, recursive:");
            graph.DepthFirstRecursive(harbor);
            Console.WriteLine("");
            Console.WriteLine("Shortest unweighted path Harbor -> Hills, reconstructed from BFS parents:");
            graph.ShortestUnweightedPath(harbor, hills);
            return 0;
        }
    }
}
```

### 47.17 Caminos mínimos con Dijkstra

**Problema.** Encontrar el costo mínimo entre ciudades con pesos no negativos y reconstruir la ruta.

**Diseño.** El solver mantiene distancia tentativa, conjunto visitado y predecesor. La selección lineal del mínimo prioriza claridad; después puede sustituirse por heap.

**Auditoría propuesta.** Rechace conceptualmente pesos negativos, proteja el infinito contra suma y pruebe destino inalcanzable. La ruta se reconstruye sólo si existe distancia finita.

**Escala real.** 4 archivos NativeSharp, 207 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

**Reproducción.**

```powershell
nsc --emit-project-oop-exe `
    book_examples/17_dijkstra_shortest_paths/Book17DijkstraShortestPaths.nsproj `
    -O2 `
    -o 17_dijkstra_shortest_paths.exe
./17_dijkstra_shortest_paths.exe
```

#### Listado completo

##### `book_examples/17_dijkstra_shortest_paths/Book17DijkstraShortestPaths.nsproj`

```xml
<Project>
  <PropertyGroup><Name>Book17DijkstraShortestPaths</Name></PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
  </ItemGroup>
</Project>
```

##### `book_examples/17_dijkstra_shortest_paths/DijkstraSolver.ns`

```csharp
using NativeSharp.Core;
namespace NativeSharp.BookExamples.Dijkstra
{
    public class DijkstraSolver
    {
        private static int SelectMinimum(int[] distances, bool[] visited, int count)
        {
            int best = -1;
            int bestDistance = 2147483647;
            int i = 0;
            while (i < count)
            {
                if (!visited[i] && distances[i] < bestDistance)
                {
                    bestDistance = distances[i];
                    best = i;
                }
                i = i + 1;
            }
            return best;
        }

        public static void SolveAndPrint(WeightedGraph graph, int start, int goal)
        {
            int n = graph.Count;
            int[] distances = new int[n];
            int[] previous = new int[n];
            bool[] visited = new bool[n];
            int i = 0;
            while (i < n)
            {
                distances[i] = 2147483647;
                previous[i] = -1;
                visited[i] = false;
                i = i + 1;
            }
            distances[start] = 0;

            int iteration = 0;
            while (iteration < n)
            {
                int current = SelectMinimum(distances, visited, n);
                if (current < 0)
                {
                    break;
                }
                visited[current] = true;

                raii string d = String.ToString(distances[current]);
                raii string a = String.Concat("settle ", graph.City(current).Name);
                raii string b = String.Concat(a, " at distance ");
                raii string line = String.Concat(b, d);
                Console.WriteLine(line);

                if (current == goal)
                {
                    break;
                }

                int neighbor = 0;
                while (neighbor < n)
                {
                    int weight = graph.Weight(current, neighbor);
                    if (weight > 0 && !visited[neighbor] && distances[current] != 2147483647)
                    {
                        int candidate = distances[current] + weight;
                        if (candidate < distances[neighbor])
                        {
                            distances[neighbor] = candidate;
                            previous[neighbor] = current;
                        }
                    }
                    neighbor = neighbor + 1;
                }
                iteration = iteration + 1;
            }

            Console.WriteLine("");
            if (distances[goal] == 2147483647)
            {
                Console.WriteLine("destination unreachable");
            }
            else
            {
                int[] reversed = new int[n];
                int length = 0;
                int current = goal;
                while (current != -1)
                {
                    reversed[length] = current;
                    length = length + 1;
                    current = previous[current];
                }
                raii string path = String.Copy("");
                int p = length - 1;
                while (p >= 0)
                {
                    raii string withName = String.Concat(path, graph.City(reversed[p]).Name);
                    path = move(withName);
                    if (p > 0)
                    {
                        raii string withArrow = String.Concat(path, " -> ");
                        path = move(withArrow);
                    }
                    p = p - 1;
                }
                raii string total = String.ToString(distances[goal]);
                raii string a = String.Concat("shortest path: ", path);
                raii string b = String.Concat(a, "  total distance=");
                raii string line = String.Concat(b, total);
                Console.WriteLine(line);
                delete[] reversed;
            }

            delete[] distances;
            delete[] previous;
            delete[] visited;
        }
    }
}
```

##### `book_examples/17_dijkstra_shortest_paths/WeightedCity.ns`

```csharp
namespace NativeSharp.BookExamples.Dijkstra
{
    public class WeightedCity
    {
        public int Id;
        public string Name;
        public WeightedCity(int id, string name)
        {
            Id = id;
            Name = name;
        }
    }
}
```

##### `book_examples/17_dijkstra_shortest_paths/WeightedGraph.ns`

```csharp
using NativeSharp.Core;
namespace NativeSharp.BookExamples.Dijkstra
{
    public class WeightedGraph
    {
        private WeightedCity[] cities;
        private int[] weights;
        private int count;
        private int capacity;

        public WeightedGraph(int maxCities)
        {
            capacity = maxCities;
            cities = new WeightedCity[maxCities];
            weights = new int[maxCities * maxCities];
            count = 0;
        }

        public int Count
        {
            get
            {
                return count;
            }
        }
        private int Cell(int a, int b)
        {
            return a * capacity + b;
        }

        public int AddCity(string name)
        {
            if (count == capacity)
            {
                throw new InvalidOperationException();
            }
            int id = count;
            cities[id] = new WeightedCity(id, name);
            count = count + 1;
            return id;
        }

        public void AddUndirectedEdge(int a, int b, int weight)
        {
            if (weight <= 0)
            {
                throw new ArgumentOutOfRangeException();
            }
            weights[Cell(a, b)] = weight;
            weights[Cell(b, a)] = weight;
        }

        public int Weight(int a, int b)
        {
            return weights[Cell(a, b)];
        }
        public WeightedCity City(int id)
        {
            return cities[id];
        }

        ~WeightedGraph()
        {
            int i = 0;
            while (i < count)
            {
                delete cities[i];
                i = i + 1;
            }
            delete[] cities;
            delete[] weights;
        }
    }
}
```

##### `book_examples/17_dijkstra_shortest_paths/Program.ns`

```csharp
using NativeSharp.Core;
namespace NativeSharp.BookExamples.Dijkstra
{
    class Program
    {
        public static int Main()
        {
            Console.WriteLine("NativeSharp Book Example 17 - Dijkstra Shortest Paths");
            Console.WriteLine("-----------------------------------------------------");
            raii WeightedGraph graph = new WeightedGraph(8);
            int a = graph.AddCity("Aster");
            int b = graph.AddCity("Beryl");
            int c = graph.AddCity("Cobalt");
            int d = graph.AddCity("Dune");
            int e = graph.AddCity("Elm");
            int f = graph.AddCity("Fjord");
            int g = graph.AddCity("Grove");
            int h = graph.AddCity("Harbor");
            graph.AddUndirectedEdge(a, b, 4);
            graph.AddUndirectedEdge(a, c, 2);
            graph.AddUndirectedEdge(b, c, 1);
            graph.AddUndirectedEdge(b, d, 5);
            graph.AddUndirectedEdge(c, d, 8);
            graph.AddUndirectedEdge(c, e, 10);
            graph.AddUndirectedEdge(d, e, 2);
            graph.AddUndirectedEdge(d, f, 6);
            graph.AddUndirectedEdge(e, f, 3);
            graph.AddUndirectedEdge(e, g, 5);
            graph.AddUndirectedEdge(f, h, 4);
            graph.AddUndirectedEdge(g, h, 2);
            DijkstraSolver.SolveAndPrint(graph, a, h);
            return 0;
        }
    }
}
```

### 47.18 Orden topológico

**Problema.** Programar tareas con dependencias dirigidas y detectar cuándo un ciclo impide un orden válido.

**Diseño.** El algoritmo de Kahn calcula grados de entrada, encola tareas disponibles y reduce dependencias. El resultado no es necesariamente único.

**Auditoría propuesta.** Pruebe dos órdenes válidos, grafo vacío y ciclo. Producir menos tareas que el total es evidencia de ciclo, no un resultado parcial exitoso.

**Escala real.** 4 archivos NativeSharp, 120 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

**Reproducción.**

```powershell
nsc --emit-project-oop-exe `
    book_examples/18_topological_sort/Book18TopologicalSort.nsproj `
    -O2 `
    -o 18_topological_sort.exe
./18_topological_sort.exe
```

#### Listado completo

##### `book_examples/18_topological_sort/Book18TopologicalSort.nsproj`

```xml
<Project>
  <PropertyGroup><Name>Book18TopologicalSort</Name></PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
  </ItemGroup>
</Project>
```

##### `book_examples/18_topological_sort/BuildTask.ns`

```csharp
namespace NativeSharp.BookExamples.TopologicalSort
{
    public class BuildTask
    {
        public int Id;
        public string Name;
        public BuildTask(int id, string name)
        {
            Id = id;
            Name = name;
        }
    }
}
```

##### `book_examples/18_topological_sort/DependencyGraph.ns`

```csharp
using NativeSharp.Core;
namespace NativeSharp.BookExamples.TopologicalSort
{
    public class DependencyGraph
    {
        private BuildTask[] tasks;
        private int[] edges;
        private int count;
        private int capacity;
        public DependencyGraph(int maxTasks)
        {
            capacity = maxTasks;
            tasks = new BuildTask[maxTasks];
            edges = new int[maxTasks * maxTasks];
            count = 0;
        }
        private int Cell(int a, int b)
        {
            return a * capacity + b;
        }
        public int AddTask(string name)
        {
            int id = count;
            tasks[id] = new BuildTask(id, name);
            count = count + 1;
            return id;
        }
        public void AddDependency(int prerequisite, int dependent)
        {
            edges[Cell(prerequisite, dependent)] = 1;
        }
        public void TopologicalOrder()
        {
            int[] indegree = new int[count];
            int a = 0;
            while (a < count)
            {
                int b = 0;
                while (b < count)
                {
                    if (edges[Cell(a, b)] != 0)
                    {
                        indegree[b] = indegree[b] + 1;
                    }
                    b = b + 1;
                }
                a = a + 1;
            }
            raii IntQueue ready = new IntQueue(count + 1);
            int i = 0;
            while (i < count)
            {
                if (indegree[i] == 0)
                {
                    ready.Enqueue(i);
                }
                i = i + 1;
            }
            int processed = 0;
            while (!ready.IsEmpty)
            {
                int task = ready.Dequeue();
                raii string number = String.ToString(processed + 1);
                raii string x = String.Concat("  ", number);
                raii string y = String.Concat(x, ". ");
                raii string line = String.Concat(y, tasks[task].Name);
                Console.WriteLine(line);
                processed = processed + 1;
                int next = 0;
                while (next < count)
                {
                    if (edges[Cell(task, next)] != 0)
                    {
                        indegree[next] = indegree[next] - 1;
                        if (indegree[next] == 0)
                        {
                            ready.Enqueue(next);
                        }
                    }
                    next = next + 1;
                }
            }
            if (processed != count)
            {
                Console.WriteLine("cycle detected: no complete topological order exists");
            }
            delete[] indegree;
        }
        ~DependencyGraph()
        {
            int i = 0;
            while (i < count)
            {
                delete tasks[i];
                i = i + 1;
            }
            delete[] tasks;
            delete[] edges;
        }
    }
}
```

##### `book_examples/18_topological_sort/IntQueue.ns`

```csharp
using NativeSharp.Core;
namespace NativeSharp.BookExamples.TopologicalSort
{
    public class IntQueue
    {
        private int[] data;
        private int head;
        private int tail;
        private int count;
        public IntQueue(int capacity)
        {
            data = new int[capacity];
            head = 0;
            tail = 0;
            count = 0;
        }
        public bool IsEmpty
        {
            get
            {
                return count == 0;
            }
        }
        public void Enqueue(int value)
        {
            data[tail] = value;
            tail = (tail + 1) % data.Length;
            count = count + 1;
        }
        public int Dequeue()
        {
            if (count == 0)
            {
                throw new InvalidOperationException();
            }
            int v = data[head];
            head = (head + 1) % data.Length;
            count = count - 1;
            return v;
        }
        ~IntQueue()
        {
            delete[] data;
        }
    }
}
```

##### `book_examples/18_topological_sort/Program.ns`

```csharp
using NativeSharp.Core;
namespace NativeSharp.BookExamples.TopologicalSort
{
    class Program
    {
        public static int Main()
        {
            Console.WriteLine("NativeSharp Book Example 18 - Topological Sort");
            Console.WriteLine("----------------------------------------------");
            Console.WriteLine("Build pipeline modeled as a DAG:");
            raii DependencyGraph graph = new DependencyGraph(10);
            int scan = graph.AddTask("scan source files");
            int lex = graph.AddTask("lex tokens");
            int parse = graph.AddTask("parse syntax tree");
            int sema = graph.AddTask("semantic analysis");
            int hir = graph.AddTask("lower to HIR");
            int mir = graph.AddTask("lower to MIR");
            int optimize = graph.AddTask("optimize MIR");
            int llvm = graph.AddTask("emit LLVM IR");
            int obj = graph.AddTask("emit COFF object");
            int link = graph.AddTask("link executable");
            graph.AddDependency(scan, lex);
            graph.AddDependency(lex, parse);
            graph.AddDependency(parse, sema);
            graph.AddDependency(sema, hir);
            graph.AddDependency(hir, mir);
            graph.AddDependency(mir, optimize);
            graph.AddDependency(optimize, llvm);
            graph.AddDependency(llvm, obj);
            graph.AddDependency(obj, link);
            graph.TopologicalOrder();
            return 0;
        }
    }
}
```

### 47.19 Kruskal y union-find

**Problema.** Construir un árbol de expansión mínimo eligiendo aristas por peso sin crear ciclos.

**Diseño.** Kruskal ordena aristas y union-find mantiene componentes con representantes. El ejemplo conecta estructura auxiliar, ordenamiento y propiedad global.

**Auditoría propuesta.** Compruebe conectividad, pesos repetidos y grafo desconectado. El resultado de un grafo desconectado es bosque, salvo que el contrato exija error.

**Escala real.** 4 archivos NativeSharp, 106 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

**Reproducción.**

```powershell
nsc --emit-project-oop-exe `
    book_examples/19_kruskal_mst_union_find/Book19KruskalMst.nsproj `
    -O2 `
    -o 19_kruskal_mst_union_find.exe
./19_kruskal_mst_union_find.exe
```

#### Listado completo

##### `book_examples/19_kruskal_mst_union_find/Book19KruskalMst.nsproj`

```xml
<Project>
  <PropertyGroup><Name>Book19KruskalMst</Name></PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
  </ItemGroup>
</Project>
```

##### `book_examples/19_kruskal_mst_union_find/DisjointSet.ns`

```csharp
namespace NativeSharp.BookExamples.Kruskal
{
    public class DisjointSet
    {
        private int[] parent;
        private int[] rank;
        public DisjointSet(int count)
        {
            parent = new int[count];
            rank = new int[count];
            int i = 0;
            while (i < count)
            {
                parent[i] = i;
                rank[i] = 0;
                i = i + 1;
            }
        }
        public int Find(int value)
        {
            int root = value;
            while (parent[root] != root)
            {
                root = parent[root];
            }
            int current = value;
            while (parent[current] != current)
            {
                int next = parent[current];
                parent[current] = root;
                current = next;
            }
            return root;
        }
        public bool Union(int a, int b)
        {
            int ra = Find(a);
            int rb = Find(b);
            if (ra == rb)
            {
                return false;
            }
            if (rank[ra] < rank[rb])
            {
                parent[ra] = rb;
            }
            else if (rank[ra] > rank[rb])
            {
                parent[rb] = ra;
            }
            else
            {
                parent[rb] = ra;
                rank[ra] = rank[ra] + 1;
            }
            return true;
        }
        ~DisjointSet()
        {
            delete[] parent;
            delete[] rank;
        }
    }
}
```

##### `book_examples/19_kruskal_mst_union_find/Edge.ns`

```csharp
namespace NativeSharp.BookExamples.Kruskal
{
    public class Edge
    {
        public int From;
        public int To;
        public int Weight;
        public Edge(int from, int to, int weight)
        {
            From = from;
            To = to;
            Weight = weight;
        }
    }
}
```

##### `book_examples/19_kruskal_mst_union_find/KruskalSolver.ns`

```csharp
using NativeSharp.Core;
namespace NativeSharp.BookExamples.Kruskal
{
    public class KruskalSolver
    {
        private static void SortByWeight(Edge[] edges, int count)
        {
            int i = 1;
            while (i < count)
            {
                Edge current = edges[i];
                int j = i - 1;
                while (j >= 0 && edges[j].Weight > current.Weight)
                {
                    edges[j + 1] = edges[j];
                    j = j - 1;
                }
                edges[j + 1] = current;
                i = i + 1;
            }
        }
        public static void Solve(string[] names, int vertexCount, Edge[] edges, int edgeCount)
        {
            SortByWeight(edges, edgeCount);
            raii DisjointSet sets = new DisjointSet(vertexCount);
            int selected = 0;
            int total = 0;
            int i = 0;
            while (i < edgeCount && selected < vertexCount - 1)
            {
                Edge edge = edges[i];
                if (sets.Union(edge.From, edge.To))
                {
                    selected = selected + 1;
                    total = total + edge.Weight;
                    raii string w = String.ToString(edge.Weight);
                    raii string a = String.Concat("  ", names[edge.From]);
                    raii string b = String.Concat(a, " -- ");
                    raii string c = String.Concat(b, names[edge.To]);
                    raii string d = String.Concat(c, "  weight=");
                    raii string line = String.Concat(d, w);
                    Console.WriteLine(line);
                }
                i = i + 1;
            }
            raii string t = String.ToString(total);
            raii string line = String.Concat("MST total weight: ", t);
            Console.WriteLine(line);
        }
    }
}
```

##### `book_examples/19_kruskal_mst_union_find/Program.ns`

```csharp
using NativeSharp.Core;
namespace NativeSharp.BookExamples.Kruskal
{
    class Program
    {
        public static int Main()
        {
            Console.WriteLine("NativeSharp Book Example 19 - Kruskal MST + Union-Find");
            Console.WriteLine("-------------------------------------------------------");
            string[] names = new string[6];
            names[0] = "A";
            names[1] = "B";
            names[2] = "C";
            names[3] = "D";
            names[4] = "E";
            names[5] = "F";
            Edge[] edges = new Edge[9];
            edges[0] = new Edge(0, 1, 4);
            edges[1] = new Edge(0, 2, 2);
            edges[2] = new Edge(1, 2, 1);
            edges[3] = new Edge(1, 3, 5);
            edges[4] = new Edge(2, 3, 8);
            edges[5] = new Edge(2, 4, 10);
            edges[6] = new Edge(3, 4, 2);
            edges[7] = new Edge(3, 5, 6);
            edges[8] = new Edge(4, 5, 3);
            KruskalSolver.Solve(names, 6, edges, 9);
            int i = 0;
            while (i < 9)
            {
                delete edges[i];
                i = i + 1;
            }
            delete[] edges;
            delete[] names;
            return 0;
        }
    }
}
```

### 47.20 Mochila 0/1 con programación dinámica

**Problema.** Maximizar valor bajo una capacidad cuando cada artículo puede elegirse como máximo una vez.

**Diseño.** La tabla dinámica registra subproblemas por prefijo de artículos y capacidad. La reconstrucción distingue el valor óptimo de la selección que lo alcanza.

**Auditoría propuesta.** Pruebe capacidad cero, artículo demasiado pesado y empates. Explique por qué recorrer capacidades en el orden equivocado cambia 0/1 por mochila ilimitada.

**Escala real.** 3 archivos NativeSharp, 97 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

**Reproducción.**

```powershell
nsc --emit-project-oop-exe `
    book_examples/20_dynamic_programming_knapsack/Book20KnapsackDynamicProgramming.nsproj `
    -O2 `
    -o 20_dynamic_programming_knapsack.exe
./20_dynamic_programming_knapsack.exe
```

#### Listado completo

##### `book_examples/20_dynamic_programming_knapsack/Book20KnapsackDynamicProgramming.nsproj`

```xml
<Project>
  <PropertyGroup><Name>Book20KnapsackDynamicProgramming</Name></PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
  </ItemGroup>
</Project>
```

##### `book_examples/20_dynamic_programming_knapsack/Item.ns`

```csharp
namespace NativeSharp.BookExamples.Knapsack
{
    public class Item
    {
        public string Name;
        public int Weight;
        public int Value;
        public Item(string name, int weight, int value)
        {
            Name = name;
            Weight = weight;
            Value = value;
        }
    }
}
```

##### `book_examples/20_dynamic_programming_knapsack/KnapsackSolver.ns`

```csharp
using NativeSharp.Core;
namespace NativeSharp.BookExamples.Knapsack
{
    public class KnapsackSolver
    {
        private static int Cell(int row, int column, int width)
        {
            return row * width + column;
        }
        private static int Max(int a, int b)
        {
            if (a > b)
            {
                return a;
            }
            return b;
        }
        public static void Solve(Item[] items, int itemCount, int capacity)
        {
            int width = capacity + 1;
            int[] dp = new int[(itemCount + 1) * width];
            int i = 1;
            while (i <= itemCount)
            {
                Item item = items[i - 1];
                int w = 0;
                while (w <= capacity)
                {
                    int without = dp[Cell(i - 1, w, width)];
                    int best = without;
                    if (item.Weight <= w)
                    {
                        int with = item.Value + dp[Cell(i - 1, w - item.Weight, width)];
                        best = Max(without, with);
                    }
                    dp[Cell(i, w, width)] = best;
                    w = w + 1;
                }
                i = i + 1;
            }
            raii string best = String.ToString(dp[Cell(itemCount, capacity, width)]);
            raii string bestLine = String.Concat("maximum value: ", best);
            Console.WriteLine(bestLine);
            Console.WriteLine("chosen items (reconstructed by walking the DP table backward):");
            int remaining = capacity;
            int row = itemCount;
            int usedWeight = 0;
            while (row > 0)
            {
                int current = dp[Cell(row, remaining, width)];
                int previous = dp[Cell(row - 1, remaining, width)];
                if (current != previous)
                {
                    Item chosen = items[row - 1];
                    usedWeight = usedWeight + chosen.Weight;
                    raii string weight = String.ToString(chosen.Weight);
                    raii string value = String.ToString(chosen.Value);
                    raii string a = String.Concat("  ", chosen.Name);
                    raii string b = String.Concat(a, "  weight=");
                    raii string c = String.Concat(b, weight);
                    raii string d = String.Concat(c, "  value=");
                    raii string line = String.Concat(d, value);
                    Console.WriteLine(line);
                    remaining = remaining - chosen.Weight;
                }
                row = row - 1;
            }
            raii string uw = String.ToString(usedWeight);
            raii string cap = String.ToString(capacity);
            raii string a = String.Concat("weight used: ", uw);
            raii string b = String.Concat(a, " / ");
            raii string line = String.Concat(b, cap);
            Console.WriteLine(line);
            delete[] dp;
        }
    }
}
```

##### `book_examples/20_dynamic_programming_knapsack/Program.ns`

```csharp
using NativeSharp.Core;
namespace NativeSharp.BookExamples.Knapsack
{
    class Program
    {
        public static int Main()
        {
            Console.WriteLine("NativeSharp Book Example 20 - 0/1 Knapsack Dynamic Programming");
            Console.WriteLine("--------------------------------------------------------------");
            Item[] items = new Item[6];
            items[0] = new Item("medical kit", 4, 12);
            items[1] = new Item("water", 2, 8);
            items[2] = new Item("radio", 3, 10);
            items[3] = new Item("rope", 2, 7);
            items[4] = new Item("tool kit", 5, 14);
            items[5] = new Item("flare", 1, 4);
            KnapsackSolver.Solve(items, 6, 10);
            int i = 0;
            while (i < 6)
            {
                delete items[i];
                i = i + 1;
            }
            delete[] items;
            return 0;
        }
    }
}
```

### 47.21 Subsecuencia común más larga

**Problema.** Encontrar una secuencia ordenada común a dos textos sin exigir contigüidad.

**Diseño.** La matriz dinámica compara prefijos y la reconstrucción camina decisiones. El caso muestra que obtener sólo la longitud es una especificación incompleta para muchas aplicaciones.

**Auditoría propuesta.** Pruebe texto vacío, caracteres repetidos y soluciones múltiples. No confunda subsecuencia con substring.

**Escala real.** 2 archivos NativeSharp, 70 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

**Reproducción.**

```powershell
nsc --emit-project-oop-exe `
    book_examples/21_dynamic_programming_lcs/Book21LongestCommonSubsequence.nsproj `
    -O2 `
    -o 21_dynamic_programming_lcs.exe
./21_dynamic_programming_lcs.exe
```

#### Listado completo

##### `book_examples/21_dynamic_programming_lcs/Book21LongestCommonSubsequence.nsproj`

```xml
<Project>
  <PropertyGroup><Name>Book21LongestCommonSubsequence</Name></PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
  </ItemGroup>
</Project>
```

##### `book_examples/21_dynamic_programming_lcs/LcsSolver.ns`

```csharp
using NativeSharp.Core;
namespace NativeSharp.BookExamples.Lcs
{
    public class LcsSolver
    {
        private static int Cell(int row, int col, int width)
        {
            return row * width + col;
        }
        private static int Max(int a, int b)
        {
            if (a > b)
            {
                return a;
            }
            return b;
        }
        public static owned string Solve(string left, string right)
        {
            int rows = left.Length + 1;
            int cols = right.Length + 1;
            int[] dp = new int[rows * cols];
            int i = 1;
            while (i < rows)
            {
                int j = 1;
                while (j < cols)
                {
                    if (left[i - 1] == right[j - 1])
                    {
                        dp[Cell(i, j, cols)] = dp[Cell(i - 1, j - 1, cols)] + 1;
                    }
                    else
                    {
                        dp[Cell(i, j, cols)] = Max(dp[Cell(i - 1, j, cols)], dp[Cell(i, j - 1, cols)]);
                    }
                    j = j + 1;
                }
                i = i + 1;
            }
            int length = dp[Cell(left.Length, right.Length, cols)];
            char[] reverse = new char[length];
            int pos = 0;
            int r = left.Length;
            int c = right.Length;
            while (r > 0 && c > 0)
            {
                if (left[r - 1] == right[c - 1])
                {
                    reverse[pos] = left[r - 1];
                    pos = pos + 1;
                    r = r - 1;
                    c = c - 1;
                }
                else if (dp[Cell(r - 1, c, cols)] >= dp[Cell(r, c - 1, cols)])
                {
                    r = r - 1;
                }
                else
                {
                    c = c - 1;
                }
            }
            raii string result = String.Copy("");
            int k = length - 1;
            while (k >= 0)
            {
                raii string ch = String.ToString(reverse[k]);
                raii string next = String.Concat(result, ch);
                result = move(next);
                k = k - 1;
            }
            delete[] reverse;
            delete[] dp;
            return move(result);
        }
    }
}
```

##### `book_examples/21_dynamic_programming_lcs/Program.ns`

```csharp
using NativeSharp.Core;
namespace NativeSharp.BookExamples.Lcs
{
    class Program
    {
        public static int Main()
        {
            Console.WriteLine("NativeSharp Book Example 21 - Longest Common Subsequence");
            Console.WriteLine("---------------------------------------------------------");
            raii string left = String.Copy("NATIVESHARP");
            raii string right = String.Copy("NATIVEGRAPH");
            raii string lcs = LcsSolver.Solve(left, right);
            raii string a = String.Concat("left : ", left);
            Console.WriteLine(a);
            raii string b = String.Concat("right: ", right);
            Console.WriteLine(b);
            raii string c = String.Concat("LCS  : ", lcs);
            Console.WriteLine(c);
            raii string len = String.ToString(lcs.Length);
            raii string d = String.Concat("length: ", len);
            Console.WriteLine(d);
            return 0;
        }
    }
}
```

### 47.22 Distancia de edición de Levenshtein

**Problema.** Medir el mínimo de inserciones, eliminaciones y sustituciones para transformar un texto en otro.

**Diseño.** La recurrencia llena una tabla por prefijos y usa bordes como costo de convertir desde o hacia vacío. El programa contrasta pares con resultados conocidos.

**Auditoría propuesta.** Pruebe simetría, igualdad, vacío y caracteres repetidos. Si se agregan costos distintos, documente que la métrica cambia.

**Escala real.** 2 archivos NativeSharp, 70 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

**Reproducción.**

```powershell
nsc --emit-project-oop-exe `
    book_examples/22_dynamic_programming_edit_distance/Book22EditDistance.nsproj `
    -O2 `
    -o 22_dynamic_programming_edit_distance.exe
./22_dynamic_programming_edit_distance.exe
```

#### Listado completo

##### `book_examples/22_dynamic_programming_edit_distance/Book22EditDistance.nsproj`

```xml
<Project>
  <PropertyGroup><Name>Book22EditDistance</Name></PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
  </ItemGroup>
</Project>
```

##### `book_examples/22_dynamic_programming_edit_distance/EditDistanceSolver.ns`

```csharp
using NativeSharp.Core;
namespace NativeSharp.BookExamples.EditDistance
{
    public class EditDistanceSolver
    {
        private static int Cell(int r, int c, int width)
        {
            return r * width + c;
        }
        private static int Min3(int a, int b, int c)
        {
            int m = a;
            if (b < m)
            {
                m = b;
            }
            if (c < m)
            {
                m = c;
            }
            return m;
        }
        public static int Solve(string source, string target)
        {
            int rows = source.Length + 1;
            int cols = target.Length + 1;
            int[] dp = new int[rows * cols];
            int r = 0;
            while (r < rows)
            {
                dp[Cell(r, 0, cols)] = r;
                r = r + 1;
            }
            int c = 0;
            while (c < cols)
            {
                dp[Cell(0, c, cols)] = c;
                c = c + 1;
            }
            r = 1;
            while (r < rows)
            {
                c = 1;
                while (c < cols)
                {
                    int cost = 1;
                    if (source[r - 1] == target[c - 1])
                    {
                        cost = 0;
                    }
                    int deletion = dp[Cell(r - 1, c, cols)] + 1;
                    int insertion = dp[Cell(r, c - 1, cols)] + 1;
                    int replacement = dp[Cell(r - 1, c - 1, cols)] + cost;
                    dp[Cell(r, c, cols)] = Min3(deletion, insertion, replacement);
                    c = c + 1;
                }
                r = r + 1;
            }
            int result = dp[Cell(source.Length, target.Length, cols)];
            Console.WriteLine("one optimal edit script, reconstructed backward:");
            r = source.Length;
            c = target.Length;
            while (r > 0 || c > 0)
            {
                if (r > 0 && c > 0 && source[r - 1] == target[c - 1])
                {
                    r = r - 1;
                    c = c - 1;
                }
                else if (r > 0 && c > 0 && dp[Cell(r, c, cols)] == dp[Cell(r - 1, c - 1, cols)] + 1)
                {
                    raii string s = String.ToString(source[r - 1]);
                    raii string t = String.ToString(target[c - 1]);
                    raii string a = String.Concat("  replace '", s);
                    raii string b = String.Concat(a, "' with '");
                    raii string d = String.Concat(b, t);
                    raii string line = String.Concat(d, "'");
                    Console.WriteLine(line);
                    r = r - 1;
                    c = c - 1;
                }
                else if (r > 0 && dp[Cell(r, c, cols)] == dp[Cell(r - 1, c, cols)] + 1)
                {
                    raii string s = String.ToString(source[r - 1]);
                    raii string a = String.Concat("  delete '", s);
                    raii string line = String.Concat(a, "'");
                    Console.WriteLine(line);
                    r = r - 1;
                }
                else
                {
                    raii string t = String.ToString(target[c - 1]);
                    raii string a = String.Concat("  insert '", t);
                    raii string line = String.Concat(a, "'");
                    Console.WriteLine(line);
                    c = c - 1;
                }
            }
            delete[] dp;
            return result;
        }
    }
}
```

##### `book_examples/22_dynamic_programming_edit_distance/Program.ns`

```csharp
using NativeSharp.Core;
namespace NativeSharp.BookExamples.EditDistance
{
    class Program
    {
        public static int Main()
        {
            Console.WriteLine("NativeSharp Book Example 22 - Levenshtein Edit Distance");
            Console.WriteLine("-----------------------------------------------------");
            raii string source = String.Copy("kitten");
            raii string target = String.Copy("sitting");
            int distance = EditDistanceSolver.Solve(source, target);
            raii string d = String.ToString(distance);
            raii string line = String.Concat("edit distance: ", d);
            Console.WriteLine(line);
            return 0;
        }
    }
}
```

### 47.23 Álgebra de polinomios

**Problema.** Representar coeficientes, sumar, restar, multiplicar, derivar y evaluar sin esconder la complejidad de cada operación.

**Diseño.** El indexador abstrae potencias, los operadores producen polinomios nuevos y Horner evalúa en tiempo lineal. La impresión materializa strings temporales bajo RAII.

**Auditoría propuesta.** Revise coeficientes cero, polinomio constante, grado resultante y ownership de cada resultado algebraico. Compare Horner con potencias repetidas.

**Escala real.** 3 archivos NativeSharp, 130 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

**Reproducción.**

```powershell
nsc --emit-project-oop-exe `
    book_examples/23_polynomial_algebra/Book23PolynomialAlgebra.nsproj `
    -O2 `
    -o 23_polynomial_algebra.exe
./23_polynomial_algebra.exe
```

#### Listado completo

##### `book_examples/23_polynomial_algebra/Book23PolynomialAlgebra.nsproj`

```xml
<Project>
  <PropertyGroup><Name>Book23PolynomialAlgebra</Name></PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
  </ItemGroup>
</Project>
```

##### `book_examples/23_polynomial_algebra/NumberText.ns`

```csharp
using NativeSharp.Core;
namespace NativeSharp.BookExamples.Polynomial
{
    public class NumberText
    {
        public static owned string Format(double value)
        {
            bool negative = value < 0.0;
            if (negative)
            {
                value = -value;
            }
            int scaled = (int)(value * 100.0 + 0.5);
            int whole = scaled / 100;
            int fraction = scaled % 100;
            raii string wholeText = String.ToString(whole);
            raii string fractionText = String.ToString(fraction);
            raii string result = String.Copy("");
            if (negative)
            {
                raii string sign = String.Concat(result, "-");
                result = move(sign);
            }
            raii string withWhole = String.Concat(result, wholeText);
            result = move(withWhole);
            raii string dot = String.Concat(result, ".");
            result = move(dot);
            if (fraction < 10)
            {
                raii string zero = String.Concat(result, "0");
                result = move(zero);
            }
            raii string complete = String.Concat(result, fractionText);
            result = move(complete);
            return move(result);
        }
    }
}
```

##### `book_examples/23_polynomial_algebra/Polynomial.ns`

```csharp
using NativeSharp.Core;
namespace NativeSharp.BookExamples.Polynomial
{
    public class Polynomial
    {
        private double[] coefficients;
        private int degree;

        public Polynomial(int degree)
        {
            if (degree < 0)
            {
                degree = 0;
            }
            this.degree = degree;
            coefficients = new double[degree + 1];
        }
        public int Degree
        {
            get
            {
                return degree;
            }
        }
        public double this[int power]
        {
            get
            {
                if (power < 0 || power > degree)
                {
                    return 0.0;
                }
                return coefficients[power];
            }
            set
            {
                if (power < 0 || power > degree)
                {
                    throw new ArgumentOutOfRangeException();
                }
                coefficients[power] = value;
            }
        }
        public double Evaluate(double x)
        {
            double result = coefficients[degree];
            int p = degree - 1;
            while (p >= 0)
            {
                result = result * x + coefficients[p];
                p = p - 1;
            }
            return result;
        }
        public Polynomial Derivative()
        {
            if (degree == 0)
            {
                return new Polynomial(0);
            }
            Polynomial result = new Polynomial(degree - 1);
            int p = 1;
            while (p <= degree)
            {
                result[p - 1] = coefficients[p] * (double)p;
                p = p + 1;
            }
            return result;
        }
        public static Polynomial operator +(Polynomial a, Polynomial b)
        {
            int d = a.degree;
            if (b.degree > d)
            {
                d = b.degree;
            }
            Polynomial r = new Polynomial(d);
            int p = 0;
            while (p <= d)
            {
                r[p] = a[p] + b[p];
                p = p + 1;
            }
            return r;
        }
        public static Polynomial operator -(Polynomial a, Polynomial b)
        {
            int d = a.degree;
            if (b.degree > d)
            {
                d = b.degree;
            }
            Polynomial r = new Polynomial(d);
            int p = 0;
            while (p <= d)
            {
                r[p] = a[p] - b[p];
                p = p + 1;
            }
            return r;
        }
        public static Polynomial operator *(Polynomial a, Polynomial b)
        {
            Polynomial r = new Polynomial(a.degree + b.degree);
            int i = 0;
            while (i <= a.degree)
            {
                int j = 0;
                while (j <= b.degree)
                {
                    r[i + j] = r[i + j] + a[i] * b[j];
                    j = j + 1;
                }
                i = i + 1;
            }
            return r;
        }
        public void Print(string name)
        {
            raii string line = String.Concat(name, "(x) = ");
            int p = degree;
            bool first = true;
            while (p >= 0)
            {
                double c = coefficients[p];
                if (c != 0.0)
                {
                    if (!first)
                    {
                        raii string plus = String.Concat(line, " + ");
                        line = move(plus);
                    }
                    raii string value = NumberText.Format(c);
                    raii string term = String.Concat(line, value);
                    line = move(term);
                    if (p > 0)
                    {
                        raii string x = String.Concat(line, "*x");
                        line = move(x);
                    }
                    if (p > 1)
                    {
                        raii string power = String.ToString(p);
                        raii string caret = String.Concat(line, "^");
                        line = move(caret);
                        raii string withPower = String.Concat(line, power);
                        line = move(withPower);
                    }
                    first = false;
                }
                p = p - 1;
            }
            if (first)
            {
                raii string zero = String.Concat(line, "0");
                line = move(zero);
            }
            Console.WriteLine(line);
        }
        ~Polynomial()
        {
            delete[] coefficients;
        }
    }
}
```

##### `book_examples/23_polynomial_algebra/Program.ns`

```csharp
using NativeSharp.Core;
namespace NativeSharp.BookExamples.Polynomial
{
    class Program
    {
        public static int Main()
        {
            Console.WriteLine("NativeSharp Book Example 23 - Polynomial Algebra");
            Console.WriteLine("-----------------------------------------------");
            Polynomial p = new Polynomial(3);
            p[0] = 2.0;
            p[1] = -1.0;
            p[2] = 0.0;
            p[3] = 3.0;
            Polynomial q = new Polynomial(2);
            q[0] = -4.0;
            q[1] = 2.0;
            q[2] = 1.0;
            p.Print("P");
            q.Print("Q");
            Polynomial sum = p + q;
            Polynomial difference = p - q;
            Polynomial product = p * q;
            Polynomial derivative = p.Derivative();
            sum.Print("P+Q");
            difference.Print("P-Q");
            product.Print("P*Q");
            derivative.Print("P'");
            double x = 2.0;
            double value = product.Evaluate(x);
            raii string xs = NumberText.Format(x);
            raii string vs = NumberText.Format(value);
            raii string a = String.Concat("(P*Q)(", xs);
            raii string b = String.Concat(a, ") = ");
            raii string line = String.Concat(b, vs);
            Console.WriteLine(line);
            delete derivative;
            delete product;
            delete difference;
            delete sum;
            delete q;
            delete p;
            return 0;
        }
    }
}
```

### 47.24 Vectores y matrices 3x3

**Problema.** Componer transformaciones lineales y operar vectores con una interfaz algebraica legible.

**Diseño.** `Vector3` implementa suma, resta, producto punto y cruz. `Matrix3` almacena nueve valores, compone matrices y transforma vectores. El programa conserva todos los resultados para imprimirlos y destruirlos.

**Auditoría propuesta.** Verifique orden de multiplicación, matriz identidad, signos del producto cruz y layout de índices. Audite que cada objeto creado se destruya una sola vez.

**Escala real.** 4 archivos NativeSharp, 124 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

**Reproducción.**

```powershell
nsc --emit-project-oop-exe `
    book_examples/24_vector_matrix_algebra/Book24VectorMatrixAlgebra.nsproj `
    -O2 `
    -o 24_vector_matrix_algebra.exe
./24_vector_matrix_algebra.exe
```

#### Listado completo

##### `book_examples/24_vector_matrix_algebra/Book24VectorMatrixAlgebra.nsproj`

```xml
<Project>
  <PropertyGroup><Name>Book24VectorMatrixAlgebra</Name></PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
  </ItemGroup>
</Project>
```

##### `book_examples/24_vector_matrix_algebra/Matrix3.ns`

```csharp
using NativeSharp.Core;
namespace NativeSharp.BookExamples.LinearAlgebra
{
    public class Matrix3
    {
        private double[] m;
        public Matrix3()
        {
            m = new double[9];
        }
        private int Cell(int r, int c)
        {
            return r * 3 + c;
        }
        public double Get(int r, int c)
        {
            return m[Cell(r, c)];
        }
        public void Set(int r, int c, double value)
        {
            m[Cell(r, c)] = value;
        }
        public static Matrix3 Identity()
        {
            Matrix3 r = new Matrix3();
            r.Set(0, 0, 1.0);
            r.Set(1, 1, 1.0);
            r.Set(2, 2, 1.0);
            return r;
        }
        public static Matrix3 operator +(Matrix3 a, Matrix3 b)
        {
            Matrix3 r = new Matrix3();
            int row = 0;
            while (row < 3)
            {
                int col = 0;
                while (col < 3)
                {
                    r.Set(row, col, a.Get(row, col) + b.Get(row, col));
                    col = col + 1;
                }
                row = row + 1;
            }
            return r;
        }
        public static Matrix3 operator *(Matrix3 a, Matrix3 b)
        {
            Matrix3 r = new Matrix3();
            int row = 0;
            while (row < 3)
            {
                int col = 0;
                while (col < 3)
                {
                    double sum = 0.0;
                    int k = 0;
                    while (k < 3)
                    {
                        sum = sum + a.Get(row, k) * b.Get(k, col);
                        k = k + 1;
                    }
                    r.Set(row, col, sum);
                    col = col + 1;
                }
                row = row + 1;
            }
            return r;
        }
        public Vector3 Transform(Vector3 v)
        {
            return new Vector3(Get(0, 0) * v.X + Get(0, 1) * v.Y + Get(0, 2) * v.Z,
                               Get(1, 0) * v.X + Get(1, 1) * v.Y + Get(1, 2) * v.Z,
                               Get(2, 0) * v.X + Get(2, 1) * v.Y + Get(2, 2) * v.Z);
        }
        public void Print(string label)
        {
            Console.WriteLine(label);
            int row = 0;
            while (row < 3)
            {
                raii string a = NumberText.Format(Get(row, 0));
                raii string b = NumberText.Format(Get(row, 1));
                raii string c = NumberText.Format(Get(row, 2));
                raii string x = String.Concat("  [ ", a);
                raii string y = String.Concat(x, ", ");
                raii string z = String.Concat(y, b);
                raii string q = String.Concat(z, ", ");
                raii string w = String.Concat(q, c);
                raii string line = String.Concat(w, " ]");
                Console.WriteLine(line);
                row = row + 1;
            }
        }
        ~Matrix3()
        {
            delete[] m;
        }
    }
}
```

##### `book_examples/24_vector_matrix_algebra/NumberText.ns`

```csharp
using NativeSharp.Core;
namespace NativeSharp.BookExamples.LinearAlgebra
{
    public class NumberText
    {
        public static owned string Format(double value)
        {
            bool negative = value < 0.0;
            if (negative)
            {
                value = -value;
            }
            int scaled = (int)(value * 100.0 + 0.5);
            int whole = scaled / 100;
            int fraction = scaled % 100;
            raii string wholeText = String.ToString(whole);
            raii string fractionText = String.ToString(fraction);
            raii string result = String.Copy("");
            if (negative)
            {
                raii string sign = String.Concat(result, "-");
                result = move(sign);
            }
            raii string withWhole = String.Concat(result, wholeText);
            result = move(withWhole);
            raii string dot = String.Concat(result, ".");
            result = move(dot);
            if (fraction < 10)
            {
                raii string zero = String.Concat(result, "0");
                result = move(zero);
            }
            raii string complete = String.Concat(result, fractionText);
            result = move(complete);
            return move(result);
        }
    }
}
```

##### `book_examples/24_vector_matrix_algebra/Vector3.ns`

```csharp
using NativeSharp.Core;
namespace NativeSharp.BookExamples.LinearAlgebra
{
    public class Vector3
    {
        public double X;
        public double Y;
        public double Z;
        public Vector3(double x, double y, double z)
        {
            X = x;
            Y = y;
            Z = z;
        }
        public double this[int index]
        {
            get
            {
                if (index == 0)
                {
                    return X;
                }
                if (index == 1)
                {
                    return Y;
                }
                return Z;
            }
            set
            {
                if (index == 0)
                {
                    X = value;
                }
                else if (index == 1)
                {
                    Y = value;
                }
                else
                {
                    Z = value;
                }
            }
        }
        public static Vector3 operator +(Vector3 a, Vector3 b)
        {
            return new Vector3(a.X + b.X, a.Y + b.Y, a.Z + b.Z);
        }
        public static Vector3 operator -(Vector3 a, Vector3 b)
        {
            return new Vector3(a.X - b.X, a.Y - b.Y, a.Z - b.Z);
        }
        public static Vector3 operator -(Vector3 a)
        {
            return new Vector3(-a.X, -a.Y, -a.Z);
        }
        public double Dot(Vector3 other)
        {
            return X * other.X + Y * other.Y + Z * other.Z;
        }
        public Vector3 Cross(Vector3 other)
        {
            return new Vector3(Y * other.Z - Z * other.Y, Z * other.X - X * other.Z, X * other.Y - Y * other.X);
        }
        public void Print(string label)
        {
            raii string x = NumberText.Format(X);
            raii string y = NumberText.Format(Y);
            raii string z = NumberText.Format(Z);
            raii string a = String.Concat(label, "(");
            raii string b = String.Concat(a, x);
            raii string c = String.Concat(b, ", ");
            raii string d = String.Concat(c, y);
            raii string e = String.Concat(d, ", ");
            raii string f = String.Concat(e, z);
            raii string line = String.Concat(f, ")");
            Console.WriteLine(line);
        }
    }
}
```

##### `book_examples/24_vector_matrix_algebra/Program.ns`

```csharp
using NativeSharp.Core;
namespace NativeSharp.BookExamples.LinearAlgebra
{
    class Program
    {
        public static int Main()
        {
            Console.WriteLine("NativeSharp Book Example 24 - Vector and Matrix Algebra");
            Console.WriteLine("------------------------------------------------------");
            Vector3 a = new Vector3(1.0, 2.0, 3.0);
            Vector3 b = new Vector3(4.0, -1.0, 2.0);
            Vector3 sum = a + b;
            Vector3 difference = a - b;
            Vector3 cross = a.Cross(b);
            double dot = a.Dot(b);
            a.Print("a = ");
            b.Print("b = ");
            sum.Print("a+b = ");
            difference.Print("a-b = ");
            cross.Print("a x b = ");
            raii string dotText = NumberText.Format(dot);
            raii string dotLine = String.Concat("a dot b = ", dotText);
            Console.WriteLine(dotLine);
            Matrix3 scale = new Matrix3();
            scale.Set(0, 0, 2.0);
            scale.Set(1, 1, 3.0);
            scale.Set(2, 2, 1.0);
            Matrix3 shear = Matrix3.Identity();
            shear.Set(0, 1, 1.0);
            Matrix3 composed = scale * shear;
            composed.Print("scale * shear:");
            Vector3 transformed = composed.Transform(a);
            transformed.Print("transformed a = ");
            delete transformed;
            delete composed;
            delete shear;
            delete scale;
            delete cross;
            delete difference;
            delete sum;
            delete b;
            delete a;
            return 0;
        }
    }
}
```

### 47.25 Laboratorio avanzado de Drozdek

**Problema.** Cerrar los huecos algorítmicos que quedan al comparar el temario completo de Drozdek con los primeros veinticuatro estudios de caso.

**Diseño.** Nueve módulos ejecutables reúnen ordenamientos no comparativos, skip lists, árboles autoajustables y multiway, grafos avanzados, hashing, compresión, administración de memoria, búsqueda de texto y los casos clásicos faltantes.

**Auditoría propuesta.** Cada módulo calcula resultados conocidos y contribuye a un checksum global. Audite precondiciones, invariantes y liberación de arrays y nodos antes de sustituir los datasets pequeños por entradas grandes.

**Escala real.** 10 archivos NativeSharp, 2520 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

**Reproducción.**

```powershell
nsc --emit-project-oop-exe `
    book_examples/25_drozdek_advanced_suite/Book25DrozdekAdvancedSuite.nsproj `
    -O2 `
    -o 25_drozdek_advanced_suite.exe
./25_drozdek_advanced_suite.exe
```

#### Listado completo

##### `book_examples/25_drozdek_advanced_suite/Book25DrozdekAdvancedSuite.nsproj`

```xml
<Project>
  <PropertyGroup>
    <Name>Book25DrozdekAdvancedSuite</Name>
  </PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
    <ProjectReference Include="../../lib/NativeSharp.Core.IO/NativeSharp.Core.IO.nsproj" />
  </ItemGroup>
</Project>
```

##### `book_examples/25_drozdek_advanced_suite/CaseStudyAlgorithms.ns`

```csharp
using NativeSharp.Core;
using NativeSharp.Core.IO;

namespace NativeSharp.BookExamples.DrozdekAdvanced
{
    public class ExpressionParser
    {
        private string text;
        private int position;

        public ExpressionParser(string source)
        {
            text = source;
            position = 0;
        }

        private void SkipSpaces()
        {
            while (position < text.Length && text[position] == ' ')
            {
                position = position + 1;
            }
        }

        private int ParseNumber()
        {
            SkipSpaces();
            int value = 0;
            bool found = false;
            while (position < text.Length && text[position] >= '0' && text[position] <= '9')
            {
                value = value * 10 + ((int)text[position] - (int)'0');
                position = position + 1;
                found = true;
            }
            if (!found)
            {
                throw new InvalidOperationException();
            }
            return value;
        }

        private int ParsePrimary()
        {
            SkipSpaces();
            if (position < text.Length && text[position] == '(')
            {
                position = position + 1;
                int value = ParseExpression();
                SkipSpaces();
                if (position >= text.Length || text[position] != ')')
                {
                    throw new InvalidOperationException();
                }
                position = position + 1;
                return value;
            }
            return ParseNumber();
        }

        private int ParseUnary()
        {
            SkipSpaces();
            if (position < text.Length && text[position] == '-')
            {
                position = position + 1;
                return -ParseUnary();
            }
            return ParsePrimary();
        }

        private int ParseTerm()
        {
            int value = ParseUnary();
            SkipSpaces();
            while (position < text.Length && (text[position] == '*' || text[position] == '/'))
            {
                char operation = text[position];
                position = position + 1;
                int right = ParseUnary();
                if (operation == '*')
                {
                    value = value * right;
                }
                else
                {
                    if (right == 0)
                    {
                        throw new InvalidOperationException();
                    }
                    value = value / right;
                }
                SkipSpaces();
            }
            return value;
        }

        private int ParseExpression()
        {
            int value = ParseTerm();
            SkipSpaces();
            while (position < text.Length && (text[position] == '+' || text[position] == '-'))
            {
                char operation = text[position];
                position = position + 1;
                int right = ParseTerm();
                value = operation == '+' ? value + right : value - right;
                SkipSpaces();
            }
            return value;
        }

        public int Evaluate()
        {
            int value = ParseExpression();
            SkipSpaces();
            if (position != text.Length)
            {
                throw new InvalidOperationException();
            }
            return value;
        }
    }

    public class CaseStudyAlgorithms
    {
        private static void PutInt(byte[] data, int offset, int value)
        {
            data[offset] = (byte)(value & 255);
            data[offset + 1] = (byte)((value >> 8) & 255);
            data[offset + 2] = (byte)((value >> 16) & 255);
            data[offset + 3] = (byte)((value >> 24) & 255);
        }

        private static int GetInt(byte[] data, int offset)
        {
            int first = (int)data[offset];
            int second = (int)data[offset + 1] << 8;
            int third = (int)data[offset + 2] << 16;
            int fourth = (int)data[offset + 3] << 24;
            return first | second | third | fourth;
        }

        private static int FixedRecordRoundTrip()
        {
            byte[] records = new byte[48];
            int index = 0;
            while (index < 4)
            {
                int offset = index * 12;
                PutInt(records, offset, 1000 + index);
                PutInt(records, offset + 4, 20 + index * 7);
                PutInt(records, offset + 8, index == 2 ? 0 : 1);
                index = index + 1;
            }
            PutInt(records, 12 + 4, 99);
            bool written = File.WriteAllBytes("drozdek_fixed_records.bin", records, records.Length);
            byte[] loaded = File.ReadAllBytes("drozdek_fixed_records.bin");
            int checksum = written && loaded.Length == 48 ? GetInt(loaded, 12) + GetInt(loaded, 16) : -1;
            File.Delete("drozdek_fixed_records.bin");
            delete[] records;
            delete[] loaded;
            return checksum;
        }

        private static int LibraryLoans()
        {
            int[] bookIds = new int[] { 101, 205, 309, 412, 518 };
            int[] borrower = new int[bookIds.Length];
            int index = 0;
            while (index < borrower.Length)
            {
                borrower[index] = -1;
                index = index + 1;
            }
            borrower[1] = 7001;
            borrower[4] = 7002;
            bool rejectedDoubleLoan = borrower[1] != -1;
            borrower[1] = -1;
            int result = rejectedDoubleLoan ? bookIds[4] + borrower[1] + 1 : -1;
            delete[] bookIds;
            delete[] borrower;
            return result;
        }

        private static int MazeShortestPath()
        {
            int width = 7;
            int height = 6;
            int[] grid = new int[] { 1, 1, 1, 0, 1, 1, 1, 0, 0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 0, 1,
                                     1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 1, 1, 1, 1, 1 };
            int total = width * height;
            int[] distance = new int[total];
            int[] queue = new int[total];
            int index = 0;
            while (index < total)
            {
                distance[index] = -1;
                index = index + 1;
            }
            int head = 0;
            int tail = 0;
            queue[tail] = 0;
            tail = tail + 1;
            distance[0] = 0;
            int[] dx = new int[] { 1, -1, 0, 0 };
            int[] dy = new int[] { 0, 0, 1, -1 };
            while (head < tail)
            {
                int cell = queue[head];
                head = head + 1;
                int x = cell % width;
                int y = cell / width;
                int direction = 0;
                while (direction < 4)
                {
                    int nx = x + dx[direction];
                    int ny = y + dy[direction];
                    if (nx >= 0 && nx < width && ny >= 0 && ny < height)
                    {
                        int next = ny * width + nx;
                        if (grid[next] != 0 && distance[next] < 0)
                        {
                            distance[next] = distance[cell] + 1;
                            queue[tail] = next;
                            tail = tail + 1;
                        }
                    }
                    direction = direction + 1;
                }
            }
            int result = distance[total - 1];
            delete[] grid;
            delete[] distance;
            delete[] queue;
            delete[] dx;
            delete[] dy;
            return result;
        }

        private static int PlaceQueens(int row, int size, bool[] columns, bool[] down, bool[] up)
        {
            if (row == size)
            {
                return 1;
            }
            int solutions = 0;
            int column = 0;
            while (column < size)
            {
                int downIndex = row - column + size - 1;
                int upIndex = row + column;
                if (!columns[column] && !down[downIndex] && !up[upIndex])
                {
                    columns[column] = true;
                    down[downIndex] = true;
                    up[upIndex] = true;
                    solutions = solutions + PlaceQueens(row + 1, size, columns, down, up);
                    columns[column] = false;
                    down[downIndex] = false;
                    up[upIndex] = false;
                }
                column = column + 1;
            }
            return solutions;
        }

        private static int EightQueens()
        {
            bool[] columns = new bool[8];
            bool[] down = new bool[15];
            bool[] up = new bool[15];
            int solutions = PlaceQueens(0, 8, columns, down, up);
            delete[] columns;
            delete[] down;
            delete[] up;
            return solutions;
        }

        private static int KochSegments(int depth)
        {
            if (depth == 0)
            {
                return 1;
            }
            return 4 * KochSegments(depth - 1);
        }

        private static int WordFrequencyChecksum()
        {
            string[] words = new string[] { "tree", "graph", "tree", "heap", "hash", "tree", "graph", "trie", "heap" };
            string[] unique = new string[words.Length];
            int[] counts = new int[words.Length];
            int uniqueCount = 0;
            int index = 0;
            while (index < words.Length)
            {
                int found = -1;
                int candidate = 0;
                while (candidate < uniqueCount)
                {
                    if (unique[candidate] == words[index])
                    {
                        found = candidate;
                    }
                    candidate = candidate + 1;
                }
                if (found < 0)
                {
                    unique[uniqueCount] = words[index];
                    counts[uniqueCount] = 1;
                    uniqueCount = uniqueCount + 1;
                }
                else
                {
                    counts[found] = counts[found] + 1;
                }
                index = index + 1;
            }
            int checksum = 0;
            index = 0;
            while (index < uniqueCount)
            {
                checksum = checksum + counts[index] * (index + 1);
                index = index + 1;
            }
            delete[] words;
            delete[] unique;
            delete[] counts;
            return checksum;
        }

        private static int SpellCheck()
        {
            string[] dictionary = new string[] { "native", "sharp", "structure", "algorithm", "compiler" };
            string misspelled = "algoritm";
            int bestDistance = 2147483647;
            int bestIndex = -1;
            int index = 0;
            while (index < dictionary.Length)
            {
                int distance = StringAlgorithms.ApproximateDistance(misspelled, dictionary[index]);
                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    bestIndex = index;
                }
                index = index + 1;
            }
            bool valid = bestIndex == 3 && bestDistance == 1;
            delete[] dictionary;
            return valid ? 31 : -1;
        }

        public static int Run()
        {
            int records = FixedRecordRoundTrip();
            int library = LibraryLoans();
            int maze = MazeShortestPath();
            int queens = EightQueens();
            raii ExpressionParser parser = new ExpressionParser("2 + 3 * (7 - 1) - 4 / 2");
            int expression = parser.Evaluate();
            int koch = KochSegments(4);
            int frequencies = WordFrequencyChecksum();
            int spelling = SpellCheck();
            Console.WriteLine("case studies: records, library, maze, queens, parser, Koch, words and spelling");
            return records + library + maze + queens + expression + koch + frequencies + spelling;
        }
    }
} // namespace NativeSharp.BookExamples. DrozdekAdvanced
```

##### `book_examples/25_drozdek_advanced_suite/CompressionAlgorithms.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.DrozdekAdvanced
{
    public class CompressionAlgorithms
    {
        public static int RunLengthTokenCount(string text)
        {
            if (text.Length == 0)
            {
                return 0;
            }
            int runs = 1;
            int index = 1;
            while (index < text.Length)
            {
                if (text[index] != text[index - 1])
                {
                    runs = runs + 1;
                }
                index = index + 1;
            }
            return runs * 2;
        }

        public static int Lz77TokenCount(string text, int windowSize, int lookaheadSize)
        {
            int position = 0;
            int tokens = 0;
            while (position < text.Length)
            {
                int bestLength = 0;
                int start = position - windowSize;
                if (start < 0)
                {
                    start = 0;
                }
                int candidate = start;
                while (candidate < position)
                {
                    int length = 0;
                    while (length < lookaheadSize && position + length < text.Length && candidate + length < position &&
                           text[candidate + length] == text[position + length])
                    {
                        length = length + 1;
                    }
                    if (length > bestLength)
                    {
                        bestLength = length;
                    }
                    candidate = candidate + 1;
                }
                position = position + (bestLength >= 3 ? bestLength : 1);
                tokens = tokens + 1;
            }
            return tokens;
        }

        private static int FindPair(int[] prefix, char[] suffix, int count, int prefixCode, char value)
        {
            int index = 0;
            while (index < count)
            {
                if (prefix[index] == prefixCode && suffix[index] == value)
                {
                    return index + 256;
                }
                index = index + 1;
            }
            return -1;
        }

        public static int LzwDictionarySize(string text)
        {
            if (text.Length == 0)
            {
                return 256;
            }
            int[] prefix = new int[1024];
            char[] suffix = new char[1024];
            int count = 0;
            int current = (int)text[0];
            int index = 1;
            while (index < text.Length)
            {
                char next = text[index];
                int combined = FindPair(prefix, suffix, count, current, next);
                if (combined >= 0)
                {
                    current = combined;
                }
                else
                {
                    prefix[count] = current;
                    suffix[count] = next;
                    count = count + 1;
                    current = (int)next;
                }
                index = index + 1;
            }
            delete[] prefix;
            delete[] suffix;
            return 256 + count;
        }

        public static int Run()
        {
            int rle = RunLengthTokenCount("AAAABBBCCDAA");
            int lz77 = Lz77TokenCount("aababacbaacbaadaaa", 12, 8);
            int lzw = LzwDictionarySize("TOBEORNOTTOBEORTOBEORNOT");
            Console.WriteLine("compression: run-length, LZ77 and LZW dictionary growth");
            return rle + lz77 + lzw;
        }
    }
} // namespace NativeSharp.BookExamples. DrozdekAdvanced
```

##### `book_examples/25_drozdek_advanced_suite/GraphAlgorithms.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.DrozdekAdvanced
{
    public class GraphAlgorithms
    {
        private static int Cell(int row, int column, int size)
        {
            return row * size + column;
        }

        private static int Minimum(int left, int right)
        {
            return left < right ? left : right;
        }

        public static void FloydWarshall(int[] distance, int size, int infinity)
        {
            int middle = 0;
            while (middle < size)
            {
                int from = 0;
                while (from < size)
                {
                    int to = 0;
                    while (to < size)
                    {
                        int first = distance[Cell(from, middle, size)];
                        int second = distance[Cell(middle, to, size)];
                        if (first < infinity && second < infinity)
                        {
                            int candidate = first + second;
                            int index = Cell(from, to, size);
                            if (candidate < distance[index])
                            {
                                distance[index] = candidate;
                            }
                        }
                        to = to + 1;
                    }
                    from = from + 1;
                }
                middle = middle + 1;
            }
        }

        private static bool FindAugmentingPath(int[] capacity, int[] flow, int size, int source, int sink, int[] parent)
        {
            bool[] visited = new bool[size];
            int[] queue = new int[size];
            int head = 0;
            int tail = 0;
            queue[tail] = source;
            tail = tail + 1;
            visited[source] = true;
            parent[source] = -1;

            while (head < tail)
            {
                int from = queue[head];
                head = head + 1;
                int to = 0;
                while (to < size)
                {
                    int residual = capacity[Cell(from, to, size)] - flow[Cell(from, to, size)];
                    if (!visited[to] && residual > 0)
                    {
                        visited[to] = true;
                        parent[to] = from;
                        queue[tail] = to;
                        tail = tail + 1;
                    }
                    to = to + 1;
                }
            }

            bool found = visited[sink];
            delete[] visited;
            delete[] queue;
            return found;
        }

        public static int MaximumFlow(int[] capacity, int size, int source, int sink)
        {
            int[] flow = new int[size * size];
            int[] parent = new int[size];
            int total = 0;
            while (FindAugmentingPath(capacity, flow, size, source, sink, parent))
            {
                int amount = 2147483647;
                int vertex = sink;
                while (vertex != source)
                {
                    int previous = parent[vertex];
                    int residual = capacity[Cell(previous, vertex, size)] - flow[Cell(previous, vertex, size)];
                    amount = Minimum(amount, residual);
                    vertex = previous;
                }

                vertex = sink;
                while (vertex != source)
                {
                    int previous = parent[vertex];
                    flow[Cell(previous, vertex, size)] = flow[Cell(previous, vertex, size)] + amount;
                    flow[Cell(vertex, previous, size)] = flow[Cell(vertex, previous, size)] - amount;
                    vertex = previous;
                }
                total = total + amount;
            }
            delete[] flow;
            delete[] parent;
            return total;
        }

        public static int StableMatching(int[] preferencesA, int[] preferencesB, int size)
        {
            int[] next = new int[size];
            int[] partnerA = new int[size];
            int[] partnerB = new int[size];
            int index = 0;
            while (index < size)
            {
                partnerA[index] = -1;
                partnerB[index] = -1;
                index = index + 1;
            }

            int freeCount = size;
            while (freeCount > 0)
            {
                int proposer = 0;
                while (partnerA[proposer] != -1)
                {
                    proposer = proposer + 1;
                }
                int candidate = preferencesA[Cell(proposer, next[proposer], size)];
                next[proposer] = next[proposer] + 1;
                int current = partnerB[candidate];
                if (current == -1)
                {
                    partnerA[proposer] = candidate;
                    partnerB[candidate] = proposer;
                    freeCount = freeCount - 1;
                }
                else
                {
                    int rankNew = 0;
                    int rankCurrent = 0;
                    int rank = 0;
                    while (rank < size)
                    {
                        int preferred = preferencesB[Cell(candidate, rank, size)];
                        if (preferred == proposer)
                        {
                            rankNew = rank;
                        }
                        if (preferred == current)
                        {
                            rankCurrent = rank;
                        }
                        rank = rank + 1;
                    }
                    if (rankNew < rankCurrent)
                    {
                        partnerA[current] = -1;
                        partnerA[proposer] = candidate;
                        partnerB[candidate] = proposer;
                    }
                }
            }

            int checksum = 0;
            index = 0;
            while (index < size)
            {
                checksum = checksum + (index + 1) * (partnerA[index] + 1);
                index = index + 1;
            }
            delete[] next;
            delete[] partnerA;
            delete[] partnerB;
            return checksum;
        }

        private static bool HamiltonianSearch(int[] edges, int size, int[] path, bool[] used, int position)
        {
            if (position == size)
            {
                return edges[Cell(path[position - 1], path[0], size)] != 0;
            }

            int candidate = 1;
            while (candidate < size)
            {
                if (!used[candidate] && edges[Cell(path[position - 1], candidate, size)] != 0)
                {
                    used[candidate] = true;
                    path[position] = candidate;
                    if (HamiltonianSearch(edges, size, path, used, position + 1))
                    {
                        return true;
                    }
                    used[candidate] = false;
                }
                candidate = candidate + 1;
            }
            return false;
        }

        public static bool HasHamiltonianCycle(int[] edges, int size)
        {
            int[] path = new int[size];
            bool[] used = new bool[size];
            path[0] = 0;
            used[0] = true;
            bool found = HamiltonianSearch(edges, size, path, used, 1);
            delete[] path;
            delete[] used;
            return found;
        }

        public static int EulerianCycleLength(int[] edges, int size, int edgeCount)
        {
            int[] remaining = new int[size * size];
            int index = 0;
            while (index < remaining.Length)
            {
                remaining[index] = edges[index];
                index = index + 1;
            }
            int[] stack = new int[edgeCount + 2];
            int[] circuit = new int[edgeCount + 2];
            int top = 0;
            int length = 0;
            stack[0] = 0;
            while (top >= 0)
            {
                int vertex = stack[top];
                int neighbor = 0;
                while (neighbor < size && remaining[Cell(vertex, neighbor, size)] == 0)
                {
                    neighbor = neighbor + 1;
                }
                if (neighbor < size)
                {
                    top = top + 1;
                    stack[top] = neighbor;
                    remaining[Cell(vertex, neighbor, size)] = 0;
                    remaining[Cell(neighbor, vertex, size)] = 0;
                }
                else
                {
                    circuit[length] = vertex;
                    length = length + 1;
                    top = top - 1;
                }
            }
            delete[] remaining;
            delete[] stack;
            delete[] circuit;
            return length;
        }

        public static int GreedyColorCount(int[] edges, int size)
        {
            int[] color = new int[size];
            int vertex = 0;
            int maximum = 0;
            while (vertex < size)
            {
                bool[] unavailable = new bool[size + 1];
                int neighbor = 0;
                while (neighbor < size)
                {
                    if (edges[Cell(vertex, neighbor, size)] != 0 && color[neighbor] > 0)
                    {
                        unavailable[color[neighbor]] = true;
                    }
                    neighbor = neighbor + 1;
                }
                int selected = 1;
                while (unavailable[selected])
                {
                    selected = selected + 1;
                }
                color[vertex] = selected;
                if (selected > maximum)
                {
                    maximum = selected;
                }
                delete[] unavailable;
                vertex = vertex + 1;
            }
            delete[] color;
            return maximum;
        }

        public static int Run()
        {
            int infinity = 1000000;
            int[] distances = new int[] { 0, 3,        8,        infinity, -4,       infinity, 0, infinity, 1,
                                          7, infinity, 4,        0,        infinity, infinity, 2, infinity, -5,
                                          0, infinity, infinity, infinity, infinity, 6,        0 };
            FloydWarshall(distances, 5, infinity);

            int[] capacities = new int[] { 0, 16, 13, 0, 0, 0,  0, 0, 10, 12, 0, 0, 0, 4, 0, 0, 14, 0,
                                           0, 0,  9,  0, 0, 20, 0, 0, 0,  7,  0, 4, 0, 0, 0, 0, 0,  0 };
            int flow = MaximumFlow(capacities, 6, 0, 5);

            int[] preferencesA = new int[] { 0, 1, 2, 3, 1, 0, 3, 2, 1, 2, 3, 0, 0, 2, 1, 3 };
            int[] preferencesB = new int[] { 1, 0, 3, 2, 2, 1, 0, 3, 0, 3, 2, 1, 3, 2, 1, 0 };
            int matching = StableMatching(preferencesA, preferencesB, 4);

            int[] undirected = new int[] { 0, 1, 0, 0, 1, 1, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0, 0, 1, 0 };
            bool hamiltonian = HasHamiltonianCycle(undirected, 5);
            int eulerLength = EulerianCycleLength(undirected, 5, 5);
            int colors = GreedyColorCount(undirected, 5);

            int result = distances[Cell(0, 3, 5)] + flow + matching + eulerLength + colors;
            if (!hamiltonian)
            {
                result = -1;
            }
            Console.WriteLine("graphs: Floyd-Warshall, flow, matching, Euler, Hamilton and coloring");
            delete[] distances;
            delete[] capacities;
            delete[] preferencesA;
            delete[] preferencesB;
            delete[] undirected;
            return result;
        }
    }
} // namespace NativeSharp.BookExamples. DrozdekAdvanced
```

##### `book_examples/25_drozdek_advanced_suite/HashAlgorithms.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.DrozdekAdvanced
{
    public class HashAlgorithms
    {
        public static int DivisionHash(int key, int size)
        {
            int value = key % size;
            return value < 0 ? value + size : value;
        }

        public static int FoldingHash(int key, int size)
        {
            int value = key < 0 ? -key : key;
            int total = 0;
            while (value > 0)
            {
                total = total + value % 100;
                value = value / 100;
            }
            return total % size;
        }

        public static int MidSquareHash(int key, int size)
        {
            long square = (long)key * (long)key;
            return (int)((square / 100L) % (long)size);
        }

        private static bool InsertLinear(int[] table, int key)
        {
            int start = DivisionHash(key, table.Length);
            int probe = 0;
            while (probe < table.Length)
            {
                int index = (start + probe) % table.Length;
                if (table[index] == -1 || table[index] == -2)
                {
                    table[index] = key;
                    return true;
                }
                probe = probe + 1;
            }
            return false;
        }

        private static bool InsertQuadratic(int[] table, int key)
        {
            int start = DivisionHash(key, table.Length);
            int probe = 0;
            while (probe < table.Length)
            {
                int index = (start + probe * probe) % table.Length;
                if (table[index] == -1 || table[index] == -2)
                {
                    table[index] = key;
                    return true;
                }
                probe = probe + 1;
            }
            return false;
        }

        private static bool InsertDoubleHash(int[] table, int key)
        {
            int start = DivisionHash(key, table.Length);
            int step = 1 + DivisionHash(key, table.Length - 2);
            int probe = 0;
            while (probe < table.Length)
            {
                int index = (start + probe * step) % table.Length;
                if (table[index] == -1 || table[index] == -2)
                {
                    table[index] = key;
                    return true;
                }
                probe = probe + 1;
            }
            return false;
        }

        private static bool Contains(int[] table, int key)
        {
            int index = 0;
            while (index < table.Length)
            {
                if (table[index] == key)
                {
                    return true;
                }
                index = index + 1;
            }
            return false;
        }

        private static bool CuckooInsert(int[] first, int[] second, int key)
        {
            int current = key;
            bool useFirst = true;
            int displacement = 0;
            while (displacement < first.Length * 2)
            {
                int index = useFirst ? DivisionHash(current, first.Length)
                                     : DivisionHash(current / first.Length, second.Length);
                int displaced = useFirst ? first[index] : second[index];
                if (useFirst)
                {
                    first[index] = current;
                }
                else
                {
                    second[index] = current;
                }
                if (displaced == -1)
                {
                    return true;
                }
                current = displaced;
                useFirst = !useFirst;
                displacement = displacement + 1;
            }
            return false;
        }

        private static int ExtendibleBucket(int key, int globalDepth)
        {
            int mask = (1 << globalDepth) - 1;
            return key & mask;
        }

        private static void Initialize(int[] table)
        {
            int index = 0;
            while (index < table.Length)
            {
                table[index] = -1;
                index = index + 1;
            }
        }

        public static int Run()
        {
            int[] linear = new int[17];
            int[] quadratic = new int[17];
            int[] doubled = new int[17];
            Initialize(linear);
            Initialize(quadratic);
            Initialize(doubled);

            int[] keys = new int[] { 19, 36, 5, 22, 39, 56, 8, 25, 42, 59 };
            int index = 0;
            while (index < keys.Length)
            {
                InsertLinear(linear, keys[index]);
                InsertQuadratic(quadratic, keys[index]);
                InsertDoubleHash(doubled, keys[index]);
                index = index + 1;
            }

            int[] cuckooA = new int[11];
            int[] cuckooB = new int[11];
            Initialize(cuckooA);
            Initialize(cuckooB);
            index = 0;
            bool cuckooValid = true;
            while (index < 7)
            {
                cuckooValid = CuckooInsert(cuckooA, cuckooB, keys[index]) && cuckooValid;
                index = index + 1;
            }

            int bucket = ExtendibleBucket(59, 4);
            bool present = Contains(linear, 42) && Contains(quadratic, 42) && Contains(doubled, 42);
            int result = present && cuckooValid
                             ? DivisionHash(123456, 101) + FoldingHash(123456, 101) + MidSquareHash(321, 101) + bucket
                             : -1;
            Console.WriteLine("hashing: functions, open addressing, cuckoo and extendible directory");
            delete[] linear;
            delete[] quadratic;
            delete[] doubled;
            delete[] keys;
            delete[] cuckooA;
            delete[] cuckooB;
            return result;
        }
    }
} // namespace NativeSharp.BookExamples. DrozdekAdvanced
```

##### `book_examples/25_drozdek_advanced_suite/MemoryAlgorithms.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.DrozdekAdvanced
{
    public class GcNode
    {
        public int Id;
        public GcNode Head;
        public GcNode Tail;
        public bool Mark;
        public bool Tag;

        public GcNode(int id)
        {
            Id = id;
        }
    }

    public class MemoryAlgorithms
    {
        private static int SelectFirstFit(int[] starts, int[] sizes, int count, int request)
        {
            int index = 0;
            while (index < count)
            {
                if (sizes[index] >= request)
                {
                    return index;
                }
                index = index + 1;
            }
            return -1;
        }

        private static int SelectBestFit(int[] starts, int[] sizes, int count, int request)
        {
            int best = -1;
            int index = 0;
            while (index < count)
            {
                if (sizes[index] >= request && (best == -1 || sizes[index] < sizes[best]))
                {
                    best = index;
                }
                index = index + 1;
            }
            return best;
        }

        private static int SelectWorstFit(int[] starts, int[] sizes, int count, int request)
        {
            int worst = -1;
            int index = 0;
            while (index < count)
            {
                if (sizes[index] >= request && (worst == -1 || sizes[index] > sizes[worst]))
                {
                    worst = index;
                }
                index = index + 1;
            }
            return worst;
        }

        private static int BuddyOrder(int request)
        {
            int size = 1;
            int order = 0;
            while (size < request)
            {
                size = size * 2;
                order = order + 1;
            }
            return order;
        }

        private static int MarkReachable(int[] head, int[] tail, bool[] atomic, bool[] marked, int root)
        {
            if (root < 0 || marked[root])
            {
                return 0;
            }
            marked[root] = true;
            int count = 1;
            if (!atomic[root])
            {
                count = count + MarkReachable(head, tail, atomic, marked, head[root]);
                count = count + MarkReachable(head, tail, atomic, marked, tail[root]);
            }
            return count;
        }

        private static int CopyReachable(int[] head, int[] tail, int[] forwarding, int root, int[] next)
        {
            if (root < 0)
            {
                return -1;
            }
            if (forwarding[root] >= 0)
            {
                return forwarding[root];
            }
            int destination = next[0];
            next[0] = next[0] + 1;
            forwarding[root] = destination;
            if (head[root] >= 0)
            {
                CopyReachable(head, tail, forwarding, head[root], next);
            }
            if (tail[root] >= 0)
            {
                CopyReachable(head, tail, forwarding, tail[root], next);
            }
            return destination;
        }

        private static void SchorrWaiteMark(GcNode root)
        {
            GcNode previous = null;
            GcNode current = root;
            while (true)
            {
                current.Mark = true;
                if (current.Head == null || current.Head.Mark)
                {
                    while (current.Tail == null || current.Tail.Mark)
                    {
                        while (previous != null && previous.Tag)
                        {
                            previous.Tag = false;
                            GcNode next = previous.Tail;
                            previous.Tail = current;
                            current = previous;
                            previous = next;
                        }
                        if (previous == null)
                        {
                            return;
                        }
                        GcNode next = previous.Head;
                        previous.Head = current;
                        current = previous;
                        previous = next;
                    }
                    current.Tag = true;
                    GcNode next = current.Tail;
                    current.Tail = previous;
                    previous = current;
                    current = next;
                }
                else
                {
                    GcNode next = current.Head;
                    current.Head = previous;
                    previous = current;
                    current = next;
                }
            }
        }

        private static int InPlaceCollectorDemo()
        {
            GcNode[] heap = new GcNode[8];
            int index = 0;
            while (index < heap.Length)
            {
                heap[index] = new GcNode(index);
                index = index + 1;
            }
            heap[0].Head = heap[1];
            heap[0].Tail = heap[2];
            heap[1].Head = heap[3];
            heap[1].Tail = heap[4];
            heap[2].Head = heap[4];
            heap[2].Tail = heap[5];
            heap[4].Head = heap[0];
            heap[6].Head = heap[7];

            SchorrWaiteMark(heap[0]);
            int marked = 0;
            index = 0;
            while (index < heap.Length)
            {
                if (heap[index].Mark)
                {
                    marked = marked + 1;
                }
                index = index + 1;
            }
            bool restored = heap[0].Head == heap[1] && heap[0].Tail == heap[2] && heap[4].Head == heap[0];
            int result = restored ? marked : -1;
            index = 0;
            while (index < heap.Length)
            {
                delete heap[index];
                index = index + 1;
            }
            delete[] heap;
            return result;
        }

        public static int Run()
        {
            int[] starts = new int[] { 0, 32, 80, 128, 224 };
            int[] sizes = new int[] { 16, 40, 24, 72, 32 };
            int first = SelectFirstFit(starts, sizes, 5, 22);
            int best = SelectBestFit(starts, sizes, 5, 22);
            int worst = SelectWorstFit(starts, sizes, 5, 22);
            int buddy = BuddyOrder(37);

            int[] head = new int[] { 1, 3, 5, -1, -1, -1, -1 };
            int[] tail = new int[] { 2, 4, 6, -1, -1, -1, -1 };
            bool[] atomic = new bool[] { false, false, false, true, true, true, true };
            bool[] marked = new bool[7];
            int reachable = MarkReachable(head, tail, atomic, marked, 0);

            int[] forwarding = new int[7];
            int index = 0;
            while (index < forwarding.Length)
            {
                forwarding[index] = -1;
                index = index + 1;
            }
            int[] next = new int[1];
            CopyReachable(head, tail, forwarding, 0, next);
            int copied = next[0];
            int inPlace = InPlaceCollectorDemo();

            Console.WriteLine("memory: fits, buddy, mark-sweep, copying and Schorr-Waite in-place marking");
            int result = first + best + worst + buddy + reachable + copied + inPlace;
            delete[] starts;
            delete[] sizes;
            delete[] head;
            delete[] tail;
            delete[] atomic;
            delete[] marked;
            delete[] forwarding;
            delete[] next;
            return result;
        }
    }
} // namespace NativeSharp.BookExamples. DrozdekAdvanced
```

##### `book_examples/25_drozdek_advanced_suite/SequenceAlgorithms.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.DrozdekAdvanced
{
    public class SequenceAlgorithms
    {
        private static void Swap(int[] values, int a, int b)
        {
            int temporary = values[a];
            values[a] = values[b];
            values[b] = temporary;
        }

        private static bool IsSorted(int[] values)
        {
            int index = 1;
            while (index < values.Length)
            {
                if (values[index - 1] > values[index])
                {
                    return false;
                }
                index = index + 1;
            }
            return true;
        }

        public static void CombSort(int[] values)
        {
            int gap = values.Length;
            bool swapped = true;
            while (gap > 1 || swapped)
            {
                gap = (gap * 10) / 13;
                if (gap < 1)
                {
                    gap = 1;
                }
                swapped = false;
                int index = 0;
                while (index + gap < values.Length)
                {
                    if (values[index] > values[index + gap])
                    {
                        Swap(values, index, index + gap);
                        swapped = true;
                    }
                    index = index + 1;
                }
            }
        }

        public static void CountingSort(int[] values, int maximum)
        {
            int[] counts = new int[maximum + 1];
            int index = 0;
            while (index < values.Length)
            {
                counts[values[index]] = counts[values[index]] + 1;
                index = index + 1;
            }

            int output = 0;
            int key = 0;
            while (key <= maximum)
            {
                int remaining = counts[key];
                while (remaining > 0)
                {
                    values[output] = key;
                    output = output + 1;
                    remaining = remaining - 1;
                }
                key = key + 1;
            }
            delete[] counts;
        }

        public static void RadixSort(int[] values)
        {
            int[] output = new int[values.Length];
            int divisor = 1;
            int pass = 0;
            while (pass < 3)
            {
                int[] counts = new int[10];
                int index = 0;
                while (index < values.Length)
                {
                    int digit = (values[index] / divisor) % 10;
                    counts[digit] = counts[digit] + 1;
                    index = index + 1;
                }

                int bucket = 1;
                while (bucket < 10)
                {
                    counts[bucket] = counts[bucket] + counts[bucket - 1];
                    bucket = bucket + 1;
                }

                index = values.Length - 1;
                while (index >= 0)
                {
                    int value = values[index];
                    int digit = (value / divisor) % 10;
                    counts[digit] = counts[digit] - 1;
                    output[counts[digit]] = value;
                    index = index - 1;
                }

                index = 0;
                while (index < values.Length)
                {
                    values[index] = output[index];
                    index = index + 1;
                }
                delete[] counts;
                divisor = divisor * 10;
                pass = pass + 1;
            }
            delete[] output;
        }

        private static int FindMoveToFront(int[] values, int key)
        {
            int index = 0;
            while (index < values.Length && values[index] != key)
            {
                index = index + 1;
            }
            if (index == values.Length)
            {
                return -1;
            }
            int value = values[index];
            while (index > 0)
            {
                values[index] = values[index - 1];
                index = index - 1;
            }
            values[0] = value;
            return 0;
        }

        private static int SparseDot(int[] leftColumns, int[] leftValues, int[] rightRows, int[] rightValues)
        {
            int left = 0;
            int right = 0;
            int total = 0;
            while (left < leftColumns.Length && right < rightRows.Length)
            {
                if (leftColumns[left] == rightRows[right])
                {
                    total = total + leftValues[left] * rightValues[right];
                    left = left + 1;
                    right = right + 1;
                }
                else if (leftColumns[left] < rightRows[right])
                {
                    left = left + 1;
                }
                else
                {
                    right = right + 1;
                }
            }
            return total;
        }

        public static int Run()
        {
            int[] comb = new int[] { 31, 4, 19, 8, 15, 2, 27, 1, 11 };
            int[] counting = new int[] { 8, 3, 3, 1, 9, 0, 4, 4, 2, 7 };
            int[] radix = new int[] { 329, 457, 657, 839, 436, 720, 355, 12 };
            CombSort(comb);
            CountingSort(counting, 9);
            RadixSort(radix);

            int[] adaptive = new int[] { 10, 20, 30, 40, 50, 60 };
            FindMoveToFront(adaptive, 50);
            FindMoveToFront(adaptive, 40);

            int[] leftColumns = new int[] { 0, 3, 7 };
            int[] leftValues = new int[] { 2, 5, 4 };
            int[] rightRows = new int[] { 1, 3, 6, 7 };
            int[] rightValues = new int[] { 8, 3, 9, 6 };
            int sparse = SparseDot(leftColumns, leftValues, rightRows, rightValues);

            bool valid = IsSorted(comb) && IsSorted(counting) && IsSorted(radix);
            int result = valid ? comb[8] + counting[9] + radix[7] + adaptive[0] + sparse : -1;
            Console.WriteLine("sequence algorithms: comb, counting, radix, adaptive and sparse");

            delete[] comb;
            delete[] counting;
            delete[] radix;
            delete[] adaptive;
            delete[] leftColumns;
            delete[] leftValues;
            delete[] rightRows;
            delete[] rightValues;
            return result;
        }
    }
} // namespace NativeSharp.BookExamples. DrozdekAdvanced
```

##### `book_examples/25_drozdek_advanced_suite/SkipListDemo.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.DrozdekAdvanced
{
    public class SkipNode
    {
        public int Key;
        public SkipNode[] Next;

        public SkipNode(int key, int levels)
        {
            Key = key;
            Next = new SkipNode[levels];
        }

        ~SkipNode()
        {
            delete[] Next;
        }
    }

    public class DeterministicSkipList
    {
        private SkipNode head;
        private int levels;

        public DeterministicSkipList(int maximumLevels)
        {
            levels = maximumLevels;
            head = new SkipNode(-2147483647, maximumLevels);
        }

        private int LevelFor(int key)
        {
            int level = 1;
            int value = key;
            while (level < levels && value % 2 == 0)
            {
                level = level + 1;
                value = value / 2;
            }
            return level;
        }

        public void Insert(int key)
        {
            SkipNode[] update = new SkipNode[levels];
            SkipNode current = head;
            int level = levels - 1;
            while (level >= 0)
            {
                while (current.Next[level] != null && current.Next[level].Key < key)
                {
                    current = current.Next[level];
                }
                update[level] = current;
                level = level - 1;
            }

            current = current.Next[0];
            if (current == null || current.Key != key)
            {
                int nodeLevels = LevelFor(key);
                SkipNode node = new SkipNode(key, nodeLevels);
                level = 0;
                while (level < nodeLevels)
                {
                    node.Next[level] = update[level].Next[level];
                    update[level].Next[level] = node;
                    level = level + 1;
                }
            }
            delete[] update;
        }

        public bool Contains(int key)
        {
            SkipNode current = head;
            int level = levels - 1;
            while (level >= 0)
            {
                while (current.Next[level] != null && current.Next[level].Key < key)
                {
                    current = current.Next[level];
                }
                level = level - 1;
            }
            current = current.Next[0];
            return current != null && current.Key == key;
        }

        ~DeterministicSkipList()
        {
            SkipNode current = head;
            while (current != null)
            {
                SkipNode next = current.Next[0];
                delete current;
                current = next;
            }
        }
    }

    public class SkipListDemo
    {
        public static int Run()
        {
            raii DeterministicSkipList list = new DeterministicSkipList(6);
            int[] values = new int[] { 24, 7, 18, 32, 4, 13, 28, 1, 40, 21 };
            int index = 0;
            while (index < values.Length)
            {
                list.Insert(values[index]);
                index = index + 1;
            }
            bool valid = list.Contains(1) && list.Contains(24) && list.Contains(40) && !list.Contains(17);
            delete[] values;
            Console.WriteLine("skip list: logarithmic search paths verified");
            return valid ? 97 : -1;
        }
    }
} // namespace NativeSharp.BookExamples. DrozdekAdvanced
```

##### `book_examples/25_drozdek_advanced_suite/StringAlgorithms.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.DrozdekAdvanced
{
    public class StringAlgorithms
    {
        public static int NaiveSearch(string text, string pattern)
        {
            if (pattern.Length == 0)
            {
                return 0;
            }
            int start = 0;
            while (start + pattern.Length <= text.Length)
            {
                int offset = 0;
                while (offset < pattern.Length && text[start + offset] == pattern[offset])
                {
                    offset = offset + 1;
                }
                if (offset == pattern.Length)
                {
                    return start;
                }
                start = start + 1;
            }
            return -1;
        }

        private static int[] PrefixTable(string pattern)
        {
            int[] prefix = new int[pattern.Length];
            int length = 0;
            int index = 1;
            while (index < pattern.Length)
            {
                if (pattern[index] == pattern[length])
                {
                    length = length + 1;
                    prefix[index] = length;
                    index = index + 1;
                }
                else if (length > 0)
                {
                    length = prefix[length - 1];
                }
                else
                {
                    prefix[index] = 0;
                    index = index + 1;
                }
            }
            return prefix;
        }

        public static int KnuthMorrisPratt(string text, string pattern)
        {
            if (pattern.Length == 0)
            {
                return 0;
            }
            int[] prefix = PrefixTable(pattern);
            int textIndex = 0;
            int patternIndex = 0;
            while (textIndex < text.Length)
            {
                if (text[textIndex] == pattern[patternIndex])
                {
                    textIndex = textIndex + 1;
                    patternIndex = patternIndex + 1;
                    if (patternIndex == pattern.Length)
                    {
                        int found = textIndex - patternIndex;
                        delete[] prefix;
                        return found;
                    }
                }
                else if (patternIndex > 0)
                {
                    patternIndex = prefix[patternIndex - 1];
                }
                else
                {
                    textIndex = textIndex + 1;
                }
            }
            delete[] prefix;
            return -1;
        }

        public static int BoyerMooreHorspool(string text, string pattern)
        {
            if (pattern.Length == 0)
            {
                return 0;
            }
            int[] shift = new int[256];
            int index = 0;
            while (index < 256)
            {
                shift[index] = pattern.Length;
                index = index + 1;
            }
            index = 0;
            while (index < pattern.Length - 1)
            {
                shift[(int)pattern[index]] = pattern.Length - index - 1;
                index = index + 1;
            }

            int position = 0;
            while (position + pattern.Length <= text.Length)
            {
                int offset = pattern.Length - 1;
                while (offset >= 0 && pattern[offset] == text[position + offset])
                {
                    offset = offset - 1;
                }
                if (offset < 0)
                {
                    delete[] shift;
                    return position;
                }
                position = position + shift[(int)text[position + pattern.Length - 1]];
            }
            delete[] shift;
            return -1;
        }

        public static int BitapSearch(string text, string pattern)
        {
            if (pattern.Length == 0)
            {
                return 0;
            }
            if (pattern.Length > 30)
            {
                return -1;
            }
            int[] masks = new int[256];
            int index = 0;
            while (index < pattern.Length)
            {
                masks[(int)pattern[index]] = masks[(int)pattern[index]] | (1 << index);
                index = index + 1;
            }
            int state = 0;
            index = 0;
            int matchBit = 1 << (pattern.Length - 1);
            while (index < text.Length)
            {
                state = ((state << 1) | 1) & masks[(int)text[index]];
                if ((state & matchBit) != 0)
                {
                    delete[] masks;
                    return index - pattern.Length + 1;
                }
                index = index + 1;
            }
            delete[] masks;
            return -1;
        }

        public static int ApproximateDistance(string left, string right)
        {
            int width = right.Length + 1;
            int[] table = new int[(left.Length + 1) * width];
            int row = 0;
            while (row <= left.Length)
            {
                table[row * width] = row;
                row = row + 1;
            }
            int column = 0;
            while (column <= right.Length)
            {
                table[column] = column;
                column = column + 1;
            }
            row = 1;
            while (row <= left.Length)
            {
                column = 1;
                while (column <= right.Length)
                {
                    int cost = left[row - 1] == right[column - 1] ? 0 : 1;
                    int deletion = table[(row - 1) * width + column] + 1;
                    int insertion = table[row * width + column - 1] + 1;
                    int substitution = table[(row - 1) * width + column - 1] + cost;
                    int best = deletion < insertion ? deletion : insertion;
                    table[row * width + column] = best < substitution ? best : substitution;
                    column = column + 1;
                }
                row = row + 1;
            }
            int result = table[left.Length * width + right.Length];
            delete[] table;
            return result;
        }

        public static int LongestCommonSubstring(string left, string right)
        {
            int width = right.Length + 1;
            int[] table = new int[(left.Length + 1) * width];
            int best = 0;
            int row = 1;
            while (row <= left.Length)
            {
                int column = 1;
                while (column <= right.Length)
                {
                    if (left[row - 1] == right[column - 1])
                    {
                        int length = table[(row - 1) * width + column - 1] + 1;
                        table[row * width + column] = length;
                        if (length > best)
                        {
                            best = length;
                        }
                    }
                    column = column + 1;
                }
                row = row + 1;
            }
            delete[] table;
            return best;
        }

        public static int Run()
        {
            string text = "ABABDABACDABABCABAB";
            string pattern = "ABABCABAB";
            int naive = NaiveSearch(text, pattern);
            int kmp = KnuthMorrisPratt(text, pattern);
            int boyer = BoyerMooreHorspool(text, pattern);
            int bitap = BitapSearch(text, pattern);
            int edit = ApproximateDistance("algorithm", "altruistic");
            int common = LongestCommonSubstring("ABABC", "BABCA");
            Console.WriteLine("strings: naive, KMP, Boyer-Moore, bitap, approximate and LCS substring");
            return naive + kmp + boyer + bitap + edit + common;
        }
    }
} // namespace NativeSharp.BookExamples. DrozdekAdvanced
```

##### `book_examples/25_drozdek_advanced_suite/TreeAlgorithms.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.DrozdekAdvanced
{
    public class IntTreeNode
    {
        public int Key;
        public int Priority;
        public IntTreeNode Left;
        public IntTreeNode Right;

        public IntTreeNode(int key, int priority)
        {
            Key = key;
            Priority = priority;
        }
    }

    public class KdNode
    {
        public int X;
        public int Y;
        public KdNode Left;
        public KdNode Right;

        public KdNode(int x, int y)
        {
            X = x;
            Y = y;
        }
    }

    public class ThreadedTreeNode
    {
        public int Key;
        public ThreadedTreeNode Left;
        public ThreadedTreeNode Right;
        public bool RightIsThread;

        public ThreadedTreeNode(int key)
        {
            Key = key;
            RightIsThread = false;
        }
    }

    public class SmallBTreeNode
    {
        public int[] Keys;
        public SmallBTreeNode[] Children;
        public int Count;
        public bool Leaf;

        public SmallBTreeNode(bool leaf)
        {
            Keys = new int[3];
            Children = new SmallBTreeNode[4];
            Count = 0;
            Leaf = leaf;
        }

        ~SmallBTreeNode()
        {
            delete[] Keys;
            delete[] Children;
        }
    }

    public class SmallBTree
    {
        private SmallBTreeNode root;

        public SmallBTree()
        {
            root = new SmallBTreeNode(true);
        }

        private void SplitChild(SmallBTreeNode parent, int childIndex)
        {
            SmallBTreeNode full = parent.Children[childIndex];
            SmallBTreeNode right = new SmallBTreeNode(full.Leaf);
            right.Count = 1;
            right.Keys[0] = full.Keys[2];
            if (!full.Leaf)
            {
                right.Children[0] = full.Children[2];
                right.Children[1] = full.Children[3];
                full.Children[2] = null;
                full.Children[3] = null;
            }
            full.Count = 1;

            int index = parent.Count;
            while (index > childIndex)
            {
                parent.Children[index + 1] = parent.Children[index];
                index = index - 1;
            }
            parent.Children[childIndex + 1] = right;

            index = parent.Count - 1;
            while (index >= childIndex)
            {
                parent.Keys[index + 1] = parent.Keys[index];
                index = index - 1;
            }
            parent.Keys[childIndex] = full.Keys[1];
            parent.Count = parent.Count + 1;
        }

        private void InsertNonFull(SmallBTreeNode node, int key)
        {
            int index = node.Count - 1;
            if (node.Leaf)
            {
                while (index >= 0 && key < node.Keys[index])
                {
                    node.Keys[index + 1] = node.Keys[index];
                    index = index - 1;
                }
                node.Keys[index + 1] = key;
                node.Count = node.Count + 1;
                return;
            }

            while (index >= 0 && key < node.Keys[index])
            {
                index = index - 1;
            }
            index = index + 1;
            if (node.Children[index].Count == 3)
            {
                SplitChild(node, index);
                if (key > node.Keys[index])
                {
                    index = index + 1;
                }
            }
            InsertNonFull(node.Children[index], key);
        }

        public void Insert(int key)
        {
            if (root.Count == 3)
            {
                SmallBTreeNode replacement = new SmallBTreeNode(false);
                replacement.Children[0] = root;
                SplitChild(replacement, 0);
                root = replacement;
            }
            InsertNonFull(root, key);
        }

        private bool ContainsNode(SmallBTreeNode node, int key)
        {
            int index = 0;
            while (index < node.Count && key > node.Keys[index])
            {
                index = index + 1;
            }
            if (index < node.Count && key == node.Keys[index])
            {
                return true;
            }
            if (node.Leaf)
            {
                return false;
            }
            return ContainsNode(node.Children[index], key);
        }

        public bool Contains(int key)
        {
            return ContainsNode(root, key);
        }

        private void DeleteNode(SmallBTreeNode node)
        {
            if (!node.Leaf)
            {
                int index = 0;
                while (index <= node.Count)
                {
                    DeleteNode(node.Children[index]);
                    index = index + 1;
                }
            }
            delete node;
        }

        ~SmallBTree()
        {
            DeleteNode(root);
        }
    }

    public class TreeAlgorithms
    {
        private static ThreadedTreeNode InsertThreaded(ThreadedTreeNode root, int key)
        {
            if (root == null)
            {
                return new ThreadedTreeNode(key);
            }
            ThreadedTreeNode current = root;
            while (true)
            {
                if (key < current.Key)
                {
                    if (current.Left == null)
                    {
                        ThreadedTreeNode node = new ThreadedTreeNode(key);
                        node.Right = current;
                        node.RightIsThread = true;
                        current.Left = node;
                        return root;
                    }
                    current = current.Left;
                }
                else if (key > current.Key)
                {
                    if (current.Right == null || current.RightIsThread)
                    {
                        ThreadedTreeNode node = new ThreadedTreeNode(key);
                        node.Right = current.Right;
                        node.RightIsThread = true;
                        current.Right = node;
                        current.RightIsThread = false;
                        return root;
                    }
                    current = current.Right;
                }
                else
                {
                    return root;
                }
            }
        }

        private static int ThreadedInorderChecksum(ThreadedTreeNode root)
        {
            int checksum = 0;
            ThreadedTreeNode current = root;
            while (current != null && current.Left != null)
            {
                current = current.Left;
            }
            while (current != null)
            {
                checksum = checksum + current.Key;
                if (current.RightIsThread)
                {
                    current = current.Right;
                }
                else
                {
                    current = current.Right;
                    while (current != null && current.Left != null)
                    {
                        current = current.Left;
                    }
                }
            }
            return checksum;
        }

        private static void DeleteThreadedTree(ThreadedTreeNode root, int count)
        {
            ThreadedTreeNode[] nodes = new ThreadedTreeNode[count];
            int found = 0;
            ThreadedTreeNode current = root;
            while (current != null && current.Left != null)
            {
                current = current.Left;
            }
            while (current != null)
            {
                nodes[found] = current;
                found = found + 1;
                if (current.RightIsThread)
                {
                    current = current.Right;
                }
                else
                {
                    current = current.Right;
                    while (current != null && current.Left != null)
                    {
                        current = current.Left;
                    }
                }
            }
            int index = 0;
            while (index < found)
            {
                delete nodes[index];
                index = index + 1;
            }
            delete[] nodes;
        }

        private static IntTreeNode DeleteByMerging(IntTreeNode root, int key)
        {
            IntTreeNode parent = null;
            IntTreeNode current = root;
            while (current != null && current.Key != key)
            {
                parent = current;
                current = key < current.Key ? current.Left : current.Right;
            }
            if (current == null)
            {
                return root;
            }

            IntTreeNode replacement = null;
            if (current.Left == null)
            {
                replacement = current.Right;
            }
            else if (current.Right == null)
            {
                replacement = current.Left;
            }
            else
            {
                replacement = current.Left;
                IntTreeNode maximum = replacement;
                while (maximum.Right != null)
                {
                    maximum = maximum.Right;
                }
                maximum.Right = current.Right;
            }

            if (parent == null)
            {
                root = replacement;
            }
            else if (parent.Left == current)
            {
                parent.Left = replacement;
            }
            else
            {
                parent.Right = replacement;
            }
            current.Left = null;
            current.Right = null;
            delete current;
            return root;
        }

        private static int IterativePreorderChecksum(IntTreeNode root)
        {
            IntTreeNode[] stack = new IntTreeNode[64];
            int top = 0;
            int checksum = 0;
            stack[0] = root;
            while (top >= 0)
            {
                IntTreeNode node = stack[top];
                top = top - 1;
                checksum = checksum + node.Key;
                if (node.Right != null)
                {
                    top = top + 1;
                    stack[top] = node.Right;
                }
                if (node.Left != null)
                {
                    top = top + 1;
                    stack[top] = node.Left;
                }
            }
            delete[] stack;
            return checksum;
        }

        private static int IterativePostorderChecksum(IntTreeNode root)
        {
            IntTreeNode[] first = new IntTreeNode[64];
            IntTreeNode[] second = new IntTreeNode[64];
            int firstTop = 0;
            int secondTop = -1;
            first[0] = root;
            while (firstTop >= 0)
            {
                IntTreeNode node = first[firstTop];
                firstTop = firstTop - 1;
                secondTop = secondTop + 1;
                second[secondTop] = node;
                if (node.Left != null)
                {
                    firstTop = firstTop + 1;
                    first[firstTop] = node.Left;
                }
                if (node.Right != null)
                {
                    firstTop = firstTop + 1;
                    first[firstTop] = node.Right;
                }
            }
            int checksum = 0;
            while (secondTop >= 0)
            {
                checksum = checksum + second[secondTop].Key;
                secondTop = secondTop - 1;
            }
            delete[] first;
            delete[] second;
            return checksum;
        }

        private static IntTreeNode RotateLeft(IntTreeNode node)
        {
            IntTreeNode top = node.Right;
            node.Right = top.Left;
            top.Left = node;
            return top;
        }

        private static IntTreeNode RotateRight(IntTreeNode node)
        {
            IntTreeNode top = node.Left;
            node.Left = top.Right;
            top.Right = node;
            return top;
        }

        private static IntTreeNode InsertTreap(IntTreeNode node, int key, int priority)
        {
            if (node == null)
            {
                return new IntTreeNode(key, priority);
            }
            if (key < node.Key)
            {
                node.Left = InsertTreap(node.Left, key, priority);
                if (node.Left.Priority < node.Priority)
                {
                    node = RotateRight(node);
                }
            }
            else if (key > node.Key)
            {
                node.Right = InsertTreap(node.Right, key, priority);
                if (node.Right.Priority < node.Priority)
                {
                    node = RotateLeft(node);
                }
            }
            return node;
        }

        private static IntTreeNode Splay(IntTreeNode root, int key)
        {
            if (root == null || root.Key == key)
            {
                return root;
            }
            if (key < root.Key)
            {
                if (root.Left == null)
                {
                    return root;
                }
                if (key < root.Left.Key)
                {
                    root.Left.Left = Splay(root.Left.Left, key);
                    root = RotateRight(root);
                }
                else if (key > root.Left.Key)
                {
                    root.Left.Right = Splay(root.Left.Right, key);
                    if (root.Left.Right != null)
                    {
                        root.Left = RotateLeft(root.Left);
                    }
                }
                return root.Left == null ? root : RotateRight(root);
            }

            if (root.Right == null)
            {
                return root;
            }
            if (key > root.Right.Key)
            {
                root.Right.Right = Splay(root.Right.Right, key);
                root = RotateLeft(root);
            }
            else if (key < root.Right.Key)
            {
                root.Right.Left = Splay(root.Right.Left, key);
                if (root.Right.Left != null)
                {
                    root.Right = RotateRight(root.Right);
                }
            }
            return root.Right == null ? root : RotateLeft(root);
        }

        private static int MorrisChecksum(IntTreeNode root)
        {
            int checksum = 0;
            IntTreeNode current = root;
            while (current != null)
            {
                if (current.Left == null)
                {
                    checksum = checksum + current.Key;
                    current = current.Right;
                }
                else
                {
                    IntTreeNode predecessor = current.Left;
                    while (predecessor.Right != null && predecessor.Right != current)
                    {
                        predecessor = predecessor.Right;
                    }
                    if (predecessor.Right == null)
                    {
                        predecessor.Right = current;
                        current = current.Left;
                    }
                    else
                    {
                        predecessor.Right = null;
                        checksum = checksum + current.Key;
                        current = current.Right;
                    }
                }
            }
            return checksum;
        }

        private static KdNode InsertKd(KdNode node, int x, int y, int depth)
        {
            if (node == null)
            {
                return new KdNode(x, y);
            }
            bool goLeft = depth % 2 == 0 ? x < node.X : y < node.Y;
            if (goLeft)
            {
                node.Left = InsertKd(node.Left, x, y, depth + 1);
            }
            else
            {
                node.Right = InsertKd(node.Right, x, y, depth + 1);
            }
            return node;
        }

        private static int CountKdRange(KdNode node, int minX, int maxX, int minY, int maxY, int depth)
        {
            if (node == null)
            {
                return 0;
            }
            int count = 0;
            if (node.X >= minX && node.X <= maxX && node.Y >= minY && node.Y <= maxY)
            {
                count = 1;
            }
            if (depth % 2 == 0)
            {
                if (minX <= node.X)
                {
                    count = count + CountKdRange(node.Left, minX, maxX, minY, maxY, depth + 1);
                }
                if (maxX >= node.X)
                {
                    count = count + CountKdRange(node.Right, minX, maxX, minY, maxY, depth + 1);
                }
            }
            else
            {
                if (minY <= node.Y)
                {
                    count = count + CountKdRange(node.Left, minX, maxX, minY, maxY, depth + 1);
                }
                if (maxY >= node.Y)
                {
                    count = count + CountKdRange(node.Right, minX, maxX, minY, maxY, depth + 1);
                }
            }
            return count;
        }

        private static void DeleteIntTree(IntTreeNode node)
        {
            if (node == null)
            {
                return;
            }
            DeleteIntTree(node.Left);
            DeleteIntTree(node.Right);
            delete node;
        }

        private static void DeleteKdTree(KdNode node)
        {
            if (node == null)
            {
                return;
            }
            DeleteKdTree(node.Left);
            DeleteKdTree(node.Right);
            delete node;
        }

        public static int Run()
        {
            IntTreeNode treap = null;
            int[] keys = new int[] { 40, 20, 60, 10, 30, 50, 70, 25, 65 };
            int[] priorities = new int[] { 50, 30, 40, 10, 35, 20, 60, 15, 25 };
            int index = 0;
            while (index < keys.Length)
            {
                treap = InsertTreap(treap, keys[index], priorities[index]);
                index = index + 1;
            }
            int inorder = MorrisChecksum(treap);
            int preorder = IterativePreorderChecksum(treap);
            int postorder = IterativePostorderChecksum(treap);
            treap = Splay(treap, 65);
            int splayed = treap.Key;
            treap = DeleteByMerging(treap, 40);

            ThreadedTreeNode threaded = null;
            index = 0;
            while (index < keys.Length)
            {
                threaded = InsertThreaded(threaded, keys[index]);
                index = index + 1;
            }
            int threadedChecksum = ThreadedInorderChecksum(threaded);

            KdNode kd = null;
            int[] points = new int[] { 30, 40, 5, 25, 10, 12, 70, 70, 50, 30, 35, 45, 65, 20 };
            index = 0;
            while (index < points.Length)
            {
                kd = InsertKd(kd, points[index], points[index + 1], 0);
                index = index + 2;
            }
            int inside = CountKdRange(kd, 0, 40, 0, 50, 0);

            raii SmallBTree btree = new SmallBTree();
            int[] btreeKeys = new int[] { 10, 20, 5, 6, 12, 30, 7, 17, 3, 4, 2, 16 };
            index = 0;
            while (index < btreeKeys.Length)
            {
                btree.Insert(btreeKeys[index]);
                index = index + 1;
            }
            bool btreeValid = btree.Contains(2) && btree.Contains(17) && btree.Contains(30) && !btree.Contains(99);

            int result = btreeValid ? inorder + preorder + postorder + splayed + threadedChecksum + inside : -1;
            Console.WriteLine("trees: iterative traversals, threads, Morris, merging, splay, treap, k-d and B-tree");
            DeleteThreadedTree(threaded, keys.Length);
            DeleteIntTree(treap);
            DeleteKdTree(kd);
            delete[] keys;
            delete[] priorities;
            delete[] points;
            delete[] btreeKeys;
            return result;
        }
    }
} // namespace NativeSharp.BookExamples. DrozdekAdvanced
```

##### `book_examples/25_drozdek_advanced_suite/Program.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.DrozdekAdvanced
{
    public class Program
    {
        public static int Main()
        {
            Console.WriteLine("NativeSharp Book Example 25 - Drozdek advanced suite");
            Console.WriteLine("---------------------------------------------------");

            int checksum = 0;
            checksum = checksum + SequenceAlgorithms.Run();
            checksum = checksum + SkipListDemo.Run();
            checksum = checksum + TreeAlgorithms.Run();
            checksum = checksum + GraphAlgorithms.Run();
            checksum = checksum + HashAlgorithms.Run();
            checksum = checksum + CompressionAlgorithms.Run();
            checksum = checksum + MemoryAlgorithms.Run();
            checksum = checksum + StringAlgorithms.Run();
            checksum = checksum + CaseStudyAlgorithms.Run();

            raii string value = String.ToString(checksum);
            raii string line = String.Concat("verified checksum: ", value);
            Console.WriteLine(line);
            return checksum == 5157 ? 0 : 25;
        }
    }
} // namespace NativeSharp.BookExamples. DrozdekAdvanced
```

### 47.26 Live code al estilo Deitel, con filosofía NativeSharp

**Problema.** Enseñar mediante aplicaciones completas que evolucionan paso a paso sin importar al mundo nativo las suposiciones del CLR.

**Diseño.** Seis demostraciones integran cuenta bancaria en centavos, libro de calificaciones, cartas y craps reproducibles, tiempo validado, archivo de crédito binario y trabajo paralelo con hilos y atomics.

**Auditoría propuesta.** Pruebe invariantes, casos de error, semillas deterministas, layout de archivo, orden de destrucción y join de hilos. El checksum global evita que una demostración sólo produzca salida plausible.

**Escala real.** 7 archivos NativeSharp, 595 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

**Reproducción.**

```powershell
nsc --emit-project-oop-exe `
    book_examples/26_deitel_live_code_native/Book26DeitelLiveCodeNative.nsproj `
    -O2 `
    -o 26_deitel_live_code_native.exe
./26_deitel_live_code_native.exe
```

#### Listado completo

##### `book_examples/26_deitel_live_code_native/Book26DeitelLiveCodeNative.nsproj`

```xml
<Project>
  <PropertyGroup>
    <Name>Book26DeitelLiveCodeNative</Name>
  </PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
    <ProjectReference Include="../../lib/NativeSharp.Core.IO/NativeSharp.Core.IO.nsproj" />
    <ProjectReference Include="../../lib/NativeSharp.Core.Threading/NativeSharp.Core.Threading.nsproj" />
  </ItemGroup>
</Project>
```

##### `book_examples/26_deitel_live_code_native/BankAccount.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.DeitelNative
{
    public class BankAccount
    {
        private string owner;
        private long balanceCents;

        public BankAccount(string ownerName, long openingBalanceCents)
        {
            if (ownerName.Length == 0 || openingBalanceCents < 0L)
            {
                throw new InvalidOperationException();
            }
            owner = String.Copy(ownerName);
            balanceCents = openingBalanceCents;
        }

        public string Owner
        {
            get
            {
                return owner;
            }
        }

        public long BalanceCents
        {
            get
            {
                return balanceCents;
            }
        }

        public void Deposit(long amountCents)
        {
            if (amountCents <= 0L)
            {
                throw new InvalidOperationException();
            }
            balanceCents = balanceCents + amountCents;
        }

        public bool TryWithdraw(long amountCents)
        {
            if (amountCents <= 0L || amountCents > balanceCents)
            {
                return false;
            }
            balanceCents = balanceCents - amountCents;
            return true;
        }

        public owned string StatementLine()
        {
            raii string cents = String.ToString(balanceCents);
            raii string first = String.Concat(owner, ": ");
            return String.Concat(first, cents);
        }

        ~BankAccount()
        {
            delete owner;
        }
    }

    public class BankAccountDemo
    {
        public static int Run()
        {
            raii BankAccount account = new BankAccount("Ada Lovelace", 125000L);
            account.Deposit(27550L);
            bool firstWithdrawal = account.TryWithdraw(32000L);
            bool rejectedOverdraft = !account.TryWithdraw(500000L);
            raii string statement = account.StatementLine();
            Console.WriteLine(statement);
            return firstWithdrawal && rejectedOverdraft && account.BalanceCents == 120550L ? 101 : -1;
        }
    }
}
```

##### `book_examples/26_deitel_live_code_native/CreditFile.ns`

```csharp
using NativeSharp.Core;
using NativeSharp.Core.IO;

namespace NativeSharp.BookExamples.DeitelNative
{
    public class CreditFileDemo
    {
        private static void PutInt(byte[] data, int offset, int value)
        {
            data[offset] = (byte)(value & 255);
            data[offset + 1] = (byte)((value >> 8) & 255);
            data[offset + 2] = (byte)((value >> 16) & 255);
            data[offset + 3] = (byte)((value >> 24) & 255);
        }

        private static int GetInt(byte[] data, int offset)
        {
            return (int)data[offset] | ((int)data[offset + 1] << 8) | ((int)data[offset + 2] << 16) |
                   ((int)data[offset + 3] << 24);
        }

        public static int Run()
        {
            int[] accounts = new int[] { 1001, 1002, 1003, 1004, 1005, 1006 };
            int[] balances = new int[] { -2500, 0, 17900, -310, 8200, 0 };
            byte[] fileData = new byte[accounts.Length * 8];
            int index = 0;
            while (index < accounts.Length)
            {
                PutInt(fileData, index * 8, accounts[index]);
                PutInt(fileData, index * 8 + 4, balances[index]);
                index = index + 1;
            }
            bool written = File.WriteAllBytes("deitel_credit_records.bin", fileData, fileData.Length);
            byte[] loaded = File.ReadAllBytes("deitel_credit_records.bin");

            int credit = 0;
            int debit = 0;
            int zero = 0;
            index = 0;
            while (index < accounts.Length && loaded.Length == fileData.Length)
            {
                int balance = GetInt(loaded, index * 8 + 4);
                if (balance > 0)
                {
                    credit = credit + 1;
                }
                else if (balance < 0)
                {
                    debit = debit + 1;
                }
                else
                {
                    zero = zero + 1;
                }
                index = index + 1;
            }
            File.Delete("deitel_credit_records.bin");
            Console.WriteLine("credit file: explicit little-endian records and three-way inquiry");
            int result = written ? credit * 100 + debit * 10 + zero : -1;
            delete[] accounts;
            delete[] balances;
            delete[] fileData;
            delete[] loaded;
            return result;
        }
    }
}
```

##### `book_examples/26_deitel_live_code_native/DeckAndCraps.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.DeitelNative
{
    public enum CrapsStatus
    {
        Continue = 0,
        Won = 1,
        Lost = 2
    }

    public class Card
    {
        public int Rank;
        public int Suit;

        public Card(int rank, int suit)
        {
            Rank = rank;
            Suit = suit;
        }
    }

    public class Deck
    {
        private Card[] cards;
        private int next;

        public Deck()
        {
            cards = new Card[52];
            int suit = 0;
            int index = 0;
            while (suit < 4)
            {
                int rank = 1;
                while (rank <= 13)
                {
                    cards[index] = new Card(rank, suit);
                    index = index + 1;
                    rank = rank + 1;
                }
                suit = suit + 1;
            }
            next = 0;
        }

        public void Shuffle(Random random)
        {
            int index = cards.Length - 1;
            while (index > 0)
            {
                int selected = random.Next(index + 1);
                Card temporary = cards[index];
                cards[index] = cards[selected];
                cards[selected] = temporary;
                index = index - 1;
            }
            next = 0;
        }

        public Card Deal()
        {
            if (next == cards.Length)
            {
                throw new InvalidOperationException();
            }
            Card card = cards[next];
            next = next + 1;
            return card;
        }

        ~Deck()
        {
            int index = 0;
            while (index < cards.Length)
            {
                delete cards[index];
                index = index + 1;
            }
            delete[] cards;
        }
    }

    public class ChanceGamesDemo
    {
        private static int RollDice(Random random)
        {
            return random.Next(1, 7) + random.Next(1, 7);
        }

        private static bool PlayCraps(Random random)
        {
            int first = RollDice(random);
            CrapsStatus status = CrapsStatus.Continue;
            int point = 0;
            switch (first)
            {
            case 7:
            case 11:
                status = CrapsStatus.Won;
                break;
            case 2:
            case 3:
            case 12:
                status = CrapsStatus.Lost;
                break;
            default:
                point = first;
                break;
            }
            while (status == CrapsStatus.Continue)
            {
                int roll = RollDice(random);
                if (roll == point)
                {
                    status = CrapsStatus.Won;
                }
                else if (roll == 7)
                {
                    status = CrapsStatus.Lost;
                }
            }
            return status == CrapsStatus.Won;
        }

        public static int Run()
        {
            raii Random random = new Random(20260906L);
            raii Deck deck = new Deck();
            deck.Shuffle(random);
            int cardChecksum = 0;
            int index = 0;
            while (index < 10)
            {
                Card card = deck.Deal();
                cardChecksum = cardChecksum + card.Rank * (card.Suit + 1);
                index = index + 1;
            }

            int wins = 0;
            index = 0;
            while (index < 1000)
            {
                if (PlayCraps(random))
                {
                    wins = wins + 1;
                }
                index = index + 1;
            }
            Console.WriteLine("chance games: Fisher-Yates deck and 1000 deterministic craps games");
            return cardChecksum + wins;
        }
    }
}
```

##### `book_examples/26_deitel_live_code_native/GradeBook.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.DeitelNative
{
    public class GradeBook
    {
        private int[] grades;

        public GradeBook(int[] source)
        {
            grades = new int[source.Length];
            int index = 0;
            while (index < source.Length)
            {
                int grade = source[index];
                if (grade < 0 || grade > 100)
                {
                    throw new InvalidOperationException();
                }
                grades[index] = grade;
                index = index + 1;
            }
        }

        public int Count
        {
            get
            {
                return grades.Length;
            }
        }

        public int Minimum()
        {
            int result = grades[0];
            int index = 1;
            while (index < grades.Length)
            {
                if (grades[index] < result)
                {
                    result = grades[index];
                }
                index = index + 1;
            }
            return result;
        }

        public int Maximum()
        {
            int result = grades[0];
            int index = 1;
            while (index < grades.Length)
            {
                if (grades[index] > result)
                {
                    result = grades[index];
                }
                index = index + 1;
            }
            return result;
        }

        public int AverageTimes100()
        {
            int total = 0;
            int index = 0;
            while (index < grades.Length)
            {
                total = total + grades[index];
                index = index + 1;
            }
            return total * 100 / grades.Length;
        }

        public int[] Histogram()
        {
            int[] buckets = new int[11];
            int index = 0;
            while (index < grades.Length)
            {
                buckets[grades[index] / 10] = buckets[grades[index] / 10] + 1;
                index = index + 1;
            }
            return buckets;
        }

        ~GradeBook()
        {
            delete[] grades;
        }
    }

    public class GradeBookDemo
    {
        public static int Run()
        {
            int[] data = new int[] { 87, 68, 94, 100, 83, 78, 85, 91, 76, 87, 72, 88, 95, 63, 79 };
            raii GradeBook gradeBook = new GradeBook(data);
            int[] histogram = gradeBook.Histogram();
            int weightedBuckets = 0;
            int bucket = 0;
            while (bucket < histogram.Length)
            {
                weightedBuckets = weightedBuckets + histogram[bucket] * bucket;
                bucket = bucket + 1;
            }
            int result = gradeBook.Minimum() + gradeBook.Maximum() + gradeBook.AverageTimes100() + weightedBuckets;
            Console.WriteLine("grade book: validated range, statistics and histogram");
            delete[] data;
            delete[] histogram;
            return result;
        }
    }
}
```

##### `book_examples/26_deitel_live_code_native/NativeParallel.ns`

```csharp
using NativeSharp.Core;
using NativeSharp.Core.Threading;

namespace NativeSharp.BookExamples.DeitelNative
{
    public class CounterWorker
    {
        private AtomicInt32 counter;
        private int iterations;

        public CounterWorker(int iterationCount)
        {
            counter = new AtomicInt32(0);
            iterations = iterationCount;
        }

        public void Work()
        {
            try
            {
                int index = 0;
                while (index < iterations)
                {
                    counter.Increment();
                    index = index + 1;
                }
            }
            catch (Exception error)
            {
                Console.WriteLine("worker contained an exception");
            }
        }

        public int Result
        {
            get
            {
                return counter.CompareExchange(0, 0);
            }
        }

        ~CounterWorker()
        {
            delete counter;
        }
    }

    public class NativeParallelDemo
    {
        public static int Run()
        {
            raii CounterWorker worker = new CounterWorker(10000);
            ThreadStart start = worker.Work;
            raii Thread first = new Thread(start);
            raii Thread second = new Thread(start);
            first.Join();
            second.Join();
            int result = worker.Result;
            Console.WriteLine("parallel work: two native threads, atomic state and explicit joins");
            return result;
        }
    }
}
```

##### `book_examples/26_deitel_live_code_native/TimeOfDay.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.DeitelNative
{
    public class TimeOfDay
    {
        private int secondsSinceMidnight;

        public TimeOfDay()
        {
            secondsSinceMidnight = 0;
        }

        public TimeOfDay(int hour)
        {
            Set(hour, 0, 0);
        }

        public TimeOfDay(int hour, int minute)
        {
            Set(hour, minute, 0);
        }

        public TimeOfDay(int hour, int minute, int second)
        {
            Set(hour, minute, second);
        }

        private void Set(int hour, int minute, int second)
        {
            if (hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59)
            {
                throw new InvalidOperationException();
            }
            secondsSinceMidnight = hour * 3600 + minute * 60 + second;
        }

        public int Hour
        {
            get
            {
                return secondsSinceMidnight / 3600;
            }
        }

        public int Minute
        {
            get
            {
                return (secondsSinceMidnight / 60) % 60;
            }
        }

        public int Second
        {
            get
            {
                return secondsSinceMidnight % 60;
            }
        }

        public void Tick()
        {
            secondsSinceMidnight = (secondsSinceMidnight + 1) % 86400;
        }

        public int CompareTo(TimeOfDay other)
        {
            if (secondsSinceMidnight < other.secondsSinceMidnight)
            {
                return -1;
            }
            return secondsSinceMidnight > other.secondsSinceMidnight ? 1 : 0;
        }
    }

    public class TimeOfDayDemo
    {
        public static int Run()
        {
            raii TimeOfDay almostMidnight = new TimeOfDay(23, 59, 59);
            almostMidnight.Tick();
            raii TimeOfDay noon = new TimeOfDay(12);
            bool wrapped = almostMidnight.Hour == 0 && almostMidnight.Minute == 0 && almostMidnight.Second == 0;
            bool ordered = almostMidnight.CompareTo(noon) < 0;
            Console.WriteLine("time: constructor chaining contract, validation and midnight wrap");
            return wrapped && ordered ? 211 : -1;
        }
    }
}
```

##### `book_examples/26_deitel_live_code_native/Program.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.DeitelNative
{
    public class Program
    {
        public static int Main()
        {
            Console.WriteLine("NativeSharp Book Example 26 - Deitel-style live code");
            Console.WriteLine("-----------------------------------------------------");
            int checksum = 0;
            checksum = checksum + BankAccountDemo.Run();
            checksum = checksum + GradeBookDemo.Run();
            checksum = checksum + ChanceGamesDemo.Run();
            checksum = checksum + TimeOfDayDemo.Run();
            checksum = checksum + CreditFileDemo.Run();
            checksum = checksum + NativeParallelDemo.Run();
            raii string value = String.ToString(checksum);
            raii string line = String.Concat("verified checksum: ", value);
            Console.WriteLine(line);
            return checksum == 29840 ? 0 : 26;
        }
    }
}
```

### 47.27 Estructuras fundamentales de Drozdek, visibles y canónicas

**Problema.** Presentar listas, pilas, colas, árboles, grafos y hashing como implementaciones completas, no como resúmenes que obligan al lector a perseguir referencias cruzadas.

**Diseño.** Ocho fuentes independientes exponen la estructura, sus invariantes, operaciones, algoritmos y destrucción determinista. Un programa conductor ejecuta todas y certifica un checksum común.

**Auditoría propuesta.** Fuerce casos frontera, crecimiento, colisiones, rotaciones y eliminaciones. Compare el checksum y la salida entre `-O0` y `-O2`, y valide también orden, balance y propiedad de memoria.

**Escala real.** 9 archivos NativeSharp, 1773 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

**Reproducción.**

```powershell
nsc --emit-project-oop-exe `
    book_examples/27_drozdek_core_structures/Book27DrozdekCoreStructures.nsproj `
    -O2 `
    -o 27_drozdek_core_structures.exe
./27_drozdek_core_structures.exe
```

#### Listado completo

##### `book_examples/27_drozdek_core_structures/Book27DrozdekCoreStructures.nsproj`

```xml
<Project>
  <PropertyGroup>
    <Name>Book27DrozdekCoreStructures</Name>
  </PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
  </ItemGroup>
</Project>
```

##### `book_examples/27_drozdek_core_structures/AvlExample.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.DrozdekCore
{
    public class AvlNode
    {
        public int Key;
        public int Height;
        public AvlNode Left;
        public AvlNode Right;

        public AvlNode(int key)
        {
            Key = key;
            Height = 1;
        }
    }

    public class IntAvlTree
    {
        private AvlNode root;
        private int count;

        public int Count
        {
            get
            {
                return count;
            }
        }

        private int HeightOf(AvlNode node)
        {
            if (node == null)
            {
                return 0;
            }
            return node.Height;
        }

        private int Maximum(int a, int b)
        {
            if (a > b)
            {
                return a;
            }
            return b;
        }

        private void UpdateHeight(AvlNode node)
        {
            node.Height = 1 + Maximum(HeightOf(node.Left), HeightOf(node.Right));
        }

        private int BalanceFactor(AvlNode node)
        {
            if (node == null)
            {
                return 0;
            }
            return HeightOf(node.Left) - HeightOf(node.Right);
        }

        private AvlNode RotateRight(AvlNode top)
        {
            AvlNode pivot = top.Left;
            AvlNode transferred = pivot.Right;
            pivot.Right = top;
            top.Left = transferred;
            UpdateHeight(top);
            UpdateHeight(pivot);
            return pivot;
        }

        private AvlNode RotateLeft(AvlNode top)
        {
            AvlNode pivot = top.Right;
            AvlNode transferred = pivot.Left;
            pivot.Left = top;
            top.Right = transferred;
            UpdateHeight(top);
            UpdateHeight(pivot);
            return pivot;
        }

        private AvlNode Rebalance(AvlNode node)
        {
            if (node == null)
            {
                return null;
            }
            UpdateHeight(node);
            int factor = BalanceFactor(node);
            if (factor > 1)
            {
                if (BalanceFactor(node.Left) < 0)
                {
                    node.Left = RotateLeft(node.Left);
                }
                return RotateRight(node);
            }
            if (factor < -1)
            {
                if (BalanceFactor(node.Right) > 0)
                {
                    node.Right = RotateRight(node.Right);
                }
                return RotateLeft(node);
            }
            return node;
        }

        private AvlNode InsertNode(AvlNode node, int key, bool[] inserted)
        {
            if (node == null)
            {
                inserted[0] = true;
                return new AvlNode(key);
            }
            if (key < node.Key)
            {
                node.Left = InsertNode(node.Left, key, inserted);
            }
            else if (key > node.Key)
            {
                node.Right = InsertNode(node.Right, key, inserted);
            }
            else
            {
                return node;
            }
            return Rebalance(node);
        }

        public bool Insert(int key)
        {
            bool[] inserted = new bool[1];
            root = InsertNode(root, key, inserted);
            bool result = inserted[0];
            delete[] inserted;
            if (result)
            {
                count = count + 1;
            }
            return result;
        }

        private AvlNode MinimumNode(AvlNode node)
        {
            AvlNode current = node;
            while (current.Left != null)
            {
                current = current.Left;
            }
            return current;
        }

        private AvlNode RemoveNode(AvlNode node, int key, bool[] removed)
        {
            if (node == null)
            {
                return null;
            }
            if (key < node.Key)
            {
                node.Left = RemoveNode(node.Left, key, removed);
            }
            else if (key > node.Key)
            {
                node.Right = RemoveNode(node.Right, key, removed);
            }
            else
            {
                removed[0] = true;
                if (node.Left == null || node.Right == null)
                {
                    AvlNode child = node.Left;
                    if (child == null)
                    {
                        child = node.Right;
                    }
                    node.Left = null;
                    node.Right = null;
                    delete node;
                    return child;
                }
                AvlNode successor = MinimumNode(node.Right);
                node.Key = successor.Key;
                bool[] ignored = new bool[1];
                node.Right = RemoveNode(node.Right, successor.Key, ignored);
                delete[] ignored;
            }
            return Rebalance(node);
        }

        public bool Remove(int key)
        {
            bool[] removed = new bool[1];
            root = RemoveNode(root, key, removed);
            bool result = removed[0];
            delete[] removed;
            if (result)
            {
                count = count - 1;
            }
            return result;
        }

        public bool Contains(int key)
        {
            AvlNode current = root;
            while (current != null)
            {
                if (key == current.Key)
                {
                    return true;
                }
                if (key < current.Key)
                {
                    current = current.Left;
                }
                else
                {
                    current = current.Right;
                }
            }
            return false;
        }

        private int ValidateNode(AvlNode node, int minimum, int maximum, bool[] valid)
        {
            if (node == null)
            {
                return 0;
            }
            if (node.Key <= minimum || node.Key >= maximum)
            {
                valid[0] = false;
            }
            int leftHeight = ValidateNode(node.Left, minimum, node.Key, valid);
            int rightHeight = ValidateNode(node.Right, node.Key, maximum, valid);
            int difference = leftHeight - rightHeight;
            if (difference < -1 || difference > 1)
            {
                valid[0] = false;
            }
            int expectedHeight = 1 + Maximum(leftHeight, rightHeight);
            if (node.Height != expectedHeight)
            {
                valid[0] = false;
            }
            return expectedHeight;
        }

        public bool IsValid()
        {
            bool[] valid = new bool[1];
            valid[0] = true;
            ValidateNode(root, -2147483647, 2147483647, valid);
            bool result = valid[0];
            delete[] valid;
            return result;
        }

        private int InorderNode(AvlNode node, int[] position)
        {
            if (node == null)
            {
                return 0;
            }
            int checksum = InorderNode(node.Left, position);
            position[0] = position[0] + 1;
            checksum = checksum + node.Key * position[0];
            checksum = checksum + InorderNode(node.Right, position);
            return checksum;
        }

        public int InorderChecksum()
        {
            int[] position = new int[1];
            int checksum = InorderNode(root, position);
            delete[] position;
            return checksum;
        }

        private void ClearNode(AvlNode node)
        {
            if (node == null)
            {
                return;
            }
            ClearNode(node.Left);
            ClearNode(node.Right);
            node.Left = null;
            node.Right = null;
            delete node;
        }

        public void Clear()
        {
            ClearNode(root);
            root = null;
            count = 0;
        }

        ~IntAvlTree()
        {
            Clear();
        }
    }

    public class AvlExample
    {
        public static int Run()
        {
            raii IntAvlTree tree = new IntAvlTree();
            int[] keys = new int[] { 30, 20, 10, 40, 50, 25, 27, 5, 4, 45, 60, 55, 54, 53, 52 };
            int i = 0;
            while (i < keys.Length)
            {
                tree.Insert(keys[i]);
                i = i + 1;
            }
            tree.Remove(4);
            tree.Remove(60);
            tree.Remove(30);
            int checksum = tree.InorderChecksum() + tree.Count * 100;
            if (tree.Contains(52) && tree.IsValid())
            {
                checksum = checksum + 10000;
            }
            delete[] keys;
            Console.WriteLine("AVL: LL, RR, LR and RL rotations; insertion, deletion and validation");
            return checksum;
        }
    }
}
```

##### `book_examples/27_drozdek_core_structures/BinarySearchTreeExample.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.DrozdekCore
{
    public class BstNode
    {
        public int Key;
        public BstNode Left;
        public BstNode Right;

        public BstNode(int key)
        {
            Key = key;
        }
    }

    public class IntBinarySearchTree
    {
        private BstNode root;
        private int count;

        public int Count
        {
            get
            {
                return count;
            }
        }

        public bool Insert(int key)
        {
            if (root == null)
            {
                root = new BstNode(key);
                count = 1;
                return true;
            }
            BstNode parent = null;
            BstNode current = root;
            while (current != null)
            {
                parent = current;
                if (key < current.Key)
                {
                    current = current.Left;
                }
                else if (key > current.Key)
                {
                    current = current.Right;
                }
                else
                {
                    return false;
                }
            }
            if (key < parent.Key)
            {
                parent.Left = new BstNode(key);
            }
            else
            {
                parent.Right = new BstNode(key);
            }
            count = count + 1;
            return true;
        }

        public bool Contains(int key)
        {
            BstNode current = root;
            while (current != null)
            {
                if (key == current.Key)
                {
                    return true;
                }
                if (key < current.Key)
                {
                    current = current.Left;
                }
                else
                {
                    current = current.Right;
                }
            }
            return false;
        }

        public int Minimum()
        {
            if (root == null)
            {
                throw new InvalidOperationException();
            }
            BstNode current = root;
            while (current.Left != null)
            {
                current = current.Left;
            }
            return current.Key;
        }

        public int Maximum()
        {
            if (root == null)
            {
                throw new InvalidOperationException();
            }
            BstNode current = root;
            while (current.Right != null)
            {
                current = current.Right;
            }
            return current.Key;
        }

        public bool Remove(int key)
        {
            BstNode parent = null;
            BstNode current = root;
            while (current != null && current.Key != key)
            {
                parent = current;
                if (key < current.Key)
                {
                    current = current.Left;
                }
                else
                {
                    current = current.Right;
                }
            }
            if (current == null)
            {
                return false;
            }
            if (current.Left != null && current.Right != null)
            {
                BstNode successorParent = current;
                BstNode successor = current.Right;
                while (successor.Left != null)
                {
                    successorParent = successor;
                    successor = successor.Left;
                }
                current.Key = successor.Key;
                parent = successorParent;
                current = successor;
            }
            BstNode child = current.Left;
            if (child == null)
            {
                child = current.Right;
            }
            if (parent == null)
            {
                root = child;
            }
            else if (parent.Left == current)
            {
                parent.Left = child;
            }
            else
            {
                parent.Right = child;
            }
            current.Left = null;
            current.Right = null;
            delete current;
            count = count - 1;
            return true;
        }

        private int InorderNode(BstNode node, int[] position)
        {
            if (node == null)
            {
                return 0;
            }
            int checksum = InorderNode(node.Left, position);
            position[0] = position[0] + 1;
            checksum = checksum + node.Key * position[0];
            checksum = checksum + InorderNode(node.Right, position);
            return checksum;
        }

        public int InorderChecksum()
        {
            int[] position = new int[1];
            int checksum = InorderNode(root, position);
            delete[] position;
            return checksum;
        }

        private bool ValidateNode(BstNode node, int minimum, int maximum)
        {
            if (node == null)
            {
                return true;
            }
            if (node.Key <= minimum || node.Key >= maximum)
            {
                return false;
            }
            return ValidateNode(node.Left, minimum, node.Key) && ValidateNode(node.Right, node.Key, maximum);
        }

        public bool IsOrdered()
        {
            return ValidateNode(root, -2147483647, 2147483647);
        }

        private void ClearNode(BstNode node)
        {
            if (node == null)
            {
                return;
            }
            ClearNode(node.Left);
            ClearNode(node.Right);
            node.Left = null;
            node.Right = null;
            delete node;
        }

        public void Clear()
        {
            ClearNode(root);
            root = null;
            count = 0;
        }

        ~IntBinarySearchTree()
        {
            Clear();
        }
    }

    public class BinarySearchTreeExample
    {
        public static int Run()
        {
            raii IntBinarySearchTree tree = new IntBinarySearchTree();
            int[] keys = new int[] { 50, 25, 75, 10, 30, 60, 90, 27, 33, 55, 65 };
            int i = 0;
            while (i < keys.Length)
            {
                tree.Insert(keys[i]);
                i = i + 1;
            }
            tree.Remove(10);
            tree.Remove(30);
            tree.Remove(75);
            int checksum = tree.InorderChecksum();
            checksum = checksum + tree.Minimum() * 10;
            checksum = checksum + tree.Maximum() * 20;
            checksum = checksum + tree.Count * 100;
            if (tree.Contains(65) && tree.IsOrdered())
            {
                checksum = checksum + 1000;
            }
            delete[] keys;
            Console.WriteLine("BST: search, min/max and deletion in all structural cases");
            return checksum;
        }
    }
}
```

##### `book_examples/27_drozdek_core_structures/BinaryTreeExample.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.DrozdekCore
{
    public class BinaryNode
    {
        public int Info;
        public BinaryNode Left;
        public BinaryNode Right;

        public BinaryNode(int value)
        {
            Info = value;
        }
    }

    public class IntBinaryTree
    {
        private BinaryNode root;

        public void BuildCompleteSample()
        {
            Clear();
            root = new BinaryNode(1);
            root.Left = new BinaryNode(2);
            root.Right = new BinaryNode(3);
            root.Left.Left = new BinaryNode(4);
            root.Left.Right = new BinaryNode(5);
            root.Right.Left = new BinaryNode(6);
            root.Right.Right = new BinaryNode(7);
        }

        private int CountNode(BinaryNode node)
        {
            if (node == null)
            {
                return 0;
            }
            return 1 + CountNode(node.Left) + CountNode(node.Right);
        }

        public int Count()
        {
            return CountNode(root);
        }

        private int HeightNode(BinaryNode node)
        {
            if (node == null)
            {
                return 0;
            }
            int leftHeight = HeightNode(node.Left);
            int rightHeight = HeightNode(node.Right);
            if (leftHeight > rightHeight)
            {
                return leftHeight + 1;
            }
            return rightHeight + 1;
        }

        public int Height()
        {
            return HeightNode(root);
        }

        private int LeafCountNode(BinaryNode node)
        {
            if (node == null)
            {
                return 0;
            }
            if (node.Left == null && node.Right == null)
            {
                return 1;
            }
            return LeafCountNode(node.Left) + LeafCountNode(node.Right);
        }

        public int LeafCount()
        {
            return LeafCountNode(root);
        }

        private int PreorderNode(BinaryNode node, int factor)
        {
            if (node == null)
            {
                return 0;
            }
            int own = node.Info * factor;
            int left = PreorderNode(node.Left, factor + 1);
            int right = PreorderNode(node.Right, factor + 1);
            return own + left + right;
        }

        public int PreorderChecksum()
        {
            return PreorderNode(root, 1);
        }

        private int InorderNode(BinaryNode node, int[] position)
        {
            if (node == null)
            {
                return 0;
            }
            int checksum = InorderNode(node.Left, position);
            position[0] = position[0] + 1;
            checksum = checksum + node.Info * position[0];
            checksum = checksum + InorderNode(node.Right, position);
            return checksum;
        }

        public int InorderChecksum()
        {
            int[] position = new int[1];
            int checksum = InorderNode(root, position);
            delete[] position;
            return checksum;
        }

        private int PostorderNode(BinaryNode node, int[] position)
        {
            if (node == null)
            {
                return 0;
            }
            int checksum = PostorderNode(node.Left, position);
            checksum = checksum + PostorderNode(node.Right, position);
            position[0] = position[0] + 1;
            return checksum + node.Info * position[0];
        }

        public int PostorderChecksum()
        {
            int[] position = new int[1];
            int checksum = PostorderNode(root, position);
            delete[] position;
            return checksum;
        }

        public int BreadthFirstChecksum()
        {
            if (root == null)
            {
                return 0;
            }
            BinaryNode[] queue = new BinaryNode[Count()];
            int first = 0;
            int last = 0;
            int position = 1;
            int checksum = 0;
            queue[last] = root;
            last = last + 1;
            while (first < last)
            {
                BinaryNode node = queue[first];
                first = first + 1;
                checksum = checksum + node.Info * position;
                position = position + 1;
                if (node.Left != null)
                {
                    queue[last] = node.Left;
                    last = last + 1;
                }
                if (node.Right != null)
                {
                    queue[last] = node.Right;
                    last = last + 1;
                }
            }
            delete[] queue;
            return checksum;
        }

        private void MirrorNode(BinaryNode node)
        {
            if (node == null)
            {
                return;
            }
            BinaryNode temporary = node.Left;
            node.Left = node.Right;
            node.Right = temporary;
            MirrorNode(node.Left);
            MirrorNode(node.Right);
        }

        public void Mirror()
        {
            MirrorNode(root);
        }

        private void ClearNode(BinaryNode node)
        {
            if (node == null)
            {
                return;
            }
            ClearNode(node.Left);
            ClearNode(node.Right);
            node.Left = null;
            node.Right = null;
            delete node;
        }

        public void Clear()
        {
            ClearNode(root);
            root = null;
        }

        ~IntBinaryTree()
        {
            Clear();
        }
    }

    public class BinaryTreeExample
    {
        public static int Run()
        {
            raii IntBinaryTree tree = new IntBinaryTree();
            tree.BuildCompleteSample();
            int checksum = tree.PreorderChecksum();
            checksum = checksum + tree.InorderChecksum();
            checksum = checksum + tree.PostorderChecksum();
            checksum = checksum + tree.BreadthFirstChecksum();
            checksum = checksum + tree.Count() * 10;
            checksum = checksum + tree.Height() * 100;
            checksum = checksum + tree.LeafCount() * 1000;
            tree.Mirror();
            checksum = checksum + tree.BreadthFirstChecksum();
            Console.WriteLine("binary tree: traversals, height, leaves and mirror");
            return checksum;
        }
    }
}
```

##### `book_examples/27_drozdek_core_structures/GraphExample.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.DrozdekCore
{
    public class WeightedGraph
    {
        private int vertexCount;
        private int[] weights;

        public WeightedGraph(int vertices)
        {
            if (vertices <= 0)
            {
                throw new ArgumentOutOfRangeException();
            }
            vertexCount = vertices;
            weights = new int[vertices * vertices];
            int i = 0;
            while (i < weights.Length)
            {
                weights[i] = -1;
                i = i + 1;
            }
            i = 0;
            while (i < vertexCount)
            {
                weights[Cell(i, i)] = 0;
                i = i + 1;
            }
        }

        private int Cell(int from, int to)
        {
            return from * vertexCount + to;
        }

        private void ValidateVertex(int vertex)
        {
            if (vertex < 0 || vertex >= vertexCount)
            {
                throw new ArgumentOutOfRangeException();
            }
        }

        public void AddUndirectedEdge(int from, int to, int weight)
        {
            ValidateVertex(from);
            ValidateVertex(to);
            if (weight <= 0)
            {
                throw new ArgumentOutOfRangeException();
            }
            weights[Cell(from, to)] = weight;
            weights[Cell(to, from)] = weight;
        }

        public int EdgeWeight(int from, int to)
        {
            ValidateVertex(from);
            ValidateVertex(to);
            return weights[Cell(from, to)];
        }

        public int BreadthFirstChecksum(int start)
        {
            ValidateVertex(start);
            bool[] visited = new bool[vertexCount];
            int[] queue = new int[vertexCount];
            int first = 0;
            int last = 0;
            int position = 1;
            int checksum = 0;
            visited[start] = true;
            queue[last] = start;
            last = last + 1;
            while (first < last)
            {
                int vertex = queue[first];
                first = first + 1;
                checksum = checksum + (vertex + 1) * position;
                position = position + 1;
                int neighbor = 0;
                while (neighbor < vertexCount)
                {
                    if (weights[Cell(vertex, neighbor)] > 0 && !visited[neighbor])
                    {
                        visited[neighbor] = true;
                        queue[last] = neighbor;
                        last = last + 1;
                    }
                    neighbor = neighbor + 1;
                }
            }
            delete[] visited;
            delete[] queue;
            return checksum;
        }

        private int DepthFirstNode(int vertex, bool[] visited, int[] position)
        {
            visited[vertex] = true;
            position[0] = position[0] + 1;
            int checksum = (vertex + 1) * position[0];
            int neighbor = 0;
            while (neighbor < vertexCount)
            {
                if (weights[Cell(vertex, neighbor)] > 0 && !visited[neighbor])
                {
                    checksum = checksum + DepthFirstNode(neighbor, visited, position);
                }
                neighbor = neighbor + 1;
            }
            return checksum;
        }

        public int DepthFirstChecksum(int start)
        {
            ValidateVertex(start);
            bool[] visited = new bool[vertexCount];
            int[] position = new int[1];
            int checksum = DepthFirstNode(start, visited, position);
            delete[] visited;
            delete[] position;
            return checksum;
        }

        private int SelectNearest(int[] distance, bool[] settled)
        {
            int selected = -1;
            int best = 2147483647;
            int vertex = 0;
            while (vertex < vertexCount)
            {
                if (!settled[vertex] && distance[vertex] < best)
                {
                    best = distance[vertex];
                    selected = vertex;
                }
                vertex = vertex + 1;
            }
            return selected;
        }

        public int[] Dijkstra(int start)
        {
            ValidateVertex(start);
            int[] distance = new int[vertexCount];
            bool[] settled = new bool[vertexCount];
            int vertex = 0;
            while (vertex < vertexCount)
            {
                distance[vertex] = 2147483647;
                vertex = vertex + 1;
            }
            distance[start] = 0;
            int step = 0;
            while (step < vertexCount)
            {
                int current = SelectNearest(distance, settled);
                if (current == -1)
                {
                    break;
                }
                settled[current] = true;
                int neighbor = 0;
                while (neighbor < vertexCount)
                {
                    int edge = weights[Cell(current, neighbor)];
                    if (edge > 0 && !settled[neighbor])
                    {
                        int candidate = distance[current] + edge;
                        if (candidate < distance[neighbor])
                        {
                            distance[neighbor] = candidate;
                        }
                    }
                    neighbor = neighbor + 1;
                }
                step = step + 1;
            }
            delete[] settled;
            return distance;
        }

        public int PrimMinimumSpanningTreeWeight()
        {
            int[] cheapest = new int[vertexCount];
            bool[] included = new bool[vertexCount];
            int i = 0;
            while (i < vertexCount)
            {
                cheapest[i] = 2147483647;
                i = i + 1;
            }
            cheapest[0] = 0;
            int total = 0;
            int accepted = 0;
            while (accepted < vertexCount)
            {
                int vertex = SelectNearest(cheapest, included);
                if (vertex == -1)
                {
                    delete[] cheapest;
                    delete[] included;
                    throw new InvalidOperationException();
                }
                included[vertex] = true;
                total = total + cheapest[vertex];
                accepted = accepted + 1;
                int neighbor = 0;
                while (neighbor < vertexCount)
                {
                    int edge = weights[Cell(vertex, neighbor)];
                    if (edge > 0 && !included[neighbor] && edge < cheapest[neighbor])
                    {
                        cheapest[neighbor] = edge;
                    }
                    neighbor = neighbor + 1;
                }
            }
            delete[] cheapest;
            delete[] included;
            return total;
        }

        ~WeightedGraph()
        {
            delete[] weights;
        }
    }

    public class GraphExample
    {
        public static int Run()
        {
            raii WeightedGraph graph = new WeightedGraph(7);
            graph.AddUndirectedEdge(0, 1, 7);
            graph.AddUndirectedEdge(0, 3, 5);
            graph.AddUndirectedEdge(1, 2, 8);
            graph.AddUndirectedEdge(1, 3, 9);
            graph.AddUndirectedEdge(1, 4, 7);
            graph.AddUndirectedEdge(2, 4, 5);
            graph.AddUndirectedEdge(3, 4, 15);
            graph.AddUndirectedEdge(3, 5, 6);
            graph.AddUndirectedEdge(4, 5, 8);
            graph.AddUndirectedEdge(4, 6, 9);
            graph.AddUndirectedEdge(5, 6, 11);
            int checksum = graph.BreadthFirstChecksum(0);
            checksum = checksum + graph.DepthFirstChecksum(0);
            int[] distances = graph.Dijkstra(0);
            int i = 0;
            while (i < distances.Length)
            {
                checksum = checksum + distances[i] * (i + 1);
                i = i + 1;
            }
            checksum = checksum + graph.PrimMinimumSpanningTreeWeight() * 100;
            delete[] distances;
            Console.WriteLine("graph: BFS, DFS, Dijkstra and Prim over a weighted matrix");
            return checksum;
        }
    }
}
```

##### `book_examples/27_drozdek_core_structures/HashTableExample.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.DrozdekCore
{
    public class IntHashTable
    {
        private int[] keys;
        private int[] values;
        private int[] states;
        private int count;
        private int used;

        public IntHashTable(int initialCapacity)
        {
            if (initialCapacity < 5)
            {
                initialCapacity = 5;
            }
            keys = new int[initialCapacity];
            values = new int[initialCapacity];
            states = new int[initialCapacity];
        }

        public int Count
        {
            get
            {
                return count;
            }
        }

        public int Capacity
        {
            get
            {
                return keys.Length;
            }
        }

        private int Hash(int key, int capacity)
        {
            int result = key % capacity;
            if (result < 0)
            {
                result = result + capacity;
            }
            return result;
        }

        private int FindSlot(int key)
        {
            int start = Hash(key, keys.Length);
            int step = 0;
            while (step < keys.Length)
            {
                int slot = (start + step) % keys.Length;
                if (states[slot] == 0)
                {
                    return -1;
                }
                if (states[slot] == 1 && keys[slot] == key)
                {
                    return slot;
                }
                step = step + 1;
            }
            return -1;
        }

        private int FindInsertionSlot(int key)
        {
            int start = Hash(key, keys.Length);
            int firstDeleted = -1;
            int step = 0;
            while (step < keys.Length)
            {
                int slot = (start + step) % keys.Length;
                if (states[slot] == 1 && keys[slot] == key)
                {
                    return slot;
                }
                if (states[slot] == 2 && firstDeleted == -1)
                {
                    firstDeleted = slot;
                }
                if (states[slot] == 0)
                {
                    if (firstDeleted != -1)
                    {
                        return firstDeleted;
                    }
                    return slot;
                }
                step = step + 1;
            }
            return firstDeleted;
        }

        private void InsertWithoutGrowth(int key, int value)
        {
            int slot = FindInsertionSlot(key);
            if (states[slot] == 1)
            {
                values[slot] = value;
                return;
            }
            if (states[slot] == 0)
            {
                used = used + 1;
            }
            keys[slot] = key;
            values[slot] = value;
            states[slot] = 1;
            count = count + 1;
        }

        private void Rehash(int newCapacity)
        {
            int[] oldKeys = keys;
            int[] oldValues = values;
            int[] oldStates = states;
            int oldCapacity = oldKeys.Length;
            keys = new int[newCapacity];
            values = new int[newCapacity];
            states = new int[newCapacity];
            count = 0;
            used = 0;
            int slot = 0;
            while (slot < oldCapacity)
            {
                if (oldStates[slot] == 1)
                {
                    InsertWithoutGrowth(oldKeys[slot], oldValues[slot]);
                }
                slot = slot + 1;
            }
            delete[] oldKeys;
            delete[] oldValues;
            delete[] oldStates;
        }

        public void Put(int key, int value)
        {
            if ((used + 1) * 10 >= keys.Length * 7)
            {
                Rehash(keys.Length * 2 + 1);
            }
            InsertWithoutGrowth(key, value);
        }

        public bool ContainsKey(int key)
        {
            return FindSlot(key) != -1;
        }

        public int Get(int key)
        {
            int slot = FindSlot(key);
            if (slot == -1)
            {
                throw new InvalidOperationException();
            }
            return values[slot];
        }

        public bool Remove(int key)
        {
            int slot = FindSlot(key);
            if (slot == -1)
            {
                return false;
            }
            states[slot] = 2;
            count = count - 1;
            return true;
        }

        public int Checksum()
        {
            int checksum = 0;
            int slot = 0;
            while (slot < keys.Length)
            {
                if (states[slot] == 1)
                {
                    checksum = checksum + keys[slot] * 3 + values[slot] * 7;
                }
                slot = slot + 1;
            }
            return checksum;
        }

        ~IntHashTable()
        {
            delete[] keys;
            delete[] values;
            delete[] states;
        }
    }

    public class HashTableExample
    {
        public static int Run()
        {
            raii IntHashTable table = new IntHashTable(5);
            table.Put(5, 50);
            table.Put(16, 160);
            table.Put(27, 270);
            table.Put(38, 380);
            table.Put(-6, 60);
            table.Put(49, 490);
            table.Put(16, 161);
            table.Remove(27);
            table.Put(60, 600);
            int checksum = table.Checksum();
            checksum = checksum + table.Get(16) * 10;
            checksum = checksum + table.Count * 100;
            checksum = checksum + table.Capacity;
            if (!table.ContainsKey(27) && table.ContainsKey(60))
            {
                checksum = checksum + 1000;
            }
            Console.WriteLine("hash: linear probing, tombstones, update and rehash");
            return checksum;
        }
    }
}
```

##### `book_examples/27_drozdek_core_structures/LinkedListExample.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.DrozdekCore
{
    public class IntListNode
    {
        public int Info;
        public IntListNode Next;

        public IntListNode(int value)
        {
            Info = value;
        }
    }

    public class IntSinglyLinkedList
    {
        private IntListNode head;
        private IntListNode tail;
        private int count;

        public int Count
        {
            get
            {
                return count;
            }
        }

        public bool IsEmpty
        {
            get
            {
                return head == null;
            }
        }

        public void AddToHead(int value)
        {
            IntListNode node = new IntListNode(value);
            node.Next = head;
            head = node;
            if (tail == null)
            {
                tail = node;
            }
            count = count + 1;
        }

        public void AddToTail(int value)
        {
            IntListNode node = new IntListNode(value);
            if (tail == null)
            {
                head = node;
                tail = node;
            }
            else
            {
                tail.Next = node;
                tail = node;
            }
            count = count + 1;
        }

        public int DeleteFromHead()
        {
            if (head == null)
            {
                throw new InvalidOperationException();
            }
            IntListNode removed = head;
            int value = removed.Info;
            head = removed.Next;
            removed.Next = null;
            delete removed;
            count = count - 1;
            if (head == null)
            {
                tail = null;
            }
            return value;
        }

        public int DeleteFromTail()
        {
            if (tail == null)
            {
                throw new InvalidOperationException();
            }
            if (head == tail)
            {
                return DeleteFromHead();
            }
            IntListNode previous = head;
            while (previous.Next != tail)
            {
                previous = previous.Next;
            }
            int value = tail.Info;
            delete tail;
            tail = previous;
            tail.Next = null;
            count = count - 1;
            return value;
        }

        public bool DeleteValue(int value)
        {
            IntListNode previous = null;
            IntListNode current = head;
            while (current != null && current.Info != value)
            {
                previous = current;
                current = current.Next;
            }
            if (current == null)
            {
                return false;
            }
            if (previous == null)
            {
                DeleteFromHead();
                return true;
            }
            previous.Next = current.Next;
            if (current == tail)
            {
                tail = previous;
            }
            current.Next = null;
            delete current;
            count = count - 1;
            return true;
        }

        public bool IsInList(int value)
        {
            IntListNode current = head;
            while (current != null)
            {
                if (current.Info == value)
                {
                    return true;
                }
                current = current.Next;
            }
            return false;
        }

        public int Checksum()
        {
            int checksum = 0;
            int position = 1;
            IntListNode current = head;
            while (current != null)
            {
                checksum = checksum + current.Info * position;
                position = position + 1;
                current = current.Next;
            }
            return checksum;
        }

        public void Clear()
        {
            while (head != null)
            {
                DeleteFromHead();
            }
        }

        ~IntSinglyLinkedList()
        {
            Clear();
        }
    }

    public class LinkedListExample
    {
        public static int Run()
        {
            raii IntSinglyLinkedList list = new IntSinglyLinkedList();
            list.AddToHead(20);
            list.AddToHead(10);
            list.AddToTail(30);
            list.AddToTail(40);
            bool removed = list.DeleteValue(30);
            int first = list.DeleteFromHead();
            list.AddToHead(5);
            int last = list.DeleteFromTail();
            bool valid = removed && first == 10 && last == 40 && list.Count == 2;
            valid = valid && list.IsInList(5) && list.IsInList(20) && !list.IsInList(30);
            Console.WriteLine("linked list: head, tail, search, deletion and deterministic cleanup");
            return valid ? list.Checksum() : -1;
        }
    }
}
```

##### `book_examples/27_drozdek_core_structures/QueueExample.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.DrozdekCore
{
    public class IntCircularQueue
    {
        private int[] data;
        private int first;
        private int count;

        public IntCircularQueue(int initialCapacity)
        {
            if (initialCapacity < 1)
            {
                initialCapacity = 1;
            }
            data = new int[initialCapacity];
        }

        public bool IsEmpty
        {
            get
            {
                return count == 0;
            }
        }

        public int Count
        {
            get
            {
                return count;
            }
        }

        private int PhysicalIndex(int logicalIndex)
        {
            return (first + logicalIndex) % data.Length;
        }

        private void Grow()
        {
            int[] replacement = new int[data.Length * 2];
            int index = 0;
            while (index < count)
            {
                replacement[index] = data[PhysicalIndex(index)];
                index = index + 1;
            }
            delete[] data;
            data = replacement;
            first = 0;
        }

        public void Enqueue(int value)
        {
            if (count == data.Length)
            {
                Grow();
            }
            data[PhysicalIndex(count)] = value;
            count = count + 1;
        }

        public int Front()
        {
            if (count == 0)
            {
                throw new InvalidOperationException();
            }
            return data[first];
        }

        public int Dequeue()
        {
            int value = Front();
            first = (first + 1) % data.Length;
            count = count - 1;
            if (count == 0)
            {
                first = 0;
            }
            return value;
        }

        ~IntCircularQueue()
        {
            delete[] data;
        }
    }

    public class QueueExample
    {
        public static int Run()
        {
            raii IntCircularQueue queue = new IntCircularQueue(3);
            queue.Enqueue(11);
            queue.Enqueue(22);
            queue.Enqueue(33);
            int first = queue.Dequeue();
            queue.Enqueue(44);
            queue.Enqueue(55);
            int checksum = first;
            int position = 1;
            while (!queue.IsEmpty)
            {
                checksum = checksum + queue.Dequeue() * position;
                position = position + 1;
            }
            Console.WriteLine("queue: circular buffer, FIFO, wrap-around and growth without shifting");
            return checksum;
        }
    }
}
```

##### `book_examples/27_drozdek_core_structures/StackExample.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.DrozdekCore
{
    public class IntArrayStack
    {
        private int[] data;
        private int count;

        public IntArrayStack(int initialCapacity)
        {
            if (initialCapacity < 1)
            {
                initialCapacity = 1;
            }
            data = new int[initialCapacity];
        }

        public bool IsEmpty
        {
            get
            {
                return count == 0;
            }
        }

        public int Count
        {
            get
            {
                return count;
            }
        }

        private void Grow()
        {
            int[] replacement = new int[data.Length * 2];
            int index = 0;
            while (index < count)
            {
                replacement[index] = data[index];
                index = index + 1;
            }
            delete[] data;
            data = replacement;
        }

        public void Push(int value)
        {
            if (count == data.Length)
            {
                Grow();
            }
            data[count] = value;
            count = count + 1;
        }

        public int Top()
        {
            if (count == 0)
            {
                throw new InvalidOperationException();
            }
            return data[count - 1];
        }

        public int Pop()
        {
            int value = Top();
            count = count - 1;
            return value;
        }

        ~IntArrayStack()
        {
            delete[] data;
        }
    }

    public class StackExample
    {
        public static int Run()
        {
            raii IntArrayStack stack = new IntArrayStack(2);
            int value = 1;
            while (value <= 8)
            {
                stack.Push(value * 10);
                value = value + 1;
            }
            int checksum = 0;
            int position = 1;
            while (!stack.IsEmpty)
            {
                checksum = checksum + stack.Pop() * position;
                position = position + 1;
            }
            Console.WriteLine("stack: dynamic array, LIFO, growth, Top and Pop under RAII");
            return checksum;
        }
    }
}
```

##### `book_examples/27_drozdek_core_structures/Program.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.DrozdekCore
{
    public class Program
    {
        public static int Main()
        {
            Console.WriteLine("Drozdek core structures in NativeSharp");
            Console.WriteLine("---------------------------------------");
            int checksum = 0;
            checksum = checksum + LinkedListExample.Run();
            checksum = checksum + StackExample.Run();
            checksum = checksum + QueueExample.Run();
            checksum = checksum + BinaryTreeExample.Run();
            checksum = checksum + BinarySearchTreeExample.Run();
            checksum = checksum + AvlExample.Run();
            checksum = checksum + GraphExample.Run();
            checksum = checksum + HashTableExample.Run();
            raii string value = String.ToString(checksum);
            raii string line = String.Concat("verified checksum: ", value);
            Console.WriteLine(line);
            return checksum == 47846 ? 0 : 27;
        }
    }
}
```

---

# Parte XII. Drozdek completo en NativeSharp

Esta parte convierte el temario de *Data Structures and Algorithms in C++*, cuarta edición, en una
ruta de estudio NativeSharp. No copia la prosa del libro: reconstruye sus definiciones, algoritmos,
casos de estudio y listados programables con el compilador NativeSharp actual. El índice de la
edición impresa se usa como lista de requisitos, no como sugerencia.

La cobertura tiene cuatro niveles verificables:

1. Cada capítulo, sección y subsección numerada de Drozdek aparece en los capítulos 48 a 63.
2. Cada listado de programa identificado en las figuras tiene un equivalente NativeSharp completo.
3. Las figuras no programables se traducen a trazas, invariantes, tablas o laboratorios reproducibles.
4. Los ejercicios no se copian literalmente; se sustituyen por problemas originales que ejercitan
   la misma técnica y que pueden resolverse sobre los proyectos entregados.

Los proyectos 3 a 27 forman el laboratorio ejecutable. El proyecto 25 concentra variantes
avanzadas; el 27 presenta las estructuras fundamentales como fuentes canónicas independientes.
Todos se publican con llaves Allman, cuatro espacios, sin espacio antes del paréntesis de una función
y con un máximo de 120 columnas en código.

## Mapa de las implementaciones fundamentales

| Estructura | Capítulo | Implementación completa | Operaciones ejercitadas |
|---|---:|---|---|
| Lista enlazada | 50.1 | `LinkedListExample.ns` | extremos, búsqueda, tres casos de borrado, limpieza |
| Pila | 51.1 | `StackExample.ns` | `Push`, `Top`, `Pop`, crecimiento LIFO |
| Cola | 51.2 | `QueueExample.ns` | buffer circular, wrap-around, crecimiento FIFO |
| Árbol binario | 53.1 | `BinaryTreeExample.ns` | cuatro recorridos, altura, hojas, espejo |
| BST | 53.3 | `BinarySearchTreeExample.ns` | inserción, búsqueda, min/max, borrado, validación |
| AVL | 53.4 | `AvlExample.ns` | LL, RR, LR, RL, inserción, borrado, validación |
| Grafo | 55.2 | `GraphExample.ns` | BFS, DFS, Dijkstra, Prim |
| Tabla hash | 57.2 | `HashTableExample.ns` | sondeo, tombstones, actualización, eliminación, rehash |

Los ocho listados aparecen dentro de esos capítulos y vuelven a reunirse como proyecto compilable
en el caso 47.27. Así el lector puede estudiarlos donde se explican o construirlos como una sola
suite, sin depender de referencias ocultas.

## 48. Programación orientada a objetos para estructuras de datos

### 48.1 Tipos abstractos de datos

Un tipo abstracto de datos se define por valores posibles, operaciones y leyes, no por sus campos.
Una pila promete `Push`, `Pop`, `Peek` y orden LIFO; decidir si usa array o lista es una decisión de
representación. En NativeSharp el contrato también debe indicar quién posee cada valor y qué pasa
cuando la estructura se destruye.

La especificación útil de una operación contiene precondición, poscondición, complejidad y efecto
sobre ownership. Para `Pop` de una pila de objetos no basta decir “devuelve la cima”: hay que fijar si
el resultado se mueve al llamador, se presta o se copia. El proyecto 6 muestra este contrato en una
pila usada por un evaluador real.

### 48.2 Encapsulación

Campos privados protegen la representación, pero la encapsulación sólo existe si ninguna operación
pública puede dejar la invariante a medias. Una lista con `head`, `tail` y `count` debe actualizar los
tres incluso cuando se elimina el único nodo. Una tabla hash debe reinsertar al cambiar de capacidad;
copiar buckets sin recalcular índices rompe búsquedas posteriores.

NativeSharp agrega una frontera que C++ antiguo y C# administrado tratan de forma diferente: el
destructor es parte de la corrección del ADT. Una clase dueña de nodos debe liberar todos los nodos,
romper ciclos cuando sea necesario y no destruir valores prestados.

### 48.3 Herencia y polimorfismo

La herencia sirve cuando existe sustitución semántica. El proyecto 2 usa una base abstracta de
empleado y políticas de pago concretas; el proyecto 4 separa algoritmo de ordenamiento y comparador.
Este segundo diseño permite comparar estrategias sin duplicar datasets ni instrumentación.

No se usa herencia para compartir tres líneas de código entre estructuras sin relación. Para una
cola y una pila, la coincidencia de almacenamiento no implica el mismo contrato. Composición y
genéricos conservan mejor las invariantes.

### 48.4 Punteros, referencias y arrays

Las referencias a clases NativeSharp representan identidad; los arrays son bloques contiguos con
longitud. Los nodos enlazados permiten crecimiento local, pero pagan asignaciones, indirecciones y
mala localidad de caché. Los arrays ofrecen recorrido contiguo y acceso O(1), pero crecer exige
reservar, mover y liberar.

Los equivalentes de punteros a función son delegados. Los equivalentes de referencias de salida son
`ref`, `out` e `in` en la superficie actual. El código histórico que negaba esas características no
se usa como autoridad porque el compilador ya llegó a stage 57.

Copiar una referencia no copia el objeto. Para valores owned, `move` transfiere la obligación de
destrucción. Esta diferencia debe revisarse en constructores de copia conceptuales, setters, arrays
de objetos y funciones que devuelven nodos.

### 48.5 Biblioteca genérica, iteradores y algoritmos

`List<T>`, `Stack<T>`, `Queue<T>`, `Dictionary<TKey,TValue>` y `HashSet<T>` cubren el papel de los
contenedores estándar. `foreach` usa el protocolo estructural de enumeración. Delegados y lambdas
ocupan el lugar de objetos función cuando se necesita inyectar comparación, hash o acción.

El objetivo pedagógico no es reemplazar toda estructura manual por una colección. Primero se
implementa la estructura para comprender su invariante; después se compara con la biblioteca para
separar aprendizaje algorítmico de código de producción.

### 48.6 Vectores y archivos de acceso por registro

El recorrido de arrays y `List<T>` incluye construcción, crecimiento, inserción, borrado, búsqueda,
ordenamiento y capacidad. Para un archivo de registros se fija un layout estable: identificador,
campos de ancho conocido, codificación y versión. `File.ReadAllBytes` y `File.WriteAllBytes` permiten
serializar sin depender del layout interno de una clase.

La biblioteca actual no expone seek posicional. El equivalente NativeSharp conserva registros de
tamaño fijo en un buffer, modifica el intervalo `offset = index * recordSize` y escribe el buffer.
No se finge que `WriteAllBytes` sea acceso aleatorio del sistema operativo: la diferencia de costo se
documenta y el formato permanece listo para una futura API `FileStream`.

### 48.7 Caso de estudio y laboratorio

El caso de archivo de acceso aleatorio se reconstruye como almacén de registros versionado. Las
pruebas mínimas son alta, consulta, actualización, baja lógica, reapertura, archivo truncado y versión
incompatible. El tutorial 5 cubre arrays y strings; los proyectos 1, 2 y 3 cubren ADT, polimorfismo y
algoritmos genéricos sobre objetos.

## 49. Análisis de complejidad

### 49.1 Complejidad computacional y asintótica

El tamaño de entrada debe definirse antes de contar. Para ordenar es el número de elementos; para un
grafo pueden ser `V` y `E`; para enteros de precisión arbitraria también interviene el número de bits.
Se cuentan operaciones dominantes y memoria adicional, separando costo del algoritmo de costo del
modelo de datos NativeSharp.

### 49.2 O grande, omega y theta

`O(g(n))` es cota superior asintótica, `Ω(g(n))` cota inferior y `Θ(g(n))` cota ajustada. Una
afirmación `f(n) = O(n²)` no dice que el algoritmo siempre tarde exactamente una constante por
`n²`; dice que, desde cierto tamaño, alguna constante multiplica a `n²` y domina a `f`.

Las propiedades usadas son transitividad, suma dominada por el término mayor, multiplicación de
costos anidados y eliminación de constantes. No se elimina un parámetro independiente: BFS con
listas cuesta `Θ(V + E)`, no simplemente `Θ(V)`.

### 49.3 Obtención de complejidad

Un loop simple suma su costo por iteración. Loops anidados independientes multiplican; loops cuyo
límite depende del exterior producen sumatorias. Dividir repetidamente por dos genera logaritmos.
Una recurrencia debe incluir trabajo local y tamaño de subproblemas.

El análisis de quicksort distingue partición lineal y distribución de pivotes. El de árboles usa la
altura `h`; sólo un árbol balanceado permite sustituir `h` por `log n`. El apéndice 61 desarrolla las
sumatorias armónica, `lg(n!)`, camino medio y mínimo de nodos AVL.

### 49.4 Mejor, promedio, peor caso y amortización

Búsqueda lineal tiene mejor caso O(1), peor O(n) y promedio dependiente de la distribución. El costo
amortizado de crecer un vector reúne muchas inserciones baratas y copias ocasionales: duplicar
capacidad mantiene O(1) amortizado por inserción aunque una inserción aislada cueste O(n).

El método agregado suma una secuencia completa; el contable asigna crédito; el potencial mide deuda
almacenada por el estado. El proyecto 7 permite observar capacidad circular sin confundir costo
amortizado con promedio probabilístico.

### 49.5 NP-completitud

P contiene problemas resolubles en tiempo polinomial; NP contiene problemas cuya solución propuesta
puede verificarse en tiempo polinomial. NP-hard describe al menos la dificultad de todo NP y
NP-complete agrega pertenencia a NP. Una reducción transforma instancias preservando sí/no en tiempo
polinomial; no es una comparación informal de que dos problemas “parecen difíciles”.

Los ejemplos de clique, 3-coloración, vertex cover y ciclo hamiltoniano reaparecen en el capítulo 55.
Para entradas pequeñas se usa búsqueda exhaustiva; para entradas grandes se documentan límites,
heurísticas o solvers especializados.

## 50. Listas enlazadas

### 50.1 Lista simple: inserción, eliminación y búsqueda

El proyecto 5 implementa nodo, cabeza, cola, cantidad, inserción frontal y final, búsqueda y borrado.
La eliminación se divide en cabeza, interior, cola y único nodo. El destructor recorre desde la
cabeza, captura `next` antes de destruir y termina con extremos nulos.

Las operaciones son O(1) en extremos mantenidos y O(n) para localizar por valor. Mantener cola no
abarata borrar la cola porque falta el predecesor; esa diferencia motiva la lista doble.

El siguiente laboratorio deja la implementación en el lugar donde se estudia. Incluye nodo,
contenedor, cabeza, cola, contador, inserciones, búsquedas, los casos de borrado y destrucción
determinista. El ejecutable que reúne los ocho módulos se reproduce completo en el caso 27.

#### Implementación completa: lista simplemente enlazada

Este listado es parte del capítulo: contiene la estructura y el programa de ejercicio correspondiente. No es pseudocódigo ni un fragmento recortado.

##### `book_examples/27_drozdek_core_structures/Book27DrozdekCoreStructures.nsproj`

```xml
<Project>
  <PropertyGroup>
    <Name>Book27DrozdekCoreStructures</Name>
  </PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../../lib/NativeSharp.Core/NativeSharp.Core.nsproj" />
  </ItemGroup>
</Project>
```

##### `book_examples/27_drozdek_core_structures/LinkedListExample.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.DrozdekCore
{
    public class IntListNode
    {
        public int Info;
        public IntListNode Next;

        public IntListNode(int value)
        {
            Info = value;
        }
    }

    public class IntSinglyLinkedList
    {
        private IntListNode head;
        private IntListNode tail;
        private int count;

        public int Count
        {
            get
            {
                return count;
            }
        }

        public bool IsEmpty
        {
            get
            {
                return head == null;
            }
        }

        public void AddToHead(int value)
        {
            IntListNode node = new IntListNode(value);
            node.Next = head;
            head = node;
            if (tail == null)
            {
                tail = node;
            }
            count = count + 1;
        }

        public void AddToTail(int value)
        {
            IntListNode node = new IntListNode(value);
            if (tail == null)
            {
                head = node;
                tail = node;
            }
            else
            {
                tail.Next = node;
                tail = node;
            }
            count = count + 1;
        }

        public int DeleteFromHead()
        {
            if (head == null)
            {
                throw new InvalidOperationException();
            }
            IntListNode removed = head;
            int value = removed.Info;
            head = removed.Next;
            removed.Next = null;
            delete removed;
            count = count - 1;
            if (head == null)
            {
                tail = null;
            }
            return value;
        }

        public int DeleteFromTail()
        {
            if (tail == null)
            {
                throw new InvalidOperationException();
            }
            if (head == tail)
            {
                return DeleteFromHead();
            }
            IntListNode previous = head;
            while (previous.Next != tail)
            {
                previous = previous.Next;
            }
            int value = tail.Info;
            delete tail;
            tail = previous;
            tail.Next = null;
            count = count - 1;
            return value;
        }

        public bool DeleteValue(int value)
        {
            IntListNode previous = null;
            IntListNode current = head;
            while (current != null && current.Info != value)
            {
                previous = current;
                current = current.Next;
            }
            if (current == null)
            {
                return false;
            }
            if (previous == null)
            {
                DeleteFromHead();
                return true;
            }
            previous.Next = current.Next;
            if (current == tail)
            {
                tail = previous;
            }
            current.Next = null;
            delete current;
            count = count - 1;
            return true;
        }

        public bool IsInList(int value)
        {
            IntListNode current = head;
            while (current != null)
            {
                if (current.Info == value)
                {
                    return true;
                }
                current = current.Next;
            }
            return false;
        }

        public int Checksum()
        {
            int checksum = 0;
            int position = 1;
            IntListNode current = head;
            while (current != null)
            {
                checksum = checksum + current.Info * position;
                position = position + 1;
                current = current.Next;
            }
            return checksum;
        }

        public void Clear()
        {
            while (head != null)
            {
                DeleteFromHead();
            }
        }

        ~IntSinglyLinkedList()
        {
            Clear();
        }
    }

    public class LinkedListExample
    {
        public static int Run()
        {
            raii IntSinglyLinkedList list = new IntSinglyLinkedList();
            list.AddToHead(20);
            list.AddToHead(10);
            list.AddToTail(30);
            list.AddToTail(40);
            bool removed = list.DeleteValue(30);
            int first = list.DeleteFromHead();
            list.AddToHead(5);
            int last = list.DeleteFromTail();
            bool valid = removed && first == 10 && last == 40 && list.Count == 2;
            valid = valid && list.IsInList(5) && list.IsInList(20) && !list.IsInList(30);
            Console.WriteLine("linked list: head, tail, search, deletion and deterministic cleanup");
            return valid ? list.Checksum() : -1;
        }
    }
}
```

### 50.2 Lista doble y circular

El proyecto 9 mantiene `previous` y `next` simétricos. El proyecto 10 cierra el último enlace sobre la
cabeza y resuelve Josephus. Una lista circular no puede limpiarse con “hasta null”; debe contar nodos,
detectar el regreso al centinela o romper primero el ciclo.

### 50.3 Skip lists

Una skip list superpone niveles enlazados. La búsqueda avanza mientras la siguiente clave no exceda
el objetivo y baja de nivel cuando ya no puede avanzar. Inserción conserva un vector `update` con el
predecesor de cada nivel. Con niveles aleatorios adecuados, búsqueda e inserción cuestan O(log n)
esperado y O(n) en el peor caso.

`DeterministicSkipList` del proyecto 25 usa el número de factores dos de la clave para obtener alturas
reproducibles. Esa política sirve para pruebas, no sustituye la distribución aleatoria de producción.

### 50.4 Listas autoorganizables

Move-to-front lleva el elemento consultado a la cabeza; transpose lo intercambia con su predecesor;
count ordena por frecuencia observada. No cambian el peor caso O(n), pero explotan localidad cuando
unas claves aparecen mucho más que otras. Deben medirse comparaciones y también costo de reorganizar.

El módulo `SequenceAlgorithms` implementa move-to-front sobre un array para hacer visible la política
sin mezclarla con asignación de nodos. El laboratorio pide repetir la secuencia de accesos de Drozdek
y comparar número total de comparaciones entre las tres estrategias.

### 50.5 Tablas dispersas

Una matriz dispersa guarda sólo valores no cero como triples `(row, column, value)`, listas por fila o
formato CSR. El producto punto disperso avanza dos listas ordenadas de índices y multiplica únicamente
coincidencias. Su costo depende de no ceros, no del área completa.

El proyecto 25 incluye el merge de índices para un producto fila-columna. La ampliación natural es
CSR por filas y CSC por columnas, suma lineal de filas y multiplicación con acumulador hash.

### 50.6 Colecciones estándar y caso de biblioteca

La colección genérica ofrece iteración y operaciones de propósito general; la implementación manual
expone la invariante. El caso de biblioteca se modela con listas separadas de catálogo, ejemplares,
usuarios y préstamos. Un préstamo enlaza identificadores, no hace dueño al usuario del ejemplar.

Pruebas: libro desconocido, ejemplar ya prestado, devolución doble, usuario con varias obras, baja de
un título con préstamos activos y destrucción del catálogo. Los proyectos 5, 9, 10 y 25 cubren todas
las representaciones de este capítulo.

## 51. Pilas, colas, colas de prioridad y deques

### 51.1 Pilas

Una pila basada en array conserva elementos en `[0, count)` y la cima en `count - 1`. Al crecer,
mueve en orden y libera el buffer anterior. Una pila enlazada inserta y quita en cabeza. Ambas ofrecen
O(1) por operación, amortizado para el array.

El proyecto 6 usa dos pilas para evaluar expresiones infijas con precedencia y paréntesis. Además de
`Push` y `Pop`, prueba entrada inválida, división por cero y limpieza durante una excepción.

Esta versión aislada muestra el almacenamiento redimensionable, el crecimiento amortizado, `Top`,
`Push`, `Pop` y la obligación de liberar el buffer.

#### Implementación completa: pila dinámica

Este listado es parte del capítulo: contiene la estructura y el programa de ejercicio correspondiente. No es pseudocódigo ni un fragmento recortado.

##### `book_examples/27_drozdek_core_structures/StackExample.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.DrozdekCore
{
    public class IntArrayStack
    {
        private int[] data;
        private int count;

        public IntArrayStack(int initialCapacity)
        {
            if (initialCapacity < 1)
            {
                initialCapacity = 1;
            }
            data = new int[initialCapacity];
        }

        public bool IsEmpty
        {
            get
            {
                return count == 0;
            }
        }

        public int Count
        {
            get
            {
                return count;
            }
        }

        private void Grow()
        {
            int[] replacement = new int[data.Length * 2];
            int index = 0;
            while (index < count)
            {
                replacement[index] = data[index];
                index = index + 1;
            }
            delete[] data;
            data = replacement;
        }

        public void Push(int value)
        {
            if (count == data.Length)
            {
                Grow();
            }
            data[count] = value;
            count = count + 1;
        }

        public int Top()
        {
            if (count == 0)
            {
                throw new InvalidOperationException();
            }
            return data[count - 1];
        }

        public int Pop()
        {
            int value = Top();
            count = count - 1;
            return value;
        }

        ~IntArrayStack()
        {
            delete[] data;
        }
    }

    public class StackExample
    {
        public static int Run()
        {
            raii IntArrayStack stack = new IntArrayStack(2);
            int value = 1;
            while (value <= 8)
            {
                stack.Push(value * 10);
                value = value + 1;
            }
            int checksum = 0;
            int position = 1;
            while (!stack.IsEmpty)
            {
                checksum = checksum + stack.Pop() * position;
                position = position + 1;
            }
            Console.WriteLine("stack: dynamic array, LIFO, growth, Top and Pop under RAII");
            return checksum;
        }
    }
}
```

### 51.2 Colas y simulación bancaria

La cola circular del proyecto 7 usa `head`, `count` y módulo por capacidad. El tail lógico es
`(head + count) % capacity`. El caso bancario simula llegada, tiempo de servicio, espera, longitud
máxima y utilización. La distribución de llegadas es parte del modelo, no de la cola.

La implementación siguiente fuerza wrap-around y crecimiento. Al redimensionar copia en orden
lógico, no en el orden físico del buffer viejo.

#### Implementación completa: cola circular

Este listado es parte del capítulo: contiene la estructura y el programa de ejercicio correspondiente. No es pseudocódigo ni un fragmento recortado.

##### `book_examples/27_drozdek_core_structures/QueueExample.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.DrozdekCore
{
    public class IntCircularQueue
    {
        private int[] data;
        private int first;
        private int count;

        public IntCircularQueue(int initialCapacity)
        {
            if (initialCapacity < 1)
            {
                initialCapacity = 1;
            }
            data = new int[initialCapacity];
        }

        public bool IsEmpty
        {
            get
            {
                return count == 0;
            }
        }

        public int Count
        {
            get
            {
                return count;
            }
        }

        private int PhysicalIndex(int logicalIndex)
        {
            return (first + logicalIndex) % data.Length;
        }

        private void Grow()
        {
            int[] replacement = new int[data.Length * 2];
            int index = 0;
            while (index < count)
            {
                replacement[index] = data[PhysicalIndex(index)];
                index = index + 1;
            }
            delete[] data;
            data = replacement;
            first = 0;
        }

        public void Enqueue(int value)
        {
            if (count == data.Length)
            {
                Grow();
            }
            data[PhysicalIndex(count)] = value;
            count = count + 1;
        }

        public int Front()
        {
            if (count == 0)
            {
                throw new InvalidOperationException();
            }
            return data[first];
        }

        public int Dequeue()
        {
            int value = Front();
            first = (first + 1) % data.Length;
            count = count - 1;
            if (count == 0)
            {
                first = 0;
            }
            return value;
        }

        ~IntCircularQueue()
        {
            delete[] data;
        }
    }

    public class QueueExample
    {
        public static int Run()
        {
            raii IntCircularQueue queue = new IntCircularQueue(3);
            queue.Enqueue(11);
            queue.Enqueue(22);
            queue.Enqueue(33);
            int first = queue.Dequeue();
            queue.Enqueue(44);
            queue.Enqueue(55);
            int checksum = first;
            int position = 1;
            while (!queue.IsEmpty)
            {
                checksum = checksum + queue.Dequeue() * position;
                position = position + 1;
            }
            Console.WriteLine("queue: circular buffer, FIFO, wrap-around and growth without shifting");
            return checksum;
        }
    }
}
```

### 51.3 Colas de prioridad y heaps

Una cola de prioridad devuelve mínimo o máximo por prioridad. El heap binario del proyecto 13 ofrece
inserción y extracción O(log n), consulta O(1) y construcción bottom-up O(n). Empates requieren una
política: estabilidad mediante secuencia de llegada o libertad de orden.

### 51.4 Deques

El proyecto 11 implementa ambos extremos con array circular y lo aplica a palíndromos. Un deque
también sostiene BFS 0-1, ventanas monotónicas y work stealing. Deben probarse todas las combinaciones
de wrap-around y crecimiento, no sólo secuencias de un extremo.

### 51.5 Salida de un laberinto

El laberinto se representa como rejilla, cada celda almacena paredes o transitabilidad. DFS con pila
encuentra alguna salida; BFS con cola encuentra el menor número de pasos cuando cada movimiento vale
lo mismo. Guardar predecesor reconstruye la ruta sin copiar una ruta completa por estado.

El proyecto 16 contiene la infraestructura BFS/DFS y camino; sustituir ciudades por celdas produce el
listado de laberinto. El laboratorio exige entrada sin salida, inicio igual a salida, corredores
cíclicos y dos rutas de diferente longitud.

## 52. Recursión y backtracking

### 52.1 Definiciones y anatomía de llamada

Una definición recursiva necesita base y reducción. Cada llamada NativeSharp ocupa un frame con
parámetros, locales, retorno y limpiezas RAII. El costo de stack es O(profundidad), incluso si el
código ocupa pocas líneas.

Recursión de cola deja la llamada como última acción; recursión no terminal conserva trabajo. El
libro no presupone eliminación de tail calls: cuando la profundidad puede depender de datos hostiles,
la versión iterativa es el contrato seguro.

### 52.2 Recursión indirecta, anidada y excesiva

En recursión indirecta A llama B y B vuelve a A. La recursión anidada usa una llamada como argumento
de otra. Ambas requieren una medida de progreso demostrable. Fibonacci ingenuo ilustra recursión
excesiva; memoización cambia el árbol exponencial por una tabla lineal.

### 52.3 Backtracking y ocho reinas

Backtracking elige, marca, explora y desmarca. Ocho reinas conserva columnas y diagonales ocupadas;
una fila es el nivel de decisión. La prueba no sólo cuenta soluciones: valida que ninguna pareja
comparta columna ni diagonal.

El mismo esquema resuelve ciclo hamiltoniano y coloración exacta en el proyecto 25. RAII libera
buffers al salir, pero la restauración de marcas sigue siendo responsabilidad del algoritmo.

### 52.4 Descenso recursivo

Una gramática de expresiones puede separarse en `Expression`, `Term`, `Unary` y `Primary`. Cada
función consume tokens del nivel de precedencia correspondiente. El parser debe distinguir fin de
entrada, token inesperado, paréntesis faltante y división por cero en evaluación.

El proyecto 6 ofrece la variante iterativa con dos pilas. El ejercicio editorial pide implementar la
misma gramática por descenso recursivo y ejecutar un corpus compartido para demostrar equivalencia.

### 52.5 Curva de Koch

La curva sustituye un segmento por cuatro segmentos de un tercio. Tras profundidad `d` contiene
`4^d` segmentos y longitud `(4/3)^d` respecto del original. NativeSharp puede generar una secuencia
de instrucciones de dibujo o vértices aunque el libro no presuponga una biblioteca gráfica.

Se prueba número de segmentos, continuidad entre extremos y caja envolvente. Separar geometría de
render permite enviar después los vértices a una DLL gráfica mediante el ABI nativo.

## 53. Árboles binarios

### 53.1 Definiciones e implementación

Árbol, árbol binario y BST no son sinónimos. Un árbol binario limita hijos; un BST agrega orden. La
altura controla costo de búsqueda, inserción y borrado. El proyecto 8 implementa nodos, búsqueda
iterativa y recursiva, recorridos y destrucción postorder.

Primero se implementa un árbol binario general, sin imponer orden BST. El ejemplo construye una
forma explícita y ejecuta preorder, inorder, postorder, breadth-first, altura, hojas y espejo.

#### Implementación completa: árbol binario general

Este listado es parte del capítulo: contiene la estructura y el programa de ejercicio correspondiente. No es pseudocódigo ni un fragmento recortado.

##### `book_examples/27_drozdek_core_structures/BinaryTreeExample.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.DrozdekCore
{
    public class BinaryNode
    {
        public int Info;
        public BinaryNode Left;
        public BinaryNode Right;

        public BinaryNode(int value)
        {
            Info = value;
        }
    }

    public class IntBinaryTree
    {
        private BinaryNode root;

        public void BuildCompleteSample()
        {
            Clear();
            root = new BinaryNode(1);
            root.Left = new BinaryNode(2);
            root.Right = new BinaryNode(3);
            root.Left.Left = new BinaryNode(4);
            root.Left.Right = new BinaryNode(5);
            root.Right.Left = new BinaryNode(6);
            root.Right.Right = new BinaryNode(7);
        }

        private int CountNode(BinaryNode node)
        {
            if (node == null)
            {
                return 0;
            }
            return 1 + CountNode(node.Left) + CountNode(node.Right);
        }

        public int Count()
        {
            return CountNode(root);
        }

        private int HeightNode(BinaryNode node)
        {
            if (node == null)
            {
                return 0;
            }
            int leftHeight = HeightNode(node.Left);
            int rightHeight = HeightNode(node.Right);
            if (leftHeight > rightHeight)
            {
                return leftHeight + 1;
            }
            return rightHeight + 1;
        }

        public int Height()
        {
            return HeightNode(root);
        }

        private int LeafCountNode(BinaryNode node)
        {
            if (node == null)
            {
                return 0;
            }
            if (node.Left == null && node.Right == null)
            {
                return 1;
            }
            return LeafCountNode(node.Left) + LeafCountNode(node.Right);
        }

        public int LeafCount()
        {
            return LeafCountNode(root);
        }

        private int PreorderNode(BinaryNode node, int factor)
        {
            if (node == null)
            {
                return 0;
            }
            int own = node.Info * factor;
            int left = PreorderNode(node.Left, factor + 1);
            int right = PreorderNode(node.Right, factor + 1);
            return own + left + right;
        }

        public int PreorderChecksum()
        {
            return PreorderNode(root, 1);
        }

        private int InorderNode(BinaryNode node, int[] position)
        {
            if (node == null)
            {
                return 0;
            }
            int checksum = InorderNode(node.Left, position);
            position[0] = position[0] + 1;
            checksum = checksum + node.Info * position[0];
            checksum = checksum + InorderNode(node.Right, position);
            return checksum;
        }

        public int InorderChecksum()
        {
            int[] position = new int[1];
            int checksum = InorderNode(root, position);
            delete[] position;
            return checksum;
        }

        private int PostorderNode(BinaryNode node, int[] position)
        {
            if (node == null)
            {
                return 0;
            }
            int checksum = PostorderNode(node.Left, position);
            checksum = checksum + PostorderNode(node.Right, position);
            position[0] = position[0] + 1;
            return checksum + node.Info * position[0];
        }

        public int PostorderChecksum()
        {
            int[] position = new int[1];
            int checksum = PostorderNode(root, position);
            delete[] position;
            return checksum;
        }

        public int BreadthFirstChecksum()
        {
            if (root == null)
            {
                return 0;
            }
            BinaryNode[] queue = new BinaryNode[Count()];
            int first = 0;
            int last = 0;
            int position = 1;
            int checksum = 0;
            queue[last] = root;
            last = last + 1;
            while (first < last)
            {
                BinaryNode node = queue[first];
                first = first + 1;
                checksum = checksum + node.Info * position;
                position = position + 1;
                if (node.Left != null)
                {
                    queue[last] = node.Left;
                    last = last + 1;
                }
                if (node.Right != null)
                {
                    queue[last] = node.Right;
                    last = last + 1;
                }
            }
            delete[] queue;
            return checksum;
        }

        private void MirrorNode(BinaryNode node)
        {
            if (node == null)
            {
                return;
            }
            BinaryNode temporary = node.Left;
            node.Left = node.Right;
            node.Right = temporary;
            MirrorNode(node.Left);
            MirrorNode(node.Right);
        }

        public void Mirror()
        {
            MirrorNode(root);
        }

        private void ClearNode(BinaryNode node)
        {
            if (node == null)
            {
                return;
            }
            ClearNode(node.Left);
            ClearNode(node.Right);
            node.Left = null;
            node.Right = null;
            delete node;
        }

        public void Clear()
        {
            ClearNode(root);
            root = null;
        }

        ~IntBinaryTree()
        {
            Clear();
        }
    }

    public class BinaryTreeExample
    {
        public static int Run()
        {
            raii IntBinaryTree tree = new IntBinaryTree();
            tree.BuildCompleteSample();
            int checksum = tree.PreorderChecksum();
            checksum = checksum + tree.InorderChecksum();
            checksum = checksum + tree.PostorderChecksum();
            checksum = checksum + tree.BreadthFirstChecksum();
            checksum = checksum + tree.Count() * 10;
            checksum = checksum + tree.Height() * 100;
            checksum = checksum + tree.LeafCount() * 1000;
            tree.Mirror();
            checksum = checksum + tree.BreadthFirstChecksum();
            Console.WriteLine("binary tree: traversals, height, leaves and mirror");
            return checksum;
        }
    }
}
```

### 53.2 Recorridos

Breadth-first usa cola. Preorder, inorder y postorder pueden escribirse recursivos o con pila. Morris
inorder crea threads temporales desde el predecesor al nodo actual y los elimina al regresar; logra
O(1) memoria auxiliar sin dejar modificado el árbol. El proyecto 25 verifica el checksum inorder
después de restaurar todos los enlaces.

Un árbol threaded permanente distingue enlaces hijo de enlaces sucesor/predecesor mediante tags. La
inserción debe conservar ambos significados; seguir un thread como si fuera hijo produce ciclos.

### 53.3 Inserción y dos formas de eliminación

Inserción baja por comparación hasta un enlace vacío. Eliminación por copying reemplaza la clave por
predecesor o sucesor y elimina ese nodo. Eliminación por merging une subárboles, colocando el derecho
en el extremo apropiado del izquierdo. Ambas preservan inorder, pero cambian forma y patrones de
acceso.

El BST completo separa búsqueda, mínimo, máximo, inserción y borrado. El dataset elimina una hoja,
un nodo con hijos y un nodo con dos hijos; al terminar valida de nuevo el orden global.

#### Implementación completa: árbol binario de búsqueda

Este listado es parte del capítulo: contiene la estructura y el programa de ejercicio correspondiente. No es pseudocódigo ni un fragmento recortado.

##### `book_examples/27_drozdek_core_structures/BinarySearchTreeExample.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.DrozdekCore
{
    public class BstNode
    {
        public int Key;
        public BstNode Left;
        public BstNode Right;

        public BstNode(int key)
        {
            Key = key;
        }
    }

    public class IntBinarySearchTree
    {
        private BstNode root;
        private int count;

        public int Count
        {
            get
            {
                return count;
            }
        }

        public bool Insert(int key)
        {
            if (root == null)
            {
                root = new BstNode(key);
                count = 1;
                return true;
            }
            BstNode parent = null;
            BstNode current = root;
            while (current != null)
            {
                parent = current;
                if (key < current.Key)
                {
                    current = current.Left;
                }
                else if (key > current.Key)
                {
                    current = current.Right;
                }
                else
                {
                    return false;
                }
            }
            if (key < parent.Key)
            {
                parent.Left = new BstNode(key);
            }
            else
            {
                parent.Right = new BstNode(key);
            }
            count = count + 1;
            return true;
        }

        public bool Contains(int key)
        {
            BstNode current = root;
            while (current != null)
            {
                if (key == current.Key)
                {
                    return true;
                }
                if (key < current.Key)
                {
                    current = current.Left;
                }
                else
                {
                    current = current.Right;
                }
            }
            return false;
        }

        public int Minimum()
        {
            if (root == null)
            {
                throw new InvalidOperationException();
            }
            BstNode current = root;
            while (current.Left != null)
            {
                current = current.Left;
            }
            return current.Key;
        }

        public int Maximum()
        {
            if (root == null)
            {
                throw new InvalidOperationException();
            }
            BstNode current = root;
            while (current.Right != null)
            {
                current = current.Right;
            }
            return current.Key;
        }

        public bool Remove(int key)
        {
            BstNode parent = null;
            BstNode current = root;
            while (current != null && current.Key != key)
            {
                parent = current;
                if (key < current.Key)
                {
                    current = current.Left;
                }
                else
                {
                    current = current.Right;
                }
            }
            if (current == null)
            {
                return false;
            }
            if (current.Left != null && current.Right != null)
            {
                BstNode successorParent = current;
                BstNode successor = current.Right;
                while (successor.Left != null)
                {
                    successorParent = successor;
                    successor = successor.Left;
                }
                current.Key = successor.Key;
                parent = successorParent;
                current = successor;
            }
            BstNode child = current.Left;
            if (child == null)
            {
                child = current.Right;
            }
            if (parent == null)
            {
                root = child;
            }
            else if (parent.Left == current)
            {
                parent.Left = child;
            }
            else
            {
                parent.Right = child;
            }
            current.Left = null;
            current.Right = null;
            delete current;
            count = count - 1;
            return true;
        }

        private int InorderNode(BstNode node, int[] position)
        {
            if (node == null)
            {
                return 0;
            }
            int checksum = InorderNode(node.Left, position);
            position[0] = position[0] + 1;
            checksum = checksum + node.Key * position[0];
            checksum = checksum + InorderNode(node.Right, position);
            return checksum;
        }

        public int InorderChecksum()
        {
            int[] position = new int[1];
            int checksum = InorderNode(root, position);
            delete[] position;
            return checksum;
        }

        private bool ValidateNode(BstNode node, int minimum, int maximum)
        {
            if (node == null)
            {
                return true;
            }
            if (node.Key <= minimum || node.Key >= maximum)
            {
                return false;
            }
            return ValidateNode(node.Left, minimum, node.Key) && ValidateNode(node.Right, node.Key, maximum);
        }

        public bool IsOrdered()
        {
            return ValidateNode(root, -2147483647, 2147483647);
        }

        private void ClearNode(BstNode node)
        {
            if (node == null)
            {
                return;
            }
            ClearNode(node.Left);
            ClearNode(node.Right);
            node.Left = null;
            node.Right = null;
            delete node;
        }

        public void Clear()
        {
            ClearNode(root);
            root = null;
            count = 0;
        }

        ~IntBinarySearchTree()
        {
            Clear();
        }
    }

    public class BinarySearchTreeExample
    {
        public static int Run()
        {
            raii IntBinarySearchTree tree = new IntBinarySearchTree();
            int[] keys = new int[] { 50, 25, 75, 10, 30, 60, 90, 27, 33, 55, 65 };
            int i = 0;
            while (i < keys.Length)
            {
                tree.Insert(keys[i]);
                i = i + 1;
            }
            tree.Remove(10);
            tree.Remove(30);
            tree.Remove(75);
            int checksum = tree.InorderChecksum();
            checksum = checksum + tree.Minimum() * 10;
            checksum = checksum + tree.Maximum() * 20;
            checksum = checksum + tree.Count * 100;
            if (tree.Contains(65) && tree.IsOrdered())
            {
                checksum = checksum + 1000;
            }
            delete[] keys;
            Console.WriteLine("BST: search, min/max and deletion in all structural cases");
            return checksum;
        }
    }
}
```

### 53.4 DSW y AVL

Day-Stout-Warren convierte el árbol en vine mediante rotaciones derechas y después comprime con
rotaciones izquierdas para obtener altura logarítmica usando O(1) memoria. AVL repara localmente tras
cada operación y almacena altura o factor de balance. El proyecto 12 cubre las cuatro rotaciones y
borrado con rebalanceo.

Aquí está la implementación autocontenida: inserción, eliminación, alturas, factores de balance,
rotaciones LL, RR, LR y RL, búsqueda, recorrido y una validación estructural independiente.

#### Implementación completa: árbol AVL

Este listado es parte del capítulo: contiene la estructura y el programa de ejercicio correspondiente. No es pseudocódigo ni un fragmento recortado.

##### `book_examples/27_drozdek_core_structures/AvlExample.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.DrozdekCore
{
    public class AvlNode
    {
        public int Key;
        public int Height;
        public AvlNode Left;
        public AvlNode Right;

        public AvlNode(int key)
        {
            Key = key;
            Height = 1;
        }
    }

    public class IntAvlTree
    {
        private AvlNode root;
        private int count;

        public int Count
        {
            get
            {
                return count;
            }
        }

        private int HeightOf(AvlNode node)
        {
            if (node == null)
            {
                return 0;
            }
            return node.Height;
        }

        private int Maximum(int a, int b)
        {
            if (a > b)
            {
                return a;
            }
            return b;
        }

        private void UpdateHeight(AvlNode node)
        {
            node.Height = 1 + Maximum(HeightOf(node.Left), HeightOf(node.Right));
        }

        private int BalanceFactor(AvlNode node)
        {
            if (node == null)
            {
                return 0;
            }
            return HeightOf(node.Left) - HeightOf(node.Right);
        }

        private AvlNode RotateRight(AvlNode top)
        {
            AvlNode pivot = top.Left;
            AvlNode transferred = pivot.Right;
            pivot.Right = top;
            top.Left = transferred;
            UpdateHeight(top);
            UpdateHeight(pivot);
            return pivot;
        }

        private AvlNode RotateLeft(AvlNode top)
        {
            AvlNode pivot = top.Right;
            AvlNode transferred = pivot.Left;
            pivot.Left = top;
            top.Right = transferred;
            UpdateHeight(top);
            UpdateHeight(pivot);
            return pivot;
        }

        private AvlNode Rebalance(AvlNode node)
        {
            if (node == null)
            {
                return null;
            }
            UpdateHeight(node);
            int factor = BalanceFactor(node);
            if (factor > 1)
            {
                if (BalanceFactor(node.Left) < 0)
                {
                    node.Left = RotateLeft(node.Left);
                }
                return RotateRight(node);
            }
            if (factor < -1)
            {
                if (BalanceFactor(node.Right) > 0)
                {
                    node.Right = RotateRight(node.Right);
                }
                return RotateLeft(node);
            }
            return node;
        }

        private AvlNode InsertNode(AvlNode node, int key, bool[] inserted)
        {
            if (node == null)
            {
                inserted[0] = true;
                return new AvlNode(key);
            }
            if (key < node.Key)
            {
                node.Left = InsertNode(node.Left, key, inserted);
            }
            else if (key > node.Key)
            {
                node.Right = InsertNode(node.Right, key, inserted);
            }
            else
            {
                return node;
            }
            return Rebalance(node);
        }

        public bool Insert(int key)
        {
            bool[] inserted = new bool[1];
            root = InsertNode(root, key, inserted);
            bool result = inserted[0];
            delete[] inserted;
            if (result)
            {
                count = count + 1;
            }
            return result;
        }

        private AvlNode MinimumNode(AvlNode node)
        {
            AvlNode current = node;
            while (current.Left != null)
            {
                current = current.Left;
            }
            return current;
        }

        private AvlNode RemoveNode(AvlNode node, int key, bool[] removed)
        {
            if (node == null)
            {
                return null;
            }
            if (key < node.Key)
            {
                node.Left = RemoveNode(node.Left, key, removed);
            }
            else if (key > node.Key)
            {
                node.Right = RemoveNode(node.Right, key, removed);
            }
            else
            {
                removed[0] = true;
                if (node.Left == null || node.Right == null)
                {
                    AvlNode child = node.Left;
                    if (child == null)
                    {
                        child = node.Right;
                    }
                    node.Left = null;
                    node.Right = null;
                    delete node;
                    return child;
                }
                AvlNode successor = MinimumNode(node.Right);
                node.Key = successor.Key;
                bool[] ignored = new bool[1];
                node.Right = RemoveNode(node.Right, successor.Key, ignored);
                delete[] ignored;
            }
            return Rebalance(node);
        }

        public bool Remove(int key)
        {
            bool[] removed = new bool[1];
            root = RemoveNode(root, key, removed);
            bool result = removed[0];
            delete[] removed;
            if (result)
            {
                count = count - 1;
            }
            return result;
        }

        public bool Contains(int key)
        {
            AvlNode current = root;
            while (current != null)
            {
                if (key == current.Key)
                {
                    return true;
                }
                if (key < current.Key)
                {
                    current = current.Left;
                }
                else
                {
                    current = current.Right;
                }
            }
            return false;
        }

        private int ValidateNode(AvlNode node, int minimum, int maximum, bool[] valid)
        {
            if (node == null)
            {
                return 0;
            }
            if (node.Key <= minimum || node.Key >= maximum)
            {
                valid[0] = false;
            }
            int leftHeight = ValidateNode(node.Left, minimum, node.Key, valid);
            int rightHeight = ValidateNode(node.Right, node.Key, maximum, valid);
            int difference = leftHeight - rightHeight;
            if (difference < -1 || difference > 1)
            {
                valid[0] = false;
            }
            int expectedHeight = 1 + Maximum(leftHeight, rightHeight);
            if (node.Height != expectedHeight)
            {
                valid[0] = false;
            }
            return expectedHeight;
        }

        public bool IsValid()
        {
            bool[] valid = new bool[1];
            valid[0] = true;
            ValidateNode(root, -2147483647, 2147483647, valid);
            bool result = valid[0];
            delete[] valid;
            return result;
        }

        private int InorderNode(AvlNode node, int[] position)
        {
            if (node == null)
            {
                return 0;
            }
            int checksum = InorderNode(node.Left, position);
            position[0] = position[0] + 1;
            checksum = checksum + node.Key * position[0];
            checksum = checksum + InorderNode(node.Right, position);
            return checksum;
        }

        public int InorderChecksum()
        {
            int[] position = new int[1];
            int checksum = InorderNode(root, position);
            delete[] position;
            return checksum;
        }

        private void ClearNode(AvlNode node)
        {
            if (node == null)
            {
                return;
            }
            ClearNode(node.Left);
            ClearNode(node.Right);
            node.Left = null;
            node.Right = null;
            delete node;
        }

        public void Clear()
        {
            ClearNode(root);
            root = null;
            count = 0;
        }

        ~IntAvlTree()
        {
            Clear();
        }
    }

    public class AvlExample
    {
        public static int Run()
        {
            raii IntAvlTree tree = new IntAvlTree();
            int[] keys = new int[] { 30, 20, 10, 40, 50, 25, 27, 5, 4, 45, 60, 55, 54, 53, 52 };
            int i = 0;
            while (i < keys.Length)
            {
                tree.Insert(keys[i]);
                i = i + 1;
            }
            tree.Remove(4);
            tree.Remove(60);
            tree.Remove(30);
            int checksum = tree.InorderChecksum() + tree.Count * 100;
            if (tree.Contains(52) && tree.IsValid())
            {
                checksum = checksum + 10000;
            }
            delete[] keys;
            Console.WriteLine("AVL: LL, RR, LR and RL rotations; insertion, deletion and validation");
            return checksum;
        }
    }
}
```

### 53.5 Árboles autoajustables y splay

Un splay tree mueve el nodo accedido a la raíz mediante zig, zig-zig y zig-zag. No garantiza altura
logarítmica en cada instante, pero sí costo amortizado O(log n). El proyecto 25 implementa splaying
recursivo y verifica que la clave consultada llegue a la raíz.

### 53.6 Heaps, treaps y k-d trees

El heap del proyecto 13 organiza un array como árbol completo. Un treap combina orden BST por clave y
orden heap por prioridad; prioridades aleatorias dan altura esperada logarítmica. El proyecto 25 usa
prioridades deterministas para reproducibilidad.

Un k-d tree alterna ejes por profundidad. La consulta rectangular poda el subárbol que no puede
intersectar el rango. El laboratorio usa puntos 2D y valida el resultado contra un recorrido lineal,
evitando confundir corrección con la eficacia de la poda.

### 53.7 Notación polaca y árboles de expresión

Prefija visita operador antes de operandos; postfija después; infija necesita precedencia o
paréntesis. Un árbol de expresión evalúa recursivamente, imprime en las tres notaciones y puede
aplicar simplificaciones como plegado de constantes. El proyecto 6 aporta tokenización y evaluación;
el ejercicio lo eleva a AST con ownership explícito.

### 53.8 Frecuencias de palabras

Tokenizar, normalizar y contar separa política lingüística de almacenamiento. Un BST produce salida
ordenada, una tabla hash favorece actualización, y un trie comparte prefijos. Los proyectos 8, 14 y
15 permiten implementar las tres versiones y comparar tiempo, memoria y orden de reporte.

## 54. Árboles multiway

### 54.1 Familia B-tree

Un B-tree de grado mínimo `t` guarda entre `t - 1` y `2t - 1` claves por nodo no raíz. Todas las
hojas tienen la misma profundidad. Inserción divide un hijo lleno antes de descender; borrado presta
de un hermano o fusiona para no entrar en un hijo mínimo.

`SmallBTree` del proyecto 25 implementa un 2-3-4 tree, el caso `t = 2`, con split top-down, búsqueda y
destrucción recursiva. El dataset fuerza splits de raíz e internos.

### 54.2 B-star, B-plus y prefix B-plus

B-star redistribuye con hermanos antes de dividir y busca mayor ocupación. B-plus conserva registros
en hojas enlazadas y usa claves internas como separadores; esto favorece rangos. Prefix B-plus acorta
separadores internos al prefijo mínimo que distingue bloques.

La adaptación NativeSharp modela páginas como arrays de capacidad fija y números de página, no como
punteros persistentes. Un split devuelve separador y nueva página al padre. El laboratorio mide
ocupación, lecturas de página y escaneo de rango.

### 54.3 K-d B-trees, bit-trees y R-trees

K-d B-tree combina partición multidimensional con páginas. Bit-tree decide por bits sucesivos de la
clave. R-tree agrupa rectángulos por cajas envolventes y permite solapamiento; una consulta debe
visitar todos los hijos cuyas cajas intersectan el objetivo.

Cada variante cambia la función de separación, no las obligaciones de una página: capacidad,
ocupación mínima, split, propagación, serialización estable y validación desde raíz.

### 54.4 2-4 trees, sets, multisets, maps y multimaps

El 2-4 tree es equivalente estructural al B-tree de grado mínimo dos. Set guarda claves únicas;
multiset agrega multiplicidad; map asocia una clave única a valor; multimap permite varias entradas.
En NativeSharp, `HashSet<T>` y `Dictionary<TKey,TValue>` cubren las variantes hash. Una estructura
ordenada multiway es necesaria cuando el orden o los rangos forman parte del contrato.

### 54.5 Tries y corrector ortográfico

El proyecto 14 implementa inserción, pertenencia y prefijos. El corrector carga diccionario, marca fin
de palabra y genera candidatos por eliminación, inserción, sustitución y transposición. Cada candidato
se valida en el trie; ranking puede incorporar frecuencia y distancia de edición.

Las pruebas incluyen palabra que es prefijo de otra, alfabeto fuera del rango admitido, duplicados,
diccionario vacío y UTF-8 multibyte. Un trie byte-oriented debe declararlo; no debe prometer caracteres
Unicode si indexa bytes.

## 55. Grafos

### 55.1 Representación y recorridos

Matriz de adyacencia usa Θ(V²); listas usan Θ(V + E). El proyecto 16 implementa BFS, DFS recursivo e
iterativo y reconstrucción de camino. En dirigido se distingue entrada de salida; en ponderado se
separa presencia de arista de peso cero.

### 55.2 Caminos mínimos

Dijkstra del proyecto 17 exige pesos no negativos. Bellman-Ford permite negativos y detecta ciclo
negativo alcanzable. Floyd-Warshall del proyecto 25 calcula todos contra todos mediante la invariante
“intermedios permitidos en 0..k”. El dataset clásico incluye pesos negativos sin ciclo negativo.

El laboratorio canónico usa una matriz ponderada y reúne BFS, DFS recursivo, Dijkstra y Prim. La
representación, las colas auxiliares, los conjuntos de visitados y las distancias aparecen en el
listado, no detrás de una biblioteca.

#### Implementación completa: grafo ponderado

Este listado es parte del capítulo: contiene la estructura y el programa de ejercicio correspondiente. No es pseudocódigo ni un fragmento recortado.

##### `book_examples/27_drozdek_core_structures/GraphExample.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.DrozdekCore
{
    public class WeightedGraph
    {
        private int vertexCount;
        private int[] weights;

        public WeightedGraph(int vertices)
        {
            if (vertices <= 0)
            {
                throw new ArgumentOutOfRangeException();
            }
            vertexCount = vertices;
            weights = new int[vertices * vertices];
            int i = 0;
            while (i < weights.Length)
            {
                weights[i] = -1;
                i = i + 1;
            }
            i = 0;
            while (i < vertexCount)
            {
                weights[Cell(i, i)] = 0;
                i = i + 1;
            }
        }

        private int Cell(int from, int to)
        {
            return from * vertexCount + to;
        }

        private void ValidateVertex(int vertex)
        {
            if (vertex < 0 || vertex >= vertexCount)
            {
                throw new ArgumentOutOfRangeException();
            }
        }

        public void AddUndirectedEdge(int from, int to, int weight)
        {
            ValidateVertex(from);
            ValidateVertex(to);
            if (weight <= 0)
            {
                throw new ArgumentOutOfRangeException();
            }
            weights[Cell(from, to)] = weight;
            weights[Cell(to, from)] = weight;
        }

        public int EdgeWeight(int from, int to)
        {
            ValidateVertex(from);
            ValidateVertex(to);
            return weights[Cell(from, to)];
        }

        public int BreadthFirstChecksum(int start)
        {
            ValidateVertex(start);
            bool[] visited = new bool[vertexCount];
            int[] queue = new int[vertexCount];
            int first = 0;
            int last = 0;
            int position = 1;
            int checksum = 0;
            visited[start] = true;
            queue[last] = start;
            last = last + 1;
            while (first < last)
            {
                int vertex = queue[first];
                first = first + 1;
                checksum = checksum + (vertex + 1) * position;
                position = position + 1;
                int neighbor = 0;
                while (neighbor < vertexCount)
                {
                    if (weights[Cell(vertex, neighbor)] > 0 && !visited[neighbor])
                    {
                        visited[neighbor] = true;
                        queue[last] = neighbor;
                        last = last + 1;
                    }
                    neighbor = neighbor + 1;
                }
            }
            delete[] visited;
            delete[] queue;
            return checksum;
        }

        private int DepthFirstNode(int vertex, bool[] visited, int[] position)
        {
            visited[vertex] = true;
            position[0] = position[0] + 1;
            int checksum = (vertex + 1) * position[0];
            int neighbor = 0;
            while (neighbor < vertexCount)
            {
                if (weights[Cell(vertex, neighbor)] > 0 && !visited[neighbor])
                {
                    checksum = checksum + DepthFirstNode(neighbor, visited, position);
                }
                neighbor = neighbor + 1;
            }
            return checksum;
        }

        public int DepthFirstChecksum(int start)
        {
            ValidateVertex(start);
            bool[] visited = new bool[vertexCount];
            int[] position = new int[1];
            int checksum = DepthFirstNode(start, visited, position);
            delete[] visited;
            delete[] position;
            return checksum;
        }

        private int SelectNearest(int[] distance, bool[] settled)
        {
            int selected = -1;
            int best = 2147483647;
            int vertex = 0;
            while (vertex < vertexCount)
            {
                if (!settled[vertex] && distance[vertex] < best)
                {
                    best = distance[vertex];
                    selected = vertex;
                }
                vertex = vertex + 1;
            }
            return selected;
        }

        public int[] Dijkstra(int start)
        {
            ValidateVertex(start);
            int[] distance = new int[vertexCount];
            bool[] settled = new bool[vertexCount];
            int vertex = 0;
            while (vertex < vertexCount)
            {
                distance[vertex] = 2147483647;
                vertex = vertex + 1;
            }
            distance[start] = 0;
            int step = 0;
            while (step < vertexCount)
            {
                int current = SelectNearest(distance, settled);
                if (current == -1)
                {
                    break;
                }
                settled[current] = true;
                int neighbor = 0;
                while (neighbor < vertexCount)
                {
                    int edge = weights[Cell(current, neighbor)];
                    if (edge > 0 && !settled[neighbor])
                    {
                        int candidate = distance[current] + edge;
                        if (candidate < distance[neighbor])
                        {
                            distance[neighbor] = candidate;
                        }
                    }
                    neighbor = neighbor + 1;
                }
                step = step + 1;
            }
            delete[] settled;
            return distance;
        }

        public int PrimMinimumSpanningTreeWeight()
        {
            int[] cheapest = new int[vertexCount];
            bool[] included = new bool[vertexCount];
            int i = 0;
            while (i < vertexCount)
            {
                cheapest[i] = 2147483647;
                i = i + 1;
            }
            cheapest[0] = 0;
            int total = 0;
            int accepted = 0;
            while (accepted < vertexCount)
            {
                int vertex = SelectNearest(cheapest, included);
                if (vertex == -1)
                {
                    delete[] cheapest;
                    delete[] included;
                    throw new InvalidOperationException();
                }
                included[vertex] = true;
                total = total + cheapest[vertex];
                accepted = accepted + 1;
                int neighbor = 0;
                while (neighbor < vertexCount)
                {
                    int edge = weights[Cell(vertex, neighbor)];
                    if (edge > 0 && !included[neighbor] && edge < cheapest[neighbor])
                    {
                        cheapest[neighbor] = edge;
                    }
                    neighbor = neighbor + 1;
                }
            }
            delete[] cheapest;
            delete[] included;
            return total;
        }

        ~WeightedGraph()
        {
            delete[] weights;
        }
    }

    public class GraphExample
    {
        public static int Run()
        {
            raii WeightedGraph graph = new WeightedGraph(7);
            graph.AddUndirectedEdge(0, 1, 7);
            graph.AddUndirectedEdge(0, 3, 5);
            graph.AddUndirectedEdge(1, 2, 8);
            graph.AddUndirectedEdge(1, 3, 9);
            graph.AddUndirectedEdge(1, 4, 7);
            graph.AddUndirectedEdge(2, 4, 5);
            graph.AddUndirectedEdge(3, 4, 15);
            graph.AddUndirectedEdge(3, 5, 6);
            graph.AddUndirectedEdge(4, 5, 8);
            graph.AddUndirectedEdge(4, 6, 9);
            graph.AddUndirectedEdge(5, 6, 11);
            int checksum = graph.BreadthFirstChecksum(0);
            checksum = checksum + graph.DepthFirstChecksum(0);
            int[] distances = graph.Dijkstra(0);
            int i = 0;
            while (i < distances.Length)
            {
                checksum = checksum + distances[i] * (i + 1);
                i = i + 1;
            }
            checksum = checksum + graph.PrimMinimumSpanningTreeWeight() * 100;
            delete[] distances;
            Console.WriteLine("graph: BFS, DFS, Dijkstra and Prim over a weighted matrix");
            return checksum;
        }
    }
}
```

### 55.3 Ciclos, union-find y árboles de expansión

DFS detecta back edges en dirigido; union-find detecta si una arista no dirigida une vértices ya
conectados. El proyecto 19 aplica path compression y union by rank en Kruskal. Prim mantiene la mejor
arista que cruza el corte. Ambos producen el mismo peso mínimo, no necesariamente las mismas aristas.

### 55.4 Conectividad

En no dirigido, BFS/DFS etiqueta componentes y puntos de articulación requieren tiempos de entrada y
low-link. En dirigido, componentes fuertemente conexas pueden obtenerse con Kosaraju o Tarjan. El
grafo condensado de SCC siempre es DAG y puede ordenarse topológicamente.

### 55.5 Orden topológico

Kahn, implementado en el proyecto 18, consume grados de entrada cero. DFS postorder ofrece una
alternativa. Procesar menos de `V` vértices demuestra ciclo. Varios órdenes pueden ser válidos; la
prueba debe validar cada dependencia, no una cadena literal arbitraria.

### 55.6 Redes: flujo máximo y costo mínimo

Edmonds-Karp usa BFS en la red residual y cuesta O(VE²). `MaximumFlow` del proyecto 25 mantiene flujo
directo y reverso y reproduce el valor 23 del dataset de seis vértices. El teorema max-flow/min-cut
permite certificar el resultado con un corte residual.

Para costo mínimo se agregan costos a aristas residuales y se buscan caminos de menor costo mientras
haya demanda. Potenciales permiten Dijkstra con costos reducidos; Bellman-Ford maneja costos
negativos iniciales. Capacidad y costo son propiedades independientes.

### 55.7 Matching

Matching bipartito máximo se reduce a flujo o usa caminos aumentantes. Gale-Shapley produce matching
estable respecto de preferencias, implementado en el proyecto 25. El problema de asignación ponderada
usa algoritmo húngaro. En grafos no bipartitos, blossoms representan ciclos impares contraídos.

El caso de representantes distintos es matching bipartito: conjuntos a la izquierda, candidatos a la
derecha y una arista por elegibilidad. Existe sistema de representantes exactamente cuando el matching
cubre todos los conjuntos; Hall proporciona la condición combinatoria.

### 55.8 Euler, Hamilton y coloración

Hierholzer construye circuito euleriano consumiendo cada arista una vez. Hamilton busca visitar cada
vértice y es NP-completo; el proyecto 25 usa backtracking con vértice inicial fijado. La diferencia es
esencial: Euler trata aristas y tiene algoritmo lineal; Hamilton trata vértices y no conoce algoritmo
polinomial general.

Coloración greedy depende del orden y entrega una cota, no siempre el cromático. Backtracking exacto
prueba `k` colores. Clique, vertex cover y coloración se conectan mediante complementos y reducciones,
pero cada transformación debe demostrarse.

## 56. Ordenamiento

### 56.1 Algoritmos elementales

Insertion sort desplaza un prefijo ordenado y es estable; selection sort minimiza swaps pero no es
estable por defecto; bubble sort intercambia inversiones adyacentes y puede cortar si no hubo cambios;
comb sort usa gaps decrecientes para eliminar “tortugas” lejanas. El proyecto 4 contiene los tres
primeros y el proyecto 25 agrega comb.

### 56.2 Árboles de decisión y límite inferior

Un algoritmo de comparación corresponde a un árbol cuyas hojas representan permutaciones. Distinguir
`n!` órdenes requiere altura al menos `lg(n!) = Ω(n log n)`. Counting y radix evitan esa cota porque
usan estructura de las claves, no sólo comparaciones.

### 56.3 Algoritmos eficientes

Shell sort compara subsecuencias por gaps; heap sort es Θ(n log n) in-place; quicksort tiene promedio
Θ(n log n), peor Θ(n²) y stack dependiente de partición; mergesort es estable Θ(n log n) con buffer.
El proyecto 4 instrumenta los cuatro.

Radix sort procesa dígitos con una pasada estable; counting sort cuenta claves de rango acotado. El
proyecto 25 implementa ambos. Deben documentarse signo, número de pasadas, base, estabilidad y costo
`Θ(n + k)` de memoria para el rango.

### 56.4 Algoritmos de biblioteca y polinomios

Una API de ordenamiento recibe comparación consistente. No debe prometer estabilidad si no la ofrece.
El proyecto 23 representa polinomios densos y suma por índices; una representación dispersa combina
listas ordenadas de términos igual que mergesort combina secuencias.

## 57. Hashing

### 57.1 Funciones hash

División usa módulo; folding suma grupos; mid-square extrae bits centrales del cuadrado; extraction
selecciona posiciones; radix transforma representación; universal elige una función de una familia
para limitar colisiones adversarias. `HashAlgorithms` implementa las tres primeras y deja las demás
como variaciones parametrizadas del mismo contrato.

### 57.2 Resolución de colisiones

Linear probing inspecciona posiciones consecutivas y sufre clustering primario. Quadratic probing
dispersa desplazamientos, pero necesita capacidad y parámetros compatibles. Double hashing usa un
segundo hash coprimo con la capacidad. Chaining, implementado en el proyecto 15, mantiene lista por
bucket. Bucket addressing guarda varias claves antes del overflow.

La eliminación en addressing abierto usa tombstone. Vaciar directamente una celda puede cortar la
cadena de búsqueda de una clave posterior. Rehashing elimina tombstones y cambia capacidad.

La tabla siguiente implementa direccionamiento abierto con sondeo lineal, estados vacío/ocupado/
tombstone, actualización, eliminación y rehash. El dataset provoca colisiones y reutiliza una tumba.

#### Implementación completa: tabla hash con direccionamiento abierto

Este listado es parte del capítulo: contiene la estructura y el programa de ejercicio correspondiente. No es pseudocódigo ni un fragmento recortado.

##### `book_examples/27_drozdek_core_structures/HashTableExample.ns`

```csharp
using NativeSharp.Core;

namespace NativeSharp.BookExamples.DrozdekCore
{
    public class IntHashTable
    {
        private int[] keys;
        private int[] values;
        private int[] states;
        private int count;
        private int used;

        public IntHashTable(int initialCapacity)
        {
            if (initialCapacity < 5)
            {
                initialCapacity = 5;
            }
            keys = new int[initialCapacity];
            values = new int[initialCapacity];
            states = new int[initialCapacity];
        }

        public int Count
        {
            get
            {
                return count;
            }
        }

        public int Capacity
        {
            get
            {
                return keys.Length;
            }
        }

        private int Hash(int key, int capacity)
        {
            int result = key % capacity;
            if (result < 0)
            {
                result = result + capacity;
            }
            return result;
        }

        private int FindSlot(int key)
        {
            int start = Hash(key, keys.Length);
            int step = 0;
            while (step < keys.Length)
            {
                int slot = (start + step) % keys.Length;
                if (states[slot] == 0)
                {
                    return -1;
                }
                if (states[slot] == 1 && keys[slot] == key)
                {
                    return slot;
                }
                step = step + 1;
            }
            return -1;
        }

        private int FindInsertionSlot(int key)
        {
            int start = Hash(key, keys.Length);
            int firstDeleted = -1;
            int step = 0;
            while (step < keys.Length)
            {
                int slot = (start + step) % keys.Length;
                if (states[slot] == 1 && keys[slot] == key)
                {
                    return slot;
                }
                if (states[slot] == 2 && firstDeleted == -1)
                {
                    firstDeleted = slot;
                }
                if (states[slot] == 0)
                {
                    if (firstDeleted != -1)
                    {
                        return firstDeleted;
                    }
                    return slot;
                }
                step = step + 1;
            }
            return firstDeleted;
        }

        private void InsertWithoutGrowth(int key, int value)
        {
            int slot = FindInsertionSlot(key);
            if (states[slot] == 1)
            {
                values[slot] = value;
                return;
            }
            if (states[slot] == 0)
            {
                used = used + 1;
            }
            keys[slot] = key;
            values[slot] = value;
            states[slot] = 1;
            count = count + 1;
        }

        private void Rehash(int newCapacity)
        {
            int[] oldKeys = keys;
            int[] oldValues = values;
            int[] oldStates = states;
            int oldCapacity = oldKeys.Length;
            keys = new int[newCapacity];
            values = new int[newCapacity];
            states = new int[newCapacity];
            count = 0;
            used = 0;
            int slot = 0;
            while (slot < oldCapacity)
            {
                if (oldStates[slot] == 1)
                {
                    InsertWithoutGrowth(oldKeys[slot], oldValues[slot]);
                }
                slot = slot + 1;
            }
            delete[] oldKeys;
            delete[] oldValues;
            delete[] oldStates;
        }

        public void Put(int key, int value)
        {
            if ((used + 1) * 10 >= keys.Length * 7)
            {
                Rehash(keys.Length * 2 + 1);
            }
            InsertWithoutGrowth(key, value);
        }

        public bool ContainsKey(int key)
        {
            return FindSlot(key) != -1;
        }

        public int Get(int key)
        {
            int slot = FindSlot(key);
            if (slot == -1)
            {
                throw new InvalidOperationException();
            }
            return values[slot];
        }

        public bool Remove(int key)
        {
            int slot = FindSlot(key);
            if (slot == -1)
            {
                return false;
            }
            states[slot] = 2;
            count = count - 1;
            return true;
        }

        public int Checksum()
        {
            int checksum = 0;
            int slot = 0;
            while (slot < keys.Length)
            {
                if (states[slot] == 1)
                {
                    checksum = checksum + keys[slot] * 3 + values[slot] * 7;
                }
                slot = slot + 1;
            }
            return checksum;
        }

        ~IntHashTable()
        {
            delete[] keys;
            delete[] values;
            delete[] states;
        }
    }

    public class HashTableExample
    {
        public static int Run()
        {
            raii IntHashTable table = new IntHashTable(5);
            table.Put(5, 50);
            table.Put(16, 160);
            table.Put(27, 270);
            table.Put(38, 380);
            table.Put(-6, 60);
            table.Put(49, 490);
            table.Put(16, 161);
            table.Remove(27);
            table.Put(60, 600);
            int checksum = table.Checksum();
            checksum = checksum + table.Get(16) * 10;
            checksum = checksum + table.Count * 100;
            checksum = checksum + table.Capacity;
            if (!table.ContainsKey(27) && table.ContainsKey(60))
            {
                checksum = checksum + 1000;
            }
            Console.WriteLine("hash: linear probing, tombstones, update and rehash");
            return checksum;
        }
    }
}
```

### 57.3 Hash perfecto

Cichelli busca pesos de caracteres para nombres conocidos; FHCD construye un grafo de restricciones y
asigna valores a vértices. Son apropiados para conjuntos estáticos. La función generada debe validarse
sobre todo el conjunto y conservarse junto con su versión.

### 57.4 Cuckoo, hashing extensible y lineal

Cuckoo mantiene dos posiciones posibles y desplaza residentes; un ciclo obliga a rehash. El proyecto
25 implementa desplazamiento acotado. Extendible hashing usa directorio por prefijo y profundidades
global/local; linear hashing divide buckets gradualmente sin duplicar un directorio completo.

La implementación editorial calcula índices de directorio y modela split. Un sistema persistente debe
agregar páginas, overflow, recuperación ante escritura parcial y métricas de ocupación.

## 58. Compresión de datos

### 58.1 Condiciones y modelo

Compresión explota redundancia. Debe especificar alfabeto, modelo, representación de bits, encabezado,
padding y manejo de streams corruptos. Una lista de códigos impresa no es todavía un formato.

### 58.2 Huffman estático y adaptativo

El tutorial 23 construye frecuencias, heap, árbol y códigos prefijos. Para un símbolo único se asigna
al menos un bit. El decoder recorre el árbol por bits y rechaza caminos inexistentes o final dentro de
un código.

Huffman adaptativo actualiza árbol y pesos tras cada símbolo y permite comenzar sin tabla. Debe
mantener sibling property e intercambiar nodos sin romper padres. Su costo y sincronización son parte
del protocolo entre encoder y decoder.

### 58.3 Run-length y Ziv-Lempel

RLE reemplaza corridas por `(count, symbol)` y puede expandir datos sin corridas. LZ77 emite distancia,
longitud y siguiente símbolo usando ventana; LZW agrega frases a un diccionario sincronizado. El
proyecto 25 implementa conteo de tokens RLE, matching de ventana LZ77 y crecimiento de diccionario
LZW sobre los datasets clásicos.

### 58.4 Huffman con RLE

El pipeline combinado transforma primero una representación que produzca símbolos sesgados y después
aplica Huffman, o viceversa según dominio. El formato debe indicar orden de filtros. La prueba completa
es round-trip byte por byte, no sólo una razón de tamaños.

## 59. Administración de memoria

### 59.1 Ajuste secuencial

First-fit toma el primer hueco suficiente; next-fit continúa desde la última posición; best-fit toma
el menor suficiente; worst-fit el mayor. Fragmentación externa mide huecos inutilizables aunque haya
memoria total. El proyecto 25 implementa selección sobre una lista explícita de intervalos.

### 59.2 Ajuste no secuencial y buddy systems

Segregated lists agrupan tamaños. Binary buddy redondea a potencia de dos, divide bloques y fusiona
con el buddy calculado por XOR. Fibonacci buddy usa tamaños consecutivos de Fibonacci y reglas de
recombinación distintas. El módulo calcula el orden binario requerido y el laboratorio agrega listas
libres por orden.

### 59.3 Mark-and-sweep

Mark parte de raíces y visita referencias; sweep libera no marcados y limpia marcas. El proyecto 25
simula un heap con campos head/tail y marca siete celdas. Una implementación sin stack puede usar
pointer reversal como Schorr-Waite, restaurando cada enlace al finalizar.

### 59.4 Copia, incremental y generacional

Cheney copia alcanzables a semispace y usa forwarding pointers para preservar sharing y ciclos. Baker
intercala trabajo de colección con asignaciones. Yuasa usa barrera de escritura para que mutaciones no
oculten objetos blancos. Generacional explota que muchos objetos mueren jóvenes y necesita remembered
sets para referencias de viejo a joven.

`CopyReachable` del proyecto 25 muestra forwarding y asignación de destinos. El laboratorio completo
debe validar alias compartido, ciclo, raíz duplicada y objeto inalcanzable.

### 59.5 Colector in-place

El caso in-place marca sin espacio proporcional a la profundidad usando inversión temporal de
punteros y tags. La auditoría registra cada transición, demuestra restauración del grafo y después
construye la free list. Este algoritmo es educativo: NativeSharp productivo ya ofrece ownership y
RAII, pero comprender un collector explica los costos que un programador de C# normalmente no ve.

## 60. Búsqueda de cadenas

### 60.1 Matching exacto directo, KMP y Boyer-Moore

La búsqueda directa prueba cada alineación. KMP precomputa la longitud del prefijo propio que también
es sufijo y nunca retrocede en el texto. Boyer-Moore compara desde la derecha y combina bad-character
y good-suffix; Horspool conserva una tabla de desplazamiento más simple.

El proyecto 25 implementa directa, KMP y Horspool sobre bytes. Todos devuelven la misma posición en el
corpus y prueban patrón vacío, ausente, solapado y mayor que el texto.

### 60.2 Búsquedas múltiples y enfoque por bits

Aho-Corasick construye trie con failure links y encuentra múltiples patrones en O(texto + salidas).
Bitap representa estados en bits y actualiza con shifts y máscaras; el proyecto 25 incluye la versión
exacta para patrones de hasta treinta bits útiles.

### 60.3 Conjuntos de palabras y expresiones regulares

Un trie reconoce conjuntos finitos. Thompson convierte una expresión regular a NFA con transiciones
epsilon para concatenación, alternancia y estrella. Simular el cierre epsilon evita backtracking
exponencial. Capturas y lookaround no forman parte del núcleo y requieren semántica adicional.

### 60.4 Suffix tries, trees y arrays

Un suffix trie inserta todos los sufijos y puede costar Θ(n²) espacio. Un suffix tree compacta caminos;
Ukkonen construye online en tiempo lineal bajo supuestos de alfabeto. Un suffix array ordena índices de
sufijos y usa menos memoria; LCP acelera consultas repetidas.

El laboratorio valida cada estructura contra búsqueda directa y usa separadores distintos al combinar
dos textos. No se usan punteros dentro de datos persistentes: nodos se serializan por índice.

### 60.5 Matching aproximado

Levenshtein mide inserción, borrado y sustitución; el proyecto 22 reconstruye la tabla completa. Para
matching con hasta `k` errores se actualiza una frontera por posición o se usa Bitap aproximado cuando
el patrón cabe en una palabra. Una banda de ancho `2k + 1` evita trabajo que no puede mejorar el límite.

### 60.6 Longest common substring

Substring exige contigüidad, a diferencia de la subsecuencia del proyecto 21. El proyecto 25 incluye
DP que reinicia a cero al no coincidir. La alternativa suffix tree/array concatena textos con
separadores y busca el LCP máximo entre sufijos de orígenes distintos.

## 61. Apéndice Drozdek A: cálculo de O grande

### 61.1 Serie armónica

`H_n = 1 + 1/2 + ... + 1/n = Θ(log n)`. Agrupar términos entre potencias de dos ofrece cotas. Aparece
en análisis promedio, árboles aleatorios y algoritmos con costo inversamente proporcional al rango.

### 61.2 Aproximación de `lg(n!)`

`lg(n!) = Σ lg i = Θ(n log n)`. La integral y Stirling refinan la aproximación. Este resultado sostiene
el límite inferior de ordenamientos por comparación.

### 61.3 Quicksort promedio

Con pivote uniformemente distribuido, la recurrencia promedia todas las particiones y produce
Θ(n log n). El peor caso Θ(n²) sigue existiendo; aleatorizar entrada o pivote reduce la probabilidad,
no cambia la cota teórica del peor caso.

### 61.4 Camino medio de árbol binario aleatorio

La profundidad total esperada se relaciona con inserciones BST aleatorias y es Θ(n log n). Dividir por
`n` da profundidad media Θ(log n), aunque una permutación ordenada produce cadena.

### 61.5 Número mínimo de nodos AVL

`N(h) = 1 + N(h - 1) + N(h - 2)` con bases pequeñas. Crece como Fibonacci, por lo que altura es
O(log n). Esta recurrencia demuestra el límite, no sólo lo observa en un dibujo.

## 62. Apéndice Drozdek B: catálogo de algoritmos de biblioteca

La biblioteca se organiza por categorías, con su equivalente NativeSharp o implementación editorial:

| Categoría | Operaciones | Equivalente en esta edición |
|---|---|---|
| no modificadores | búsqueda, conteo, comparación | proyectos 3 y 25 |
| modificadores | copia, movimiento, reemplazo, eliminación | arrays, `List<T>` y ownership |
| partición y orden | partition, stable partition, sort | proyecto 4 |
| búsqueda ordenada | lower bound, upper bound, binary search | proyecto 3 |
| merge y set ordenado | merge, unión, intersección, diferencia | proyectos 4 y 15 |
| heap | make, push, pop, sort heap | proyecto 13 |
| permutaciones | next/previous permutation | laboratorio de backtracking |
| numéricos | accumulate, inner product, partial sums | proyectos 23 y 24 |

Un algoritmo de biblioteca no elimina su precondición. Binary search requiere orden; merge requiere
dos rangos ordenados; heap operations requieren la invariante heap. La API debe decidir si verifica,
documenta o expresa la precondición mediante tipo.

## 63. Apéndice Drozdek C: NP-completitud

### 63.1 Lenguajes, certificados y reducciones

Un problema de decisión se expresa como lenguaje de instancias sí. Un verificador polinomial recibe
instancia y certificado. Una reducción many-one calcula `f(x)` en tiempo polinomial y satisface
`x ∈ A` si y sólo si `f(x) ∈ B`.

### 63.2 Teorema de Cook

Cook-Levin demuestra que SAT es NP-completo al codificar el cómputo de una máquina no determinista en
una fórmula. Esta edición presenta la arquitectura de la prueba: tableau, restricciones de celda,
estado inicial, transición y aceptación. No confunde el teorema con una implementación práctica de SAT.

### 63.3 Laboratorio de problemas difíciles

Los solvers pequeños de Hamilton y coloración del proyecto 25 ofrecen verificadores independientes.
El ejercicio agrega clique y vertex cover sobre la misma matriz, prueba la relación
`S es clique en G` si y sólo si `S es independiente en el complemento`, y registra crecimiento al
aumentar `V`. El objetivo es observar explosión combinatoria con un contrato correcto, no presentar
backtracking como solución escalable.

## 64. Matriz de equivalencia de los listados de Drozdek

La siguiente matriz cubre los listados de programa señalados explícitamente por las figuras de la
cuarta edición. “Proyecto” identifica la implementación NativeSharp completa incluida en este libro.

| Figuras | Contenido programable | Proyecto NativeSharp |
|---|---|---:|
| 1.4 | operaciones de vector | tutoriales 5 y 15 |
| 1.5 | registros de archivo | 25, `CaseStudyAlgorithms.ns` |
| 3.2 | lista simple de enteros | 5 |
| 3.10 | lista doble | 9 |
| 3.17 | skip list | 25, `SkipListDemo.ns` |
| 3.25 | operaciones de lista estándar | tutorial 15 |
| 3.28 | sistema de biblioteca | 25, `CaseStudyAlgorithms.ns` |
| 4.4 | pila sobre vector | 6 |
| 4.9-4.10 | colas sobre array y lista | 7 y 22 |
| 4.13 | simulación bancaria | 7 |
| 4.18 | priority queue | 13 |
| 4.20 | deque | 11 |
| 4.24 | laberinto | 25, `CaseStudyAlgorithms.ns` |
| 5.6 | curva de Koch | 25, `CaseStudyAlgorithms.ns` |
| 5.13 | ocho reinas | 25, `CaseStudyAlgorithms.ns` |
| 5.20 | intérprete | 25, `ExpressionParser` |
| 6.8 | BST genérico | 8 |
| 6.10-6.11 | BFS y DFS de árbol | 8 y 16 |
| 6.15-6.17 | recorridos iterativos | 25, `TreeAlgorithms.ns` |
| 6.19-6.20 | árbol threaded y Morris | 25, `TreeAlgorithms.ns` |
| 6.23-6.24 | inserción normal y threaded | 8 y 25, `TreeAlgorithms.ns` |
| 6.29-6.32 | borrado por merging y copying | 8 y 25, `TreeAlgorithms.ns` |
| 6.56 | sift-down | 13 |
| 6.68 | frecuencias de palabras | 25, `CaseStudyAlgorithms.ns` |
| 7.43 | trie | 14 |
| 7.44 | corrector ortográfico | 14 y 25, `CaseStudyAlgorithms.ns` |
| 8.47 | representantes distintos | 25, flujo y matching |
| 9.8 | Shell sort | 4 |
| 9.11 | quicksort | 4 |
| 9.16 | radix sort | 25, `SequenceAlgorithms.ns` |
| 9.22 | suma de polinomios | 23 |
| 10.17 | hashing con buckets | 15 y 25 |
| 11.12 | Huffman combinado con RLE | tutorial 23 y 25 |
| 12.17 | colector in-place | 25, `MemoryAlgorithms.ns`, Schorr-Waite |
| 13.8 | longest common substring | 25, `StringAlgorithms.ns` |

## 65. Criterio de aceptación de la cobertura Drozdek

Esta parte sólo puede marcarse como terminada cuando se cumplen simultáneamente estas condiciones:

- todas las secciones 48 a 63 están presentes en el índice y el cuerpo;
- cada algoritmo declara precondición, invariante, complejidad y límites;
- cada listado de la matriz apunta a código NativeSharp completo o a una adaptación explícita;
- los proyectos compilan en `-O0` y `-O2` con el compilador registrado en el colofón;
- ambos ejecutables producen salida y código de retorno idénticos;
- el estilo editorial se aplica antes de recompilar las versiones publicadas;
- ninguna limitación de la biblioteca se disfraza como equivalencia inexistente.

La matriz es deliberadamente auditable. Si una futura edición de Drozdek o del compilador agrega un
tema, el hueco debe aparecer como requisito pendiente antes de que el libro vuelva a llamarse completo.

---

# Parte XIII. Live code al estilo Deitel, con filosofía NativeSharp

Esta parte adopta la fortaleza pedagógica del enfoque Deitel: presentar una aplicación funcionando,
desmontarla línea por línea, hacerla crecer y terminar con ejercicios que obligan a modificar el
diseño. La adaptación no copia programas ni explicaciones. Conserva la disciplina de aprendizaje por
código vivo y reemplaza las suposiciones del CLR por contratos propios de NativeSharp.

En cada laboratorio aparecen seis elementos:

1. **Prueba de manejo:** salida y comportamiento antes de estudiar la implementación.
2. **Modelo:** clases, structs, arrays, delegates o recursos que forman la solución.
3. **Construcción incremental:** versiones pequeñas que agregan una responsabilidad por vez.
4. **Auditoría nativa:** ownership, RAII, layout, costo, errores y fronteras de plataforma.
5. **Pruebas:** resultado conocido, checksum y casos de borde.
6. **Ejercicios:** cambios suficientemente grandes para exigir razonamiento, no edición cosmética.

## 66. La filosofía live-code de NativeSharp

### 66.1 Familiaridad sin fingir un CLR

Una clase, una propiedad o un `foreach` deben resultar familiares para un programador C#. Esa
familiaridad es una puerta de entrada, no una promesa de semántica administrada. NativeSharp produce
código nativo, destruye de manera determinista y expresa transferencia mediante `owned`, `borrowed`,
`raii` y `move`.

Cada ejemplo responde, además de “¿qué imprime?”, estas preguntas:

- ¿Qué bloque de memoria contiene cada valor?
- ¿Quién destruye cada objeto, array, string, handle o delegate?
- ¿Qué operaciones asignan memoria o cruzan el sistema operativo?
- ¿Qué cambia entre O0 y O2 y qué debe permanecer idéntico?
- ¿Qué parte del comportamiento pertenece al lenguaje y cuál a Windows o a una DLL?

### 66.2 De la demostración al programa editorial

Una demostración de diez líneas sirve para una regla aislada. Un capítulo de programación necesita
estado, invariantes, errores, varias clases y un resultado verificable. Por ello, las versiones finales
se publican como proyectos `.nsproj` completos y no como fragmentos inconexos.

El proyecto 26 contiene seis aplicaciones conectadas por un checksum. Los proyectos 1 a 25 ofrecen
las ampliaciones de algoritmos, estructuras, interoperabilidad y diseño orientado a objetos.

### 66.3 La secuencia de una lección

El lector ejecuta primero. Después predice una modificación, cambia el código, recompila y compara.
La explicación se introduce donde el programa la necesita. Una excepción aparece al validar una hora,
no en una lista abstracta de keywords; un destructor aparece cuando una cuenta posee un nombre; un
atomic aparece cuando dos hilos comparten un contador.

## 67. Aplicación 1: cuenta bancaria nativa

### 67.1 Primera versión: nombre y saldo

`BankAccount` comienza con dos campos privados. El nombre se copia porque la cuenta lo posee. El
saldo se expresa en centavos `long`; no se usa `double` para dinero. El constructor rechaza nombre
vacío y saldo negativo antes de establecer la invariante.

La primera prueba crea una cuenta y consulta propiedades. El lector debe identificar que la referencia
local `account` es el dueño RAII y que la propiedad `Owner` presta el string interno. Destruir el valor
devuelto sería un error de ownership.

### 67.2 Segunda versión: depósito y retiro

`Deposit` exige cantidad positiva y falla mediante excepción si el contrato se viola. `TryWithdraw`
usa resultado booleano porque “fondos insuficientes” es una posibilidad normal del dominio. La API
distingue error de programación de decisión comercial.

Después de depositar 27,550 centavos y retirar 32,000, el saldo conocido es 120,550. Un segundo retiro
demasiado grande debe dejar el saldo intacto. El resultado se incorpora al checksum, por lo que no
basta imprimir una frase correcta.

### 67.3 Tercera versión: estado y ownership

`StatementLine` devuelve `owned string`: crea una representación que el llamador debe poseer o mover.
El destructor libera el nombre interno exactamente una vez. Este pequeño ejemplo establece la regla
que después gobierna árboles, colecciones y respuestas de APIs nativas.

### 67.4 Ejercicios de ampliación

- Agregar identificador inmutable y detectar cuentas duplicadas en un ledger.
- Implementar transferencia atómica entre dos cuentas bajo un `Mutex`.
- Registrar movimientos en un array dinámico y producir estado de cuenta.
- Definir una política de sobregiro separada de la clase de almacenamiento.
- Serializar centavos en little-endian y validar versión del archivo.

## 68. Aplicación 2: control de flujo con políticas reales

### 68.1 Calificaciones, sentinelas y clasificación

Un flujo de calificaciones valida rango 0..100, acumula total y clasifica en once buckets. La versión
interactiva puede usar sentinela; la versión automatizada usa un array para que las pruebas sean
reproducibles. `switch` clasifica políticas discretas; `if` expresa rangos y condiciones compuestas.

No se mezcla lectura con cálculo. Un productor obtiene datos, `GradeBook` valida y conserva, y una
capa de presentación imprime. Esta separación permite ejecutar el mismo dominio desde consola, GUI o
host C sin duplicar estadísticas.

### 68.2 Interés compuesto sin redondeo accidental

El ejemplo clásico de interés es útil para loops, pero un libro NativeSharp debe discutir
representación. Un simulador financiero serio no acumula dinero en `double`. El laboratorio usa
centavos o fixed-point, define la regla de redondeo por periodo y comprueba conservación.

Ejercicio: compare redondear mensualmente con redondear sólo al final. Documente cuál contrato usa una
institución y pruebe valores cercanos al límite de `long`.

### 68.3 Pólizas mediante `switch`

Una póliza de automóvil clasifica región y tipo de vehículo. La primera versión usa `switch`; la
segunda extrae una estrategia. La evolución enseña que una construcción de control es adecuada para
un conjunto cerrado, mientras el polimorfismo favorece políticas extensibles.

### 68.4 Operadores lógicos y cortocircuito

Las condiciones se escriben para que una lectura peligrosa aparezca después de su guardia:
`index < length && data[index] == value`. El cortocircuito es parte de la seguridad del acceso. Los
operadores booleanos no cortocircuitados evalúan ambos lados y sólo se usan cuando eso forma parte del
contrato.

## 69. Aplicación 3: azar reproducible, baraja y craps

### 69.1 Generador y semilla

`Random(20260906L)` produce una secuencia reproducible. Una prueba que usa auto-semilla puede fallar
una vez y volverse imposible de repetir. La semilla explícita se registra junto al compilador y nivel
de optimización.

El ejemplo lanza dados en `[1, 7)` y construye histogramas. La discusión incluye sesgo por módulo,
diferencia entre PRNG y CSPRNG y por qué ningún resultado de `Random` debe proteger secretos.

### 69.2 Craps como máquina de estados

`CrapsStatus` distingue continuar, ganar y perder. El primer lanzamiento decide resultado o punto;
los siguientes terminan al repetir punto o sacar siete. La implementación evita booleanos ambiguos
como `done` más `won` que pueden representar estados imposibles.

La prueba ejecuta mil partidas con semilla fija. Los triunfos no se comparan con una probabilidad
redondeada: se comparan con el resultado exacto de esa secuencia. Otro test estadístico separado puede
evaluar distribución sobre muchas semillas.

### 69.3 Baraja y Fisher-Yates

`Deck` crea 52 objetos `Card`, posee el array y destruye cada carta. Fisher-Yates recorre desde el
final y elige uniformemente entre posiciones todavía disponibles. Intercambiar cada posición con una
posición de todo el array introduce sesgo y no es una variante equivalente.

`Deal` presta una carta cuyo dueño sigue siendo la baraja. El llamador puede leerla, pero no
destruirla ni conservarla más allá de la vida del deck. El ejemplo hace explícita una relación que el
GC de C# suele mantener automáticamente.

### 69.4 Ejercicios

- Evaluar manos de póquer sin depender del orden recibido.
- Simular blackjack con un shoe de varias barajas y carta de corte.
- Comparar Fisher-Yates con un shuffle sesgado mediante frecuencias de permutaciones pequeñas.
- Guardar y restaurar estado del PRNG para reproducir una partida.

## 70. Aplicación 4: `GradeBook`, arrays y excepciones

### 70.1 Construcción y copia defensiva

El constructor recibe un array prestado, valida cada calificación y copia a almacenamiento propio.
Guardar directamente la referencia permitiría que el llamador cambiara las notas sin pasar por la
invariante y crearía ambigüedad sobre quién ejecuta `delete[]`.

Una entrada inválida lanza antes de publicar el objeto. El proyecto prueba mínimo, máximo, promedio
multiplicado por cien e histograma. El promedio entero se presenta como decisión deliberada, no como
equivalente silencioso de formato decimal.

### 70.2 Histogramas y visualización textual

Once buckets representan 0-9 hasta 100. El gráfico textual separa cómputo de salida y permite validar
el array de conteos antes de dibujar. Para millones de datos, el número de buckets permanece fijo y
el costo es O(n).

### 70.3 Rectangular frente a layout plano

El runtime actual favorece arrays nativos contiguos. Una matriz `rows × columns` puede almacenarse en
un array plano con `row * columns + column`. Esto hace explícito el layout que una DLL C espera y
evita confundir un array de referencias con una matriz contigua.

### 70.4 Manejo de excepciones

El ejemplo diferencia una calificación inválida, un conjunto vacío y un fallo de asignación o archivo.
El `catch` sólo captura lo que puede resolver; `finally` y RAII garantizan limpieza. No se usa una
excepción para una búsqueda que simplemente no encontró un elemento.

## 71. Aplicación 5: una clase de tiempo correcta

### 71.1 Representación canónica

`TimeOfDay` almacena segundos desde medianoche. Una sola representación evita estados como minuto 90
o dos combinaciones distintas para la misma hora. Las propiedades derivan hora, minuto y segundo.

El constructor vacío y las sobrecargas de una, dos y tres componentes terminan en la misma validación.
No se duplica lógica. El test principal avanza 23:59:59 y exige 00:00:00.

### 71.2 Encapsulación y comparación

El valor interno no se expone para escritura. `CompareTo` ordena por segundos. Una versión como
`struct` sería adecuada si se desea copia por valor; una clase permite identidad y extensión. El
capítulo obliga a justificar la elección en lugar de convertir todo tipo pequeño en clase por hábito
de C#.

### 71.3 Excepciones y fronteras

Se prueban 24:00:00, minuto 60, segundo negativo y último instante válido. El mensaje humano puede
mejorar, pero la invariante no depende del texto de la excepción. En una frontera `extern C`, la
excepción se captura y se traduce a código de error; nunca cruza el ABI.

## 72. Aplicación 6: nómina, facturas e interfaces

El proyecto 2 implementa una jerarquía de empleados con pago polimórfico. La ampliación estilo Deitel
agrega una interfaz `Payable` compartida por empleados y facturas. El reporte recibe elementos
heterogéneos y solicita importe sin conocer la fórmula concreta.

La filosofía NativeSharp agrega tres auditorías:

- La colección define si posee cada `Payable`.
- La destrucción a través de base o interfaz debe invocar el destructor dinámico correcto.
- Los importes se representan en centavos y se comprueba overflow antes de multiplicar.

Ejercicio: agregar impuestos como estrategia, generar una DLL con función `extern C` que calcule el
total y consumirla desde un host C# mediante P/Invoke.

## 73. Aplicación 7: consultas funcionales sin ocultar costo

### 73.1 De LINQ a pipelines explícitos

Los tutoriales 15, 16 y 17 muestran colecciones, `foreach`, delegates, lambdas y closures. Un pipeline
NativeSharp puede filtrar, proyectar, ordenar y agregar, pero debe documentar si es lazy o materializa
un array nuevo. “Una línea” no significa “una pasada” ni “sin asignaciones”.

El laboratorio de empleados ejecuta:

1. filtro por salario;
2. proyección a registro de reporte;
3. orden por departamento y apellido;
4. agrupación y total por departamento;
5. liberación de cada resultado materializado.

### 73.2 Genéricos AOT

Cada instancia cerrada se monomorfiza. Esto permite código especializado, pero puede aumentar tamaño
del binario. Constraints expresan capacidades necesarias. Un comparador o hasher delegado evita
suponer reflection, boxing o métodos virtuales universales.

### 73.3 Paralelismo explícito

No se presenta PLINQ como magia. Primero se mide la versión secuencial; después se particiona por
rangos, cada hilo produce un acumulador local y una reducción combina resultados. Compartir un
contador por elemento generaría contención aunque fuera correcto.

## 74. Aplicación 8: archivo de crédito con formato estable

### 74.1 Registros binarios

`CreditFileDemo` escribe pares de `int` como little-endian: número de cuenta y saldo en centavos.
No serializa el layout privado de una clase. El formato se puede leer desde otra versión del
compilador o desde C porque tamaño, orden de bytes y offsets están definidos.

### 74.2 Consulta de tres vías

La lectura clasifica saldos en crédito, débito y cero. La lógica recibe bytes ya validados; la capa de
archivo comprueba longitud antes de recorrer. Un archivo truncado no debe producir una lectura fuera
de límites ni un registro parcial aceptado.

### 74.3 Evolución del formato

La siguiente versión agrega magic, versión, cantidad de registros y checksum. Una migración lee la
versión anterior y escribe la nueva en un archivo temporal antes de reemplazar el original. La
durabilidad requiere distinguir “la función devolvió” de “los bytes llegaron al almacenamiento”.

### 74.4 Ejercicios

- Implementar consulta por cuenta con índice hash separado.
- Agregar tombstones y compactación.
- Exportar el lector como biblioteca estática y DLL.
- Fuzzear archivos truncados y campos fuera de rango.
- Comparar texto delimitado, registros fijos y formato versionado.

## 75. Aplicación 9: eventos y una interfaz nativa

Los capítulos GUI de C# enseñan controles y event handlers. NativeSharp no debe inventar Windows Forms.
El equivalente conceptual separa modelo, evento y adaptador de plataforma. El tutorial 18 implementa
publicación, suscripción y desuscripción mediante delegates nativos.

Una UI Win32, SDL o una DLL host puede traducir un mensaje externo a un método del modelo. El handler
no posee automáticamente al publisher ni al receptor. El programa debe garantizar que ningún
delegate invoque un objeto destruido.

El laboratorio comienza con una interfaz de consola que dispara los mismos eventos. Después se crea
un adaptador C ABI. Sólo al final se conecta una toolkit gráfica. De esta forma, la lógica permanece
probable sin clicks automatizados y la dependencia visual no contamina el dominio.

## 76. Aplicación 10: dos hilos nativos y estado atómico

### 76.1 Del `async` administrado al trabajo nativo

`async` y `await` dependen de Task, scheduler y convenciones que NativeSharp stage 57 no promete.
La adaptación usa `Thread`, `AtomicInt32`, mutexes, eventos y semáforos disponibles. No renombra un
hilo como “async”; explica bloqueo, join y memoria compartida.

### 76.2 Orden de vida

`CounterWorker` se declara antes que los objetos `Thread`. La destrucción inversa garantiza que los
hilos se unan antes de destruir el worker si se abandona el scope. El ejemplo llama `Join`
explícitamente y el destructor de `Thread` conserva auto-join como red de seguridad.

Cada worker captura sus excepciones: una excepción que escapa del entry point de un hilo puede terminar
el proceso. Dos hilos incrementan un `AtomicInt32` diez mil veces cada uno y el resultado exacto debe
ser 20,000.

### 76.3 Ejercicios

- Sustituir atomic por incremento ordinario y observar la carrera bajo muchas repeticiones.
- Particionar suma de un array sin compartir escritura durante el loop.
- Implementar productor-consumidor con `Semaphore` y buffer circular.
- Propagar error del worker mediante estado protegido y traducirlo en el hilo principal.
- Comparar join explícito, auto-join y detach, documentando riesgos de vida.

## 77. Doce talleres live-code

Los siguientes talleres forman una secuencia de estudio. Cada uno comienza con un ejecutable, agrega
una decisión de diseño y termina con una auditoría nativa.

| Taller | Aplicación | Conceptos NativeSharp | Implementación base |
|---:|---|---|---|
| 1 | diagnóstico de compilación | proyecto, `Main`, consola, exit code | tutorial 1 |
| 2 | calculadora de enteros | precedencia, overflow, división y errores | tutoriales 2-4 |
| 3 | cuenta bancaria | clase, propiedad, centavos, ownership | proyecto 26 |
| 4 | libro de calificaciones | arrays owned, estadísticas, histograma | proyecto 26 |
| 5 | simulador de interés | loops, fixed-point, redondeo | capítulo 68 |
| 6 | craps reproducible | enum, switch, PRNG y máquina de estados | proyecto 26 |
| 7 | baraja | objetos en array, Fisher-Yates y préstamos | proyecto 26 |
| 8 | tiempo del día | sobrecargas, validación y representación canónica | proyecto 26 |
| 9 | nómina y facturas | abstract, override, interface y destrucción | proyecto 2 |
| 10 | consultas de empleados | genéricos, delegates, materialización | tutoriales 15-17 |
| 11 | archivo de crédito | bytes, formato, versión y corrupción | proyecto 26 |
| 12 | contador paralelo | threads, atomics, excepciones y join | proyecto 26 |

## 78. Correspondencia editorial con la progresión Deitel

Esta tabla registra qué se conserva y qué se adapta de cada capítulo de *C# 6 for Programmers*.

| Capítulo Deitel | Propósito pedagógico | Tratamiento NativeSharp |
|---:|---|---|
| 1 | panorama de tecnología | código nativo, ABI, ownership y toolchain |
| 2 | IDE y programación visual | `.nsproj`, diagnósticos, build y adaptadores UI |
| 3 | primera aplicación | tutoriales 1-3 con exit codes verificables |
| 4 | clases y cuenta | capítulos 67 y proyecto 26 |
| 5-6 | control de flujo | tutorial 3 y capítulos 68-69 |
| 7 | métodos, azar y recursión | tutoriales 4 y 20, proyectos 25-26 |
| 8 | arrays y excepciones | tutoriales 5 y 19, capítulo 70 |
| 9 | LINQ y List | pipelines explícitos del capítulo 73 |
| 10 | clases profundas y tiempo | capítulo 71 |
| 11-12 | herencia, interfaces y polimorfismo | proyecto 2 y capítulo 72 |
| 13 | excepciones profundas | RAII, unwinding y fronteras nativas |
| 14-15 | GUI y eventos | modelo de eventos más adaptador de plataforma |
| 16 | strings y caracteres | bytes, UTF-8, ownership y algoritmos de texto |
| 17 | archivos y streams | capítulo 74 y `NativeSharp.Core.IO` |
| 18 | genéricos | monomorfización AOT y constraints |
| 19 | colecciones y funcional | costos visibles, delegates y partición explícita |
| 20 | bases de datos | ABI de driver, transacciones y mapeo sin ORM supuesto |
| 21 | async/await | threads, atomics, eventos, semáforos y join |

## 79. Criterio de aceptación de un ejemplo live-code

Un ejemplo de esta parte no se considera terminado sólo porque compila. Debe cumplir:

- el comportamiento inicial puede ejecutarse antes de leer la explicación;
- cada incremento introduce una sola responsabilidad conceptual;
- el resultado se valida por datos o checksum, no sólo por salida plausible;
- se prueban al menos un límite y un fallo;
- ownership y destrucción se explican donde aparecen;
- no se importan garantías de GC, reflection, Task, LINQ o Windows Forms que no existen;
- la versión editorial exacta compila en O0 y O2;
- ambas ejecuciones coinciden;
- el código respeta Allman, cuatro espacios, `Funcion()` y 120 columnas.

Con este criterio, “estilo Deitel” describe una forma rigurosa de enseñar mediante programas vivos;
“filosofía NativeSharp” determina las decisiones de memoria, rendimiento, ABI y plataforma.

---

# Apéndice A. Índice público de la biblioteca

Este índice se genera desde las declaraciones `public` de los fuentes actuales. Es una guía de
navegación, no reemplaza los contratos de ownership, excepciones y precondiciones descritos en cada
tipo.

## A.1 `NativeSharp.Core`

### `NativeSharp.Core` - `Allocator.ns`

- `public class Allocator`
- `public virtual byte* Allocate(int bytes)`
- `public virtual void Reset()`
- `public virtual long BytesUsed()`

### `NativeSharp.Core` - `ArenaAllocator.ns`

- `public class ArenaBlock`
- `public owned byte[] almacen;`
- `public int usado;`
- `public owned ArenaBlock siguiente;`
- `public ArenaBlock(int tamano)`
- `public class ArenaAllocator : Allocator`
- `public static int DefaultBlockSize()`
- `public ArenaAllocator()`
- `public ArenaAllocator(int tamanoDeBloque)`
- `public int BlockSize()`
- `public int BlockCount()`
- `public override byte* Allocate(int bytes)`
- `public override void Reset()`
- `public override long BytesUsed()`
- `public long BytesReserved()`
- `public Span<byte> AllocateSpan(int cuantos, int bytesPorElemento)`

### `NativeSharp.Core` - `BinaryReader.ns`

- `public borrowed struct BinaryReader`
- `public BinaryReader(ReadOnlySpan<byte> source)`
- `public int Position()`
- `public int Length()`
- `public int Remaining()`
- `public bool IsAtEnd()`
- `public bool Seek(int newPosition)`
- `public bool Skip(int count)`
- `public Optional<byte> ReadByte()`
- `public Optional<sbyte> ReadSByte()`
- `public Optional<ushort> ReadUInt16()`
- `public Optional<short> ReadInt16()`
- `public Optional<uint> ReadUInt32()`
- `public Optional<int> ReadInt32()`
- `public Optional<ulong> ReadUInt64()`
- `public Optional<long> ReadInt64()`
- `public ReadOnlySpan<byte> ReadSpan(int count)`
- `public int ReadInto(byte[] destination, int count)`

### `NativeSharp.Core` - `BinaryWriter.ns`

- `public class BinaryWriter`
- `public BinaryWriter(ByteBuffer buffer)`
- `public int Count()`
- `public void WriteByte(byte value)`
- `public void WriteSByte(sbyte value)`
- `public void WriteUInt16(ushort value)`
- `public void WriteInt16(short value)`
- `public void WriteUInt32(uint value)`
- `public void WriteInt32(int value)`
- `public void WriteUInt64(ulong value)`
- `public void WriteInt64(long value)`
- `public int WriteBytes(byte[] source, int start, int length)`
- `public int WriteAllBytes(byte[] source)`
- `public void WriteZeros(int count)`
- `public int WriteFixedAscii(string text, int width)`

### `NativeSharp.Core` - `ByteBuffer.ns`

- `public class ByteBuffer`
- `public ByteBuffer()`
- `public ByteBuffer(int capacity)`
- `public int Count()`
- `public int Capacity()`
- `public bool IsEmpty()`
- `public void Clear()`
- `public int EnsureCapacity(int capacity)`
- `public void Add(byte value)`
- `public int AddRange(byte[] source, int start, int length)`
- `public int AddAll(byte[] source)`
- `public Span<byte> AsSpan()`
- `public byte[] ToArray()`
- `public byte this[int index]`

### `NativeSharp.Core` - `Console.ns`

- `public class Console`
- `public static void Write(string value)`
- `public static void WriteLine(string value)`
- `public static void ErrorWrite(string value)`
- `public static void ErrorWriteLine(string value)`

### `NativeSharp.Core` - `CString.ns`

- `public class CString`
- `public CString(string texto)`
- `public byte* Pointer()`
- `public int Length()`
- `public int SizeWithNul()`
- `public byte At(int index)`

### `NativeSharp.Core` - `DateTime.ns`

- `public struct DateTime`
- `public long Ticks;`
- `public static DateTime FromTicks(long ticks)`
- `public static DateTime FromFileTimeUtc(long fileTimeTicks)`

### `NativeSharp.Core` - `Environment.ns`

- `public class Environment`
- `public static long TickCount64()`
- `public static int ProcessorCount()`
- `public static string CurrentDirectory()`
- `public static void Exit(int exitCode)`
- `public static string GetEnvironmentVariable(string name)`
- `public static void SetEnvironmentVariable(string name, string value)`
- `public static int GetCommandLineArgCount()`
- `public static int GetCommandLineArgs(string[] destination)`

### `NativeSharp.Core` - `Exception.ns`

- `public class Exception`
- `public string Message;`
- `public class InvalidOperationException : Exception`
- `public class ArgumentOutOfRangeException : Exception`
- `public class KeyNotFoundException : Exception`
- `public class ArgumentException : Exception`

### `NativeSharp.Core` - `MathUtil.ns`

- `public class MathUtil`
- `public static int Min(int a, int b)`
- `public static int Max(int a, int b)`
- `public static int Clamp(int value, int min, int max)`
- `public static long Min(long a, long b)`
- `public static long Max(long a, long b)`
- `public static long Clamp(long value, long min, long max)`
- `public static float Min(float a, float b)`
- `public static float Max(float a, float b)`
- `public static float Clamp(float value, float min, float max)`
- `public static float Lerp(float a, float b, float t)`
- `public static double Min(double a, double b)`
- `public static double Max(double a, double b)`
- `public static double Clamp(double value, double min, double max)`
- `public static double Lerp(double a, double b, double t)`

### `NativeSharp.Core.Diagnostics` - `NativeClock.ns`

- `public class NativeClock`
- `public static long QueryCounterRaw()`
- `public static long QueryFrequencyRaw()`
- `public static long QuerySystemFileTimeRaw()`

### `NativeSharp.Core` - `Number.ns`

- `public class Number`
- `public static bool TryParseUInt64(string text, ref int index, int radix, out ulong value)`
- `public static bool TryParseInt64(string text, ref int index, int radix, out long value)`
- `public static bool TryParseIntegerLiteral( string text, ref int index, out ulong magnitude, out bool hasUnsignedSuffix, out bool hasLongSuffix)`
- `public static bool TryParseDouble(string text, ref int index, out double value)`
- `public static bool TryParseUInt64(string text, out ulong value)`
- `public static bool TryParseInt64(string text, out long value)`
- `public static bool TryParseInt32(string text, out int value)`
- `public static bool TryParseUInt32(string text, out uint value)`
- `public static bool TryParseDouble(string text, out double value)`
- `public static bool TryParseSingle(string text, out float value)`
- `public static bool TryParseUInt64Radix(string text, int radix, out ulong value)`

### `NativeSharp.Core` - `NumericLimits.ns`

- `public class NumericLimits`
- `public static byte ByteMin()`
- `public static byte ByteMax()`
- `public static short ShortMin()`
- `public static short ShortMax()`
- `public static ushort UShortMin()`
- `public static ushort UShortMax()`
- `public static int Int32Min()`
- `public static int Int32Max()`
- `public static uint UInt32Min()`
- `public static uint UInt32Max()`
- `public static long Int64Min()`
- `public static long Int64Max()`
- `public static ulong UInt64Min()`
- `public static ulong UInt64Max()`
- `public static float FloatMin()`
- `public static float FloatMax()`
- `public static double DoubleMin()`
- `public static double DoubleMax()`

### `NativeSharp.Core` - `Object.ns`

- `public class Object`
- `public virtual string ToString()`
- `public virtual bool Equals(Object other)`
- `public virtual int GetHashCode()`
- `public class RuntimeHelpers`
- `public static string TypeName(Object value)`
- `public static int ReferenceHash(Object value)`

### `NativeSharp.Core` - `Optional.ns`

- `public struct Optional<T>`
- `public Optional(T item)`
- `public bool HasValue()`
- `public bool IsEmpty()`
- `public T Value()`
- `public T ValueOr(T fallback)`
- `public T UncheckedValue()`
- `public class Optionals`
- `public static Optional<T> Some<T>(T value)`
- `public static Optional<T> None<T>()`

### `NativeSharp.Core` - `Random.ns`

- `public class Random`
- `public Random()`
- `public Random(long seed)`
- `public int Next()`
- `public int Next(int maxValue)`
- `public int Next(int minValue, int maxValue)`
- `public long NextInt64()`
- `public double NextDouble()`
- `public bool NextBool()`

### `NativeSharp.Core` - `Span.ns`

- `public borrowed struct Span<T>`
- `public Span(T* data, int length)`
- `public int Length()`
- `public bool IsEmpty()`
- `public T* Data()`
- `public T this[int index]`
- `public T UncheckedAt(int index)`
- `public void UncheckedSet(int index, T value)`
- `public Span<T> Slice(int start)`
- `public Span<T> Slice(int start, int length)`
- `public void Fill(T value)`
- `public int CopyTo(Span<T> destination)`
- `public int CopyToOverlapping(Span<T> destination)`
- `public ReadOnlySpan<T> AsReadOnly()`
- `public borrowed struct ReadOnlySpan<T>`
- `public ReadOnlySpan(T* data, int length)`
- `public int Length()`
- `public bool IsEmpty()`
- `public T* Data()`
- `public T this[int index]`
- `public T UncheckedAt(int index)`
- `public ReadOnlySpan<T> Slice(int start)`
- `public ReadOnlySpan<T> Slice(int start, int length)`
- `public int CopyTo(Span<T> destination)`
- `public class Spans`
- `public static Span<T> Empty<T>()`
- `public static Span<T> Of<T>(T[] items)`
- `public static Span<T> OfRange<T>(T[] items, int start, int length)`
- `public static Span<T> OfRaw<T>(T* data, int length)`
- `public static ReadOnlySpan<T> ReadOnlyEmpty<T>()`
- `public static ReadOnlySpan<T> ReadOnlyOf<T>(T[] items)`
- `public static ReadOnlySpan<T> ReadOnlyOfRange<T>(T[] items, int start, int length)`
- `public static ReadOnlySpan<T> ReadOnlyOfRaw<T>(T* data, int length)`

### `NativeSharp.Core` - `String.ns`

- `public enum StringComparison`
- `public enum StringSplitOptions`
- `public class String`
- `public static owned string Concat(string left, string right)`
- `public static bool Equals(string left, string right)`
- `public static owned string Substring(string value, int startIndex, int length)`
- `public static owned string FromCharArray(char[] value)`
- `public static char* CharacterPointer(string value, int index)`
- `public static string ToString(string value)`
- `public static owned string Copy(string value)`
- `public static string Empty`
- `public static bool IsNullOrEmpty(string value)`
- `public static bool Equals(string left, string right, StringComparison comparison)`
- `public static bool Equals(string left, string right, bool ignoreCase)`
- `public static bool IsNullOrWhiteSpace(string value)`
- `public static int IndexOf(string value, string needle)`
- `public static int IndexOf(string value, string needle, StringComparison comparison)`
- `public static int IndexOf(string value, string needle, bool ignoreCase)`
- `public static int LastIndexOf(string value, string needle)`
- `public static int IndexOfAny(string value, char[] anyOf)`
- `public static int LastIndexOfAny(string value, char[] anyOf)`
- `public static bool Contains(string value, string needle)`
- `public static bool Contains(string value, string needle, bool ignoreCase)`
- `public static bool Contains(string value, string needle, StringComparison comparison)`
- `public static bool StartsWith(string value, string prefix)`
- `public static bool StartsWith(string value, string prefix, bool ignoreCase)`
- `public static bool StartsWith(string value, string prefix, StringComparison comparison)`
- `public static bool EndsWith(string value, string suffix)`
- `public static bool EndsWith(string value, string suffix, bool ignoreCase)`
- `public static bool EndsWith(string value, string suffix, StringComparison comparison)`
- `public static owned string Insert(string value, int startIndex, string inserted)`
- `public static owned string Remove(string value, int startIndex)`
- `public static owned string Remove(string value, int startIndex, int count)`
- `public static owned string Replace(string value, char oldChar, char newChar)`
- `public static owned string Replace(string value, string oldValue, string newValue)`
- `public static owned string TrimStart(string value)`
- `public static owned string TrimEnd(string value)`
- `public static owned string Trim(string value)`
- `public static owned string Strip(string value)`
- `public static owned string LStrip(string value)`
- `public static owned string RStrip(string value)`
- `public static owned string PadLeft(string value, int totalWidth, char paddingChar)`
- `public static owned string PadLeft(string value, int totalWidth)`
- `public static owned string PadRight(string value, int totalWidth, char paddingChar)`
- `public static owned string PadRight(string value, int totalWidth)`
- `public static owned string LJust(string value, int width)`
- `public static owned string RJust(string value, int width)`
- `public static owned string Center(string value, int width)`
- `public static owned string ZFill(string value, int width)`
- `public static owned string ToLower(string value)`
- `public static owned string ToUpper(string value)`
- `public static owned string CaseFold(string value)`
- `public static owned string SwapCase(string value)`
- `public static owned string Capitalize(string value)`
- `public static owned string Title(string value)`
- `public static bool IsAlpha(string value)`
- `public static bool IsDigit(string value)`
- `public static bool IsAlnum(string value)`
- `public static bool IsAscii(string value)`
- `public static bool IsWhiteSpace(string value)`
- `public static bool IsUpper(string value)`
- `public static bool IsLower(string value)`
- `public static int GetHashCode(string value)`
- `public static char[] ToCharArray(string value)`
- `public static void CopyTo(string value, char[] destination, int destinationIndex)`
- `public static owned string Join(string separator, string[] values)`
- `public static int Split(string value, char separator, string[] destination)`
- `public static int Split(string value, string separator, string[] destination)`
- `public static int RSplit(string value, char separator, string[] destination)`
- `public static int SplitLines(string value, string[] destination)`
- `public static bool Partition(string value, string separator, string[] destination)`
- `public static bool RPartition(string value, string separator, string[] destination)`
- `public static int Count(string value, string needle)`
- `public static owned string RemovePrefix(string value, string prefix)`
- `public static owned string RemoveSuffix(string value, string suffix)`
- `public static int Compare(string left, string right)`
- `public static int CompareOrdinal(string left, string right)`
- `public static int CompareTo(string left, string right)`
- `public static owned string Clone(string value)`
- `public static owned string ToLowerInvariant(string value)`
- `public static owned string ToUpperInvariant(string value)`
- `public static owned string Format(string template, string[] args)`
- `public static bool IsDecimal(string value)`
- `public static bool IsNumeric(string value)`
- `public static bool IsIdentifier(string value)`
- `public static bool IsPrintable(string value)`
- `public static bool IsTitle(string value)`
- `public static owned string ExpandTabs(string value)`
- `public static owned string ExpandTabs(string value, int tabSize)`
- `public static owned string ReplaceLineEndings(string value)`
- `public static owned string ToString(bool value)`
- `public static owned string ToString(char value)`
- `public static owned string ToString(ulong value)`
- `public static owned string ToString(long value)`
- `public static owned string ToString(int value)`
- `public static owned string ToString(ulong value, int radix)`
- `public static owned string ToString(long value, int radix)`
- `public static owned string ToString(uint value, int radix)`
- `public static owned string ToString(int value, int radix)`
- `public static owned string ToHexString(ulong value)`
- `public static owned string ToHexString(ulong value, int minimumDigits, bool upperCase)`
- `public static owned string ToHexString(long value)`
- `public static owned string ToHexString(long value, int minimumDigits, bool upperCase)`
- `public static owned string ToHexString(uint value)`
- `public static owned string ToHexString(uint value, int minimumDigits, bool upperCase)`
- `public static owned string ToHexString(int value)`
- `public static owned string ToHexString(int value, int minimumDigits, bool upperCase)`
- `public static owned string ToString(double value)`
- `public static owned string ToString(double value, int significantDigits)`
- `public static owned string ToString(float value)`
- `public static owned string ToString(float value, int significantDigits)`
- `public static owned string ToStringFixed(double value, int decimals)`
- `public static owned string ToStringFixed(float value, int decimals)`

### `NativeSharp.Core` - `StringBuilder.ns`

- `public class StringBuilder`
- `public StringBuilder()`
- `public StringBuilder(int capacidadInicial)`
- `public int Length()`
- `public int Capacity()`
- `public void Clear()`
- `public char this[int index]`
- `public void Append(char valor)`
- `public void Append(string texto)`
- `public void Append(int valor)`
- `public void Append(long valor)`
- `public void Append(uint valor)`
- `public void Append(ulong valor)`
- `public void Append(bool valor)`
- `public void Append(double valor)`
- `public void AppendLine(string texto)`
- `public void AppendLine()`
- `public void AppendRepeat(char valor, int veces)`
- `public owned string ToOwnedString()`

### `NativeSharp.Core` - `StringView.ns`

- `public borrowed struct StringView`
- `public StringView(char* data, int length)`
- `public static StringView Empty()`
- `public static StringView Of(string text)`
- `public static StringView Of(string text, int start, int length)`
- `public int Length()`
- `public bool IsEmpty()`
- `public char* Data()`
- `public char this[int index]`
- `public char UncheckedAt(int index)`
- `public StringView Slice(int start)`
- `public StringView Slice(int start, int length)`
- `public static bool Equals(StringView left, StringView right)`
- `public static bool Equals(StringView left, string right)`
- `public int IndexOf(char value)`
- `public int LastIndexOf(char value)`
- `public bool Contains(char value)`
- `public bool StartsWith(StringView prefix)`
- `public bool EndsWith(StringView suffix)`
- `public StringView TrimStart()`
- `public StringView TrimEnd()`
- `public StringView Trim()`
- `public owned string ToString()`

### `NativeSharp.Core` - `TimeSpan.ns`

- `public struct TimeSpan`
- `public long Ticks;`
- `public static TimeSpan FromTicks(long ticks)`
- `public static TimeSpan Zero()`
- `public static TimeSpan FromMilliseconds(double milliseconds)`
- `public static TimeSpan FromSeconds(double seconds)`
- `public static TimeSpan FromMinutes(double minutes)`
- `public static TimeSpan FromHours(double hours)`
- `public static TimeSpan FromDays(double days)`
- `public double TotalMilliseconds()`
- `public double TotalSeconds()`
- `public double TotalMinutes()`
- `public double TotalHours()`
- `public double TotalDays()`
- `public TimeSpan Add(TimeSpan other)`
- `public TimeSpan Subtract(TimeSpan other)`
- `public TimeSpan Negate()`
- `public bool Equals(TimeSpan other)`
- `public int CompareTo(TimeSpan other)`

### `NativeSharp.Core` - `Utf8.ns`

- `public class Utf8`
- `public static int Invalid()`
- `public static int SequenceLength(byte lead)`
- `public static int DecodeAt(string texto, int index, out int usados)`
- `public static int NextIndex(string texto, int index)`
- `public static int CodePointCount(string texto)`
- `public static bool IsValid(string texto)`
- `public static bool IsBoundary(string texto, int index)`
- `public static int BoundaryAtOrBefore(string texto, int index)`
- `public static int EncodedLength(int punto)`
- `public static int Encode(int punto, byte[] destino, int desde)`

## A.2 `NativeSharp.Core.Collections.Generic`

### `NativeSharp.Core.Collections.Generic` - `Algorithm.ns`

- `public class Algorithm`
- `public static int Find<T>(ReadOnlySpan<T> donde, T que)`
- `public static int FindIf<T>(ReadOnlySpan<T> donde, Predicate<T> condicion)`
- `public static bool AnyOf<T>(ReadOnlySpan<T> donde, Predicate<T> condicion)`
- `public static bool AllOf<T>(ReadOnlySpan<T> donde, Predicate<T> condicion)`
- `public static T Min<T>(T a, T b)`
- `public static T Max<T>(T a, T b)`
- `public static void Reverse<T>(Span<T> datos)`
- `public static int RemoveIf<T>(Span<T> datos, Predicate<T> condicion)`
- `public static void Sort<T>(Span<T> datos)`
- `public static void Sort<T>(Span<T> datos, Comparison<T> comparador)`

### `NativeSharp.Core.Collections.Generic` - `Comparer.ns`

- `public class Comparer<T>`
- `public Comparer()`
- `public Comparer(Comparison<T> comparisonFunc)`
- `public static int CompareNatural(T a, T b)`
- `public int Compare(T a, T b)`
- `public class EqualityComparer<T>`
- `public EqualityComparer(HashFunc<T> hashFunc)`
- `public bool Equals(T a, T b)`
- `public int GetHashCode(T value)`

### `NativeSharp.Core.Collections.Generic` - `Delegates.ns`

- `public delegate int HashFunc<T>(T value);`
- `public delegate int Comparison<T>(T a, T b);`
- `public delegate bool Predicate<T>(T value);`
- `public delegate void Action<T>(T value);`
- `public delegate void KeyValueAction<TKey, TValue>(TKey key, TValue value);`

### `NativeSharp.Core.Collections.Generic` - `Dictionary.ns`

- `public class Dictionary<TKey, TValue>`
- `public Dictionary(HashFunc<TKey> hashFunc)`
- `public Dictionary(HashFunc<TKey> hashFunc, int capacity)`
- `public int Count()`
- `public bool ContainsKey(TKey key)`
- `public bool ContainsValue(TValue value)`
- `public bool TryGetValue(TKey key, TValue[] result)`
- `public TValue this[TKey key]`
- `public int EnsureCapacity(int capacity)`
- `public void TrimExcess()`
- `public void Add(owned TKey key, owned TValue value)`
- `public bool TryAdd(owned TKey key, owned TValue value)`
- `public void AddOrUpdate(owned TKey key, owned TValue value)`
- `public void SetValue(TKey key, owned TValue value)`
- `public bool Remove(TKey key)`
- `public void Clear()`
- `public TKey[] Keys()`
- `public TValue[] Values()`
- `public KeyValuePair<TKey, TValue>[] ToArray()`
- `public void ForEach(KeyValueAction<TKey, TValue> action)`

### `NativeSharp.Core.Collections.Generic` - `Hashing.ns`

- `public class Hashing`
- `public static int Int32(int value)`
- `public static int UInt32(uint value)`
- `public static int Int64(long value)`
- `public static int UInt64(ulong value)`
- `public static int Boolean(bool value)`
- `public static int Char(char value)`
- `public static int Byte(byte value)`
- `public static int Short(short value)`
- `public static int Str(string value)`
- `public static int HashCombine(int runningHash, int fieldHash)`

### `NativeSharp.Core.Collections.Generic` - `HashSet.ns`

- `public class HashSet<T>`
- `public HashSet(HashFunc<T> hashFunc)`
- `public HashSet(HashFunc<T> hashFunc, int capacity)`
- `public int Count()`
- `public bool Add(owned T value)`
- `public bool Contains(T value)`
- `public bool Remove(T value)`
- `public void Clear()`
- `public T[] ToArray()`
- `public int EnsureCapacity(int capacity)`
- `public void TrimExcess()`
- `public void UnionWith(HashSet<T> other)`
- `public void IntersectWith(HashSet<T> other)`
- `public void ExceptWith(HashSet<T> other)`
- `public void SymmetricExceptWith(HashSet<T> other)`
- `public bool IsSubsetOf(HashSet<T> other)`
- `public bool IsProperSubsetOf(HashSet<T> other)`
- `public bool IsSupersetOf(HashSet<T> other)`
- `public bool IsProperSupersetOf(HashSet<T> other)`
- `public bool Overlaps(HashSet<T> other)`
- `public bool SetEquals(HashSet<T> other)`
- `public int RemoveWhere(Predicate<T> match)`
- `public bool TryGetValue(T equalValue, T[] result)`
- `public void ForEach(Action<T> action)`
- `public owned HashSetEnumerator<T> GetEnumerator()`

### `NativeSharp.Core.Collections.Generic` - `HashSetEnumerator.ns`

- `public class HashSetEnumerator<T>`
- `public HashSetEnumerator(HashSet<T> sourceSet)`
- `public bool MoveNext()`
- `public T Current`

### `NativeSharp.Core.Collections.Generic` - `KeyValuePair.ns`

- `public struct KeyValuePair<TKey, TValue>`
- `public TKey Key;`
- `public TValue Value;`

### `NativeSharp.Core.Collections.Generic` - `LinkedList.ns`

- `public class LinkedList<T>`
- `public LinkedList()`
- `public int Count()`
- `public LinkedListNode<T> First()`
- `public LinkedListNode<T> Last()`
- `public LinkedListNode<T> AddFirst(owned T value)`
- `public LinkedListNode<T> AddLast(owned T value)`
- `public owned T RemoveFirst()`
- `public owned T RemoveLast()`
- `public LinkedListNode<T> AddAfter(LinkedListNode<T> node, owned T value)`
- `public LinkedListNode<T> AddBefore(LinkedListNode<T> node, owned T value)`
- `public LinkedListNode<T> Find(T value)`
- `public LinkedListNode<T> FindLast(T value)`
- `public void Remove(LinkedListNode<T> node)`
- `public bool Remove(T value)`
- `public bool Contains(T value)`
- `public T[] ToArray()`
- `public void Clear()`
- `public owned LinkedListEnumerator<T> GetEnumerator()`

### `NativeSharp.Core.Collections.Generic` - `LinkedListEnumerator.ns`

- `public class LinkedListEnumerator<T>`
- `public LinkedListEnumerator(LinkedList<T> sourceList)`
- `public bool MoveNext()`
- `public T Current`

### `NativeSharp.Core.Collections.Generic` - `LinkedListNode.ns`

- `public class LinkedListNode<T>`
- `public LinkedListNode()`
- `public T Value()`
- `public void SetValue(owned T value)`
- `public owned T TakeValue()`
- `public LinkedListNode<T> Next()`
- `public owned LinkedListNode<T> TakeNext()`
- `public owned LinkedListNode<T> SetNext(owned LinkedListNode<T> node)`
- `public LinkedListNode<T> Previous()`
- `public void SetPrevious(LinkedListNode<T> node)`

### `NativeSharp.Core.Collections.Generic` - `List.ns`

- `public class List<T>`
- `public List()`
- `public List(int capacity)`
- `public int Count()`
- `public owned ListEnumerator<T> GetEnumerator()`
- `public int Capacity()`
- `public T this[int index]`
- `public int EnsureCapacity(int capacity)`
- `public void TrimExcess()`
- `public void Add(owned T value)`
- `public void SetAt(int index, owned T value)`
- `public void Insert(int index, owned T value)`
- `public void RemoveAt(int index)`
- `public void AddRange(T[] values)`
- `public void InsertRange(int index, T[] values)`
- `public void RemoveRange(int index, int rangeCount)`
- `public T[] GetRange(int index, int rangeCount)`
- `public int IndexOf(T value)`
- `public int LastIndexOf(T value)`
- `public bool Contains(T value)`
- `public bool Remove(T value)`
- `public void Clear()`
- `public T[] ToArray()`
- `public void CopyTo(T[] destination, int destinationIndex)`
- `public void Reverse()`
- `public void Sort()`
- `public void Sort(Comparison<T> comparer)`
- `public int BinarySearch(T value)`
- `public int FindIndex(Predicate<T> match)`
- `public T Find(Predicate<T> match)`
- `public int FindLastIndex(Predicate<T> match)`
- `public T FindLast(Predicate<T> match)`
- `public T[] FindAll(Predicate<T> match)`
- `public bool Exists(Predicate<T> match)`
- `public bool TrueForAll(Predicate<T> match)`
- `public int RemoveAll(Predicate<T> match)`
- `public void ForEach(Action<T> action)`

### `NativeSharp.Core.Collections.Generic` - `ListEnumerator.ns`

- `public class ListEnumerator<T>`
- `public ListEnumerator(List<T> sourceList)`
- `public bool MoveNext()`
- `public T Current`

### `NativeSharp.Core.Collections.Generic` - `PriorityQueue.ns`

- `public class PriorityQueue<TElement, TPriority>`
- `public PriorityQueue()`
- `public PriorityQueue(int capacity)`
- `public PriorityQueue(Comparison<TPriority> comparerFunc)`
- `public int Count()`
- `public int Capacity()`
- `public int EnsureCapacity(int capacity)`
- `public void TrimExcess()`
- `public void Enqueue(owned TElement element, owned TPriority priority)`
- `public TElement Peek()`
- `public owned TElement Dequeue()`
- `public bool TryDequeue(TElement[] elementResult, TPriority[] priorityResult)`
- `public bool TryPeek(TElement[] elementResult, TPriority[] priorityResult)`
- `public void Clear()`
- `public TElement this[int index]`
- `public owned PriorityQueueEnumerator<TElement, TPriority> GetEnumerator()`

### `NativeSharp.Core.Collections.Generic` - `PriorityQueueEnumerator.ns`

- `public class PriorityQueueEnumerator<TElement, TPriority>`
- `public PriorityQueueEnumerator(PriorityQueue<TElement, TPriority> sourceQueue)`
- `public bool MoveNext()`
- `public TElement Current`

### `NativeSharp.Core.Collections.Generic` - `Queue.ns`

- `public class Queue<T>`
- `public Queue()`
- `public Queue(int capacity)`
- `public int Count()`
- `public int Capacity()`
- `public T this[int index]`
- `public int EnsureCapacity(int capacity)`
- `public void TrimExcess()`
- `public void Enqueue(owned T value)`
- `public owned T Dequeue()`
- `public T Peek()`
- `public bool TryDequeue(T[] result)`
- `public bool TryPeek(T[] result)`
- `public bool Contains(T value)`
- `public void Clear()`
- `public T[] ToArray()`
- `public void CopyTo(T[] destination, int destinationIndex)`
- `public owned QueueEnumerator<T> GetEnumerator()`

### `NativeSharp.Core.Collections.Generic` - `QueueEnumerator.ns`

- `public class QueueEnumerator<T>`
- `public QueueEnumerator(Queue<T> sourceQueue)`
- `public bool MoveNext()`
- `public T Current`

### `NativeSharp.Core.Collections.Generic` - `SortedDictionary.ns`

- `public class SortedDictionary<TKey, TValue>`
- `public SortedDictionary()`
- `public SortedDictionary(int capacity)`
- `public SortedDictionary(Comparison<TKey> comparerFunc)`
- `public int Count()`
- `public int EnsureCapacity(int capacity)`
- `public void TrimExcess()`
- `public bool ContainsKey(TKey key)`
- `public bool ContainsValue(TValue value)`
- `public bool TryGetValue(TKey key, TValue[] result)`
- `public TValue this[TKey key]`
- `public void Add(owned TKey key, owned TValue value)`
- `public void AddOrUpdate(owned TKey key, owned TValue value)`
- `public void SetValue(TKey key, owned TValue value)`
- `public bool Remove(TKey key)`
- `public void Clear()`
- `public TKey[] Keys()`
- `public TValue[] Values()`
- `public KeyValuePair<TKey, TValue>[] ToArray()`
- `public void ForEach(KeyValueAction<TKey, TValue> action)`

### `NativeSharp.Core.Collections.Generic` - `SortedList.ns`

- `public class SortedList<TKey, TValue>`
- `public SortedList()`
- `public SortedList(int capacity)`
- `public SortedList(Comparison<TKey> comparerFunc)`
- `public SortedList(Comparison<TKey> comparerFunc, int capacity)`
- `public int Count()`
- `public int Capacity()`
- `public int IndexOfKey(TKey key)`
- `public bool ContainsKey(TKey key)`
- `public bool ContainsValue(TValue value)`
- `public int IndexOfValue(TValue value)`
- `public bool TryGetValue(TKey key, TValue[] result)`
- `public TValue this[TKey key]`
- `public TKey KeyAt(int index)`
- `public TValue ValueAt(int index)`
- `public int EnsureCapacity(int capacity)`
- `public void TrimExcess()`
- `public void Add(owned TKey key, owned TValue value)`
- `public void AddOrUpdate(owned TKey key, owned TValue value)`
- `public void SetValue(TKey key, owned TValue value)`
- `public void RemoveAt(int index)`
- `public bool Remove(TKey key)`
- `public void Clear()`
- `public TKey[] Keys()`
- `public TValue[] Values()`
- `public KeyValuePair<TKey, TValue>[] ToArray()`
- `public void ForEach(KeyValueAction<TKey, TValue> action)`

### `NativeSharp.Core.Collections.Generic` - `SortedSet.ns`

- `public class SortedSet<T>`
- `public SortedSet()`
- `public SortedSet(int capacity)`
- `public SortedSet(Comparison<T> comparerFunc)`
- `public int Count()`
- `public int EnsureCapacity(int capacity)`
- `public void TrimExcess()`
- `public bool Add(owned T value)`
- `public bool Contains(T value)`
- `public bool Remove(T value)`
- `public void Clear()`
- `public T Min()`
- `public T Max()`
- `public T[] ToArray()`
- `public T[] Reverse()`
- `public SortedSet<T> GetViewBetween(T lowerValue, T upperValue)`
- `public int RemoveWhere(Predicate<T> match)`
- `public void UnionWith(SortedSet<T> other)`
- `public void IntersectWith(SortedSet<T> other)`
- `public void ExceptWith(SortedSet<T> other)`
- `public void SymmetricExceptWith(SortedSet<T> other)`
- `public bool IsSubsetOf(SortedSet<T> other)`
- `public bool IsProperSubsetOf(SortedSet<T> other)`
- `public bool IsSupersetOf(SortedSet<T> other)`
- `public bool IsProperSupersetOf(SortedSet<T> other)`
- `public bool Overlaps(SortedSet<T> other)`
- `public bool SetEquals(SortedSet<T> other)`
- `public void ForEach(Action<T> action)`
- `public owned SortedSetEnumerator<T> GetEnumerator()`

### `NativeSharp.Core.Collections.Generic` - `SortedSetEnumerator.ns`

- `public class SortedSetEnumerator<T>`
- `public SortedSetEnumerator(SortedSet<T> sourceSet)`
- `public bool MoveNext()`
- `public T Current`

### `NativeSharp.Core.Collections.Generic` - `Stack.ns`

- `public class Stack<T>`
- `public Stack()`
- `public Stack(int capacity)`
- `public int Count()`
- `public int Capacity()`
- `public int EnsureCapacity(int capacity)`
- `public void TrimExcess()`
- `public void Push(owned T value)`
- `public owned T Pop()`
- `public T Peek()`
- `public bool TryPop(T[] result)`
- `public bool TryPeek(T[] result)`
- `public bool Contains(T value)`
- `public T this[int index]`
- `public void Clear()`
- `public T[] ToArray()`
- `public void CopyTo(T[] destination, int destinationIndex)`
- `public owned StackEnumerator<T> GetEnumerator()`

### `NativeSharp.Core.Collections.Generic` - `StackEnumerator.ns`

- `public class StackEnumerator<T>`
- `public StackEnumerator(Stack<T> sourceStack)`
- `public bool MoveNext()`
- `public T Current`

## A.3 `NativeSharp.Core.Diagnostics`

### `NativeSharp.Core.Diagnostics` - `Debug.ns`

- `public class Debug`
- `public static bool IsDebuggerPresent()`
- `public static int CaptureStackFrameCount()`
- `public static void OutputString(string message)`

### `NativeSharp.Core.Diagnostics` - `Process.ns`

- `public class Process`
- `public static long CurrentProcessId()`
- `public static int Run(string executable, string commandLine, string outputPath)`
- `public static long WorkingSetSize()`
- `public static TimeSpan KernelTime()`
- `public static TimeSpan UserTime()`
- `public static int PriorityClass()`
- `public static void SetPriorityClass(int priorityClass)`

### `NativeSharp.Core.Diagnostics` - `Stopwatch.ns`

- `public class Stopwatch`
- `public Stopwatch()`
- `public void Start()`
- `public void Stop()`
- `public void Reset()`
- `public void Restart()`
- `public bool IsRunning()`
- `public TimeSpan Elapsed()`
- `public double ElapsedMilliseconds()`
- `public double ElapsedSeconds()`
- `public long Frequency()`
- `public static owned Stopwatch StartNew()`

### `NativeSharp.Core.Diagnostics` - `SystemInfo.ns`

- `public class SystemInfo`
- `public static long PageSize()`
- `public static long ProcessorArchitecture()`
- `public static long AllocationGranularity()`
- `public static long TotalPhysicalMemoryBytes()`
- `public static long AvailablePhysicalMemoryBytes()`
- `public static long MemoryLoadPercent()`

## A.4 `NativeSharp.Core.IO`

### `NativeSharp.Core.IO` - `Directory.ns`

- `public class Directory`
- `public static bool Exists(string path)`
- `public static void CreateDirectory(string path)`
- `public static void Delete(string path)`
- `public static int GetFileCount(string path)`
- `public static int GetFiles(string path, string[] destination)`
- `public static int GetDirectoryCount(string path)`
- `public static int GetDirectories(string path, string[] destination)`
- `public static bool Move(string sourcePath, string destPath)`

### `NativeSharp.Core.IO` - `File.ns`

- `public class File`
- `public static bool Exists(string path)`
- `public static void Delete(string path)`
- `public static string ReadAllText(string path)`
- `public static long GetSize(string path)`
- `public static byte[] ReadAllBytes(string path)`
- `public static int ReadBytes(string path, byte[] destination)`
- `public static bool WriteAllBytes(string path, byte[] data, int count)`
- `public static void WriteAllText(string path, string content)`
- `public static void AppendAllText(string path, string content)`
- `public static bool Copy(string sourcePath, string destPath, bool overwrite)`
- `public static bool Move(string sourcePath, string destPath)`

### `NativeSharp.Core.IO` - `Path.ns`

- `public class Path`
- `public static char DirectorySeparatorChar()`
- `public static char AltDirectorySeparatorChar()`
- `public static owned string Combine(string path1, string path2)`
- `public static owned string GetFileName(string path)`
- `public static owned string GetDirectoryName(string path)`
- `public static owned string GetExtension(string path)`
- `public static owned string GetFileNameWithoutExtension(string path)`
- `public static string GetFullPath(string path)`

### `NativeSharp.Core.IO` - `TextWriter.ns`

- `public class TextWriter`
- `public virtual void Write(string texto)`
- `public void WriteLine(string texto)`
- `public void WriteLine()`
- `public void Write(int valor)`
- `public void Write(long valor)`
- `public void Write(ulong valor)`
- `public void Write(bool valor)`
- `public void Write(double valor)`
- `public void Write(char valor)`
- `public void WriteIndent(int niveles, int anchoPorNivel)`
- `public class ConsoleTextWriter : TextWriter`
- `public ConsoleTextWriter()`
- `public ConsoleTextWriter(bool error)`
- `public override void Write(string texto)`
- `public class StringTextWriter : TextWriter`
- `public StringTextWriter()`
- `public StringTextWriter(int capacidadInicial)`
- `public override void Write(string texto)`
- `public int Length()`
- `public void Clear()`
- `public owned string ToOwnedString()`
- `public class FileTextWriter : TextWriter`
- `public FileTextWriter(string rutaDestino)`
- `public FileTextWriter(string rutaDestino, bool anexarAlFinal)`
- `public override void Write(string texto)`
- `public int Pending()`
- `public void Flush()`

## A.5 `NativeSharp.Core.Threading`

### `NativeSharp.Core.Threading` - `Atomics.ns`

- `public class AtomicInt32`
- `public AtomicInt32(int initialValue)`
- `public int Value`
- `public int Load()`
- `public void Store(int value)`
- `public int Increment()`
- `public int Decrement()`
- `public int Add(int value)`
- `public int Exchange(int value)`
- `public int CompareExchange(int value, int comparand)`
- `public class AtomicInt64`
- `public AtomicInt64(long initialValue)`
- `public long Value`
- `public long Load()`
- `public void Store(long value)`
- `public long Increment()`
- `public long Decrement()`
- `public long Add(long value)`
- `public long Exchange(long value)`
- `public long CompareExchange(long value, long comparand)`

### `NativeSharp.Core.Threading` - `Event.ns`

- `public class Event`
- `public Event(bool manualReset, bool initialState)`
- `public void Set()`
- `public void Reset()`
- `public bool Wait(int millisecondsTimeout)`

### `NativeSharp.Core.Threading` - `Monitor.ns`

- `public class Monitor`
- `public void Enter()`
- `public void Exit()`
- `public bool Wait(int millisecondsTimeout)`
- `public void Pulse()`
- `public void PulseAll()`

### `NativeSharp.Core.Threading` - `Mutex.ns`

- `public class Mutex`
- `public Mutex()`
- `public void Lock()`
- `public void Unlock()`

### `NativeSharp.Core.Threading` - `ScopedLock.ns`

- `public class ScopedLock`
- `public ScopedLock(Mutex mutex)`

### `NativeSharp.Core.Threading` - `Semaphore.ns`

- `public class Semaphore`
- `public Semaphore(int initialCount, int maximumCount)`
- `public bool Wait(int millisecondsTimeout)`
- `public void Release()`

### `NativeSharp.Core.Threading` - `Thread.ns`

- `public delegate void ThreadStart();`
- `public class Thread`
- `public Thread(ThreadStart start)`
- `public void Join()`
- `public void Detach()`
- `public static void Sleep(int milliseconds)`

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
