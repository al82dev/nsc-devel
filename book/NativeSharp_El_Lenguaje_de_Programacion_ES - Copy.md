---
title: "NativeSharp: el lenguaje de programación"
subtitle: "La referencia absoluta"
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

### Primera edición en español

**Lenguaje NativeSharp 0.1**  
**Compilador `nsc` 0.9.0-alpha, Stage 57**  
**Plataforma de referencia: Windows x64, ABI de Microsoft, objetos COFF y ejecutables PE32+**

> **Contrato de esta edición.** Todo ejemplo de proyecto incluido en el apéndice de estudios de
> caso fue compilado y ejecutado de nuevo el 6 de septiembre de 2026 con la build
> `0.9.0-alpha+build.20260906T204233Z.c7f7fa9da221841c`, tanto en `-O0` como en `-O2`. No se
> presupuso que los ejemplos antiguos seguían siendo correctos: se comprobaron contra el
> compilador y las bibliotecas actuales. La suite registrada por el repositorio contiene 297
> pruebas.

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
  están incluidos, archivo por archivo, en la Parte IX.
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

## Parte IX. Estudios de caso completos

34. [Cómo leer los proyectos editoriales](#34-cómo-leer-los-proyectos-editoriales)
35. [Los 24 programas completos](#35-los-24-programas-completos)

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
presenta su diseño; el estudio de racionales y el sistema de nómina de la Parte IX muestran la
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
            get { return id; }
        }

        public long BalanceInCents
        {
            get { return balanceInCents; }
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
    if (left.Priority < right.Priority) { return -1; }
    if (left.Priority > right.Priority) { return 1; }
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
extern export C
true false null this base
virtual override abstract sealed
get set operator implicit explicit is as
delegate event raii move owned borrowed var foreach in
```

`ref`, `out`, `where`, `from`, `pack`, `align`, `at` y otros vocablos de construcciones concretas
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

# Parte IX. Estudios de caso completos

## 34. Cómo leer los proyectos editoriales

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

Los listados se insertan exactamente desde los archivos que fueron validados. Esto conserva una
relación auditable entre libro y código. Su formato refleja el fuente real; una edición posterior
puede aplicar un formatter de NativeSharp cuando exista uno certificado.

## 35. Los 24 programas completos

### 35.1 Números racionales

**Problema.** Construir un tipo numérico que preserve la fracción exacta, normalice signos y reduzca cada resultado mediante el máximo común divisor.

**Diseño.** La clase concentra una invariante algebraica, sobrecarga aritmética y comparaciones, convierte a texto y usa una excepción específica para el denominador cero. Dos series ejercitan el tipo fuera de una demostración trivial.

**Auditoría propuesta.** Compruebe que el denominador siempre queda positivo, que cero se normaliza, que cada resultado temporal tiene un dueño y que dividir por un racional cero no produce un valor silencioso.

**Escala real.** 3 archivos NativeSharp, 344 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

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
            get { return numerator; }
        }

        public int Denominator
        {
            get { return denominator; }
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
                if (right == 0) { return 1; }
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
            return new Rational(
                left.numerator * right.numerator,
                left.denominator * right.denominator);
        }

        public static Rational operator /(Rational left, Rational right)
        {
            if (right.numerator == 0)
            {
                ZeroDenominatorException problem = new ZeroDenominatorException();
                problem.Message = String.Copy("Division by the rational value zero is undefined.");
                throw problem;
            }

            return new Rational(
                left.numerator * right.denominator,
                left.denominator * right.numerator);
        }

        public static bool operator ==(Rational left, Rational right)
        {
            return left.numerator == right.numerator && left.denominator == right.denominator;
        }

        public static bool operator !=(Rational left, Rational right)
        {
            return !(left == right);
        }

        public static bool operator <(Rational left, Rational right)
        {
            return left.numerator * right.denominator < right.numerator * left.denominator;
        }

        public static bool operator >(Rational left, Rational right)
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
        private static void PrintBinaryOperation(
            string caption,
            Rational left,
            string symbol,
            Rational right,
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
            if (a < b) { Console.WriteLine("  2/3 < 5/7"); }
            if (sum > a) { Console.WriteLine("  (2/3 + 5/7) > 2/3"); }

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

### 35.2 Nómina polimórfica

**Problema.** Calcular el pago semanal de empleados asalariados, por hora, por comisión y por comisión con salario base sin convertir el reporte en una cadena de pruebas de tipo.

**Diseño.** Una clase abstracta define el contrato y cuatro clases concretas implementan la fórmula. `Money` separa centavos de formato; el reporte consume un array heterogéneo mediante despacho virtual.

**Auditoría propuesta.** Siga la destrucción de empleados, resultados `Money` y strings devueltos como `owned`. Verifique también que las reglas de horas extra y comisión permanezcan en la clase que posee la política.

**Escala real.** 8 archivos NativeSharp, 334 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

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

        public BasePlusCommissionEmployee(
            int id,
            string employeeName,
            int weeklySales,
            int percentage,
            int basePay)
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

        public CommissionEmployee(
            int id,
            string employeeName,
            int weeklySales,
            int percentage)
            : base(id, employeeName)
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
            get { return employeeId; }
        }

        public string Name
        {
            get { return name; }
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

        public HourlyEmployee(int id, string employeeName, int hourlyRate, int hours)
            : base(id, employeeName)
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
            get { return cents; }
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

        public static bool operator <(Money left, Money right)
        {
            return left.cents < right.cents;
        }

        public static bool operator >(Money left, Money right)
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

        public SalariedEmployee(int id, string employeeName, int weeklySalary)
            : base(id, employeeName)
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

### 35.3 Búsqueda sobre objetos

**Problema.** Comparar búsqueda lineal, binaria, binaria recursiva y exponencial sobre un conjunto de estudiantes, midiendo comparaciones y precondiciones.

**Diseño.** Los algoritmos comparten un modelo de datos y producen un reporte uniforme. La búsqueda binaria no se presenta aislada: se muestra la necesidad de ordenar y se contrasta el costo observado.

**Auditoría propuesta.** Pruebe claves ausentes, primer y último elemento y un conjunto vacío. Revise qué algoritmos exigen orden y cuáles conservan el arreglo.

**Escala real.** 10 archivos NativeSharp, 404 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

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

        private int BinarySearchRange(
            Student[] data,
            int targetId,
            int low,
            int high,
            SearchStatistics statistics)
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

        private int SearchRange(
            Student[] data,
            int targetId,
            int low,
            int high,
            SearchStatistics statistics)
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
        public static void Run(
            StudentSearcher searcher,
            Student[] data,
            int count,
            int targetId,
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

        public int Comparisons { get { return comparisons; } }
        public int Probes { get { return probes; } }

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

        public int Id { get { return id; } }
        public string Name { get { return name; } }
        public int Grade { get { return grade; } }

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

### 35.4 Laboratorio de ordenamiento

**Problema.** Comparar siete algoritmos de ordenamiento sobre objetos y dos criterios sin duplicar cada algoritmo por criterio.

**Diseño.** Una jerarquía de sorters separa la estrategia; otra jerarquía separa la comparación. El reporte reconstruye el dataset, cuenta comparaciones, swaps y movimientos, y certifica que la salida está ordenada.

**Auditoría propuesta.** Observe estabilidad, complejidad, memoria auxiliar y peor caso. Audite que cada corrida trabaje sobre una copia equivalente y destruya tanto dataset como estrategias.

**Escala real.** 14 archivos NativeSharp, 647 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

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
        public override owned string Name() { return String.Copy("Bubble Sort"); }

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
        public override owned string Name() { return String.Copy("Heap Sort"); }

        private void SiftDown(
            Student[] data,
            int root,
            int count,
            StudentComparer comparer,
            SortStatistics statistics)
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
        public override owned string Name() { return String.Copy("Insertion Sort"); }

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
        public override owned string Name() { return String.Copy("Merge Sort"); }

        private void Merge(
            Student[] data,
            Student[] buffer,
            int left,
            int middle,
            int right,
            StudentComparer comparer,
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

        private void SortRange(
            Student[] data,
            Student[] buffer,
            int left,
            int right,
            StudentComparer comparer,
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
        public override owned string Name() { return String.Copy("Quick Sort"); }

        private int Partition(
            Student[] data,
            int low,
            int high,
            StudentComparer comparer,
            SortStatistics statistics)
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

        private void SortRange(
            Student[] data,
            int low,
            int high,
            StudentComparer comparer,
            SortStatistics statistics)
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
        public override owned string Name() { return String.Copy("Selection Sort"); }

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
        public override owned string Name() { return String.Copy("Shell Sort"); }

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

        private static void PrintSummary(
            StudentSorter sorter,
            StudentComparer comparer,
            Student[] data,
            int count,
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

        public int Comparisons { get { return comparisons; } }
        public int Swaps { get { return swaps; } }
        public int Moves { get { return moves; } }

        public void Reset()
        {
            comparisons = 0;
            swaps = 0;
            moves = 0;
        }

        public void Comparison() { comparisons = comparisons + 1; }
        public void Swap() { swaps = swaps + 1; }
        public void Move() { moves = moves + 1; }
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

        public int Id { get { return id; } }
        public string Name { get { return name; } }
        public int Grade { get { return grade; } }

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
            if (left.Id < right.Id) { return -1; }
            if (left.Id > right.Id) { return 1; }
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
            if (left.Grade < right.Grade) { return -1; }
            if (left.Grade > right.Grade) { return 1; }
            if (left.Id < right.Id) { return -1; }
            if (left.Id > right.Id) { return 1; }
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
        public static int Count() { return 12; }

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

### 35.5 Lista simplemente enlazada genérica

**Problema.** Implementar inserción, búsqueda, eliminación y recorrido sin delegar la invariante de enlaces a una colección existente.

**Diseño.** La lista genérica separa nodo y contenedor. El contenedor conserva cabeza, cola y cantidad; las operaciones actualizan los tres de manera coordinada.

**Auditoría propuesta.** Dibuje los enlaces antes y después de quitar cabeza, cola y nodo medio. Compruebe que `Clear` destruye cada nodo una sola vez y deja el objeto reutilizable.

**Escala real.** 4 archivos NativeSharp, 331 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

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

        public int Count { get { return count; } }
        public bool IsEmpty { get { return count == 0; } }

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

### 35.6 Pila y evaluador de expresiones

**Problema.** Evaluar expresiones infijas con precedencia, paréntesis, números de varios dígitos y recuperación controlada ante entrada inválida.

**Diseño.** Una pila genérica redimensionable sostiene valores y operadores. El evaluador convierte la gramática operacional en código, reduce según precedencia y lanza una excepción de dominio con estado de error.

**Auditoría propuesta.** Trace una expresión carácter por carácter. Pruebe paréntesis desbalanceados, división por cero, operador sin operando y una expresión vacía.

**Escala real.** 4 archivos NativeSharp, 355 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

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

        public int Count { get { return count; } }
        public bool IsEmpty { get { return count == 0; } }

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

### 35.7 Simulación de una cola de servicio

**Problema.** Modelar llegadas, espera, atención y utilización de un escritorio durante una secuencia discreta de ticks.

**Diseño.** Una cola circular evita desplazar elementos. La simulación separa clientes, dataset y estadísticas para que la política pueda cambiar sin alterar la estructura.

**Auditoría propuesta.** Compruebe wrap-around, cola llena, tick sin cliente y cierre con trabajo pendiente. Distinga tiempo medio de espera de utilización del servidor.

**Escala real.** 6 archivos NativeSharp, 347 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

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
            if (capacity < 2) { capacity = 2; }
            items = new T[capacity];
            head = 0;
            tail = 0;
            count = 0;
        }

        public int Count { get { return count; } }
        public bool IsEmpty { get { return count == 0; } }

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

        public int Id { get { return id; } }
        public int ArrivalTime { get { return arrivalTime; } }
        public int ServiceDuration { get { return serviceDuration; } }
        public int ServiceStart { get { return serviceStart; } }
        public int ServiceFinish { get { return serviceFinish; } }
        public int WaitingTime { get { return serviceStart - arrivalTime; } }

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
        public static int Count() { return 12; }

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

        public int TotalWaitingTime { get { return totalWaitingTime; } }
        public int MaximumWaitingTime { get { return maximumWaitingTime; } }
        public int MaximumQueueLength { get { return maximumQueueLength; } }
        public int IdleMinutes { get { return idleMinutes; } }
        public int ServedCustomers { get { return servedCustomers; } }

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

### 35.8 Árbol binario de búsqueda

**Problema.** Mantener estudiantes ordenados por clave con inserción, búsqueda, recorridos y eliminación de los tres casos estructurales.

**Diseño.** El árbol encapsula la raíz y el tamaño; los nodos mantienen enlaces. La eliminación sustituye mediante sucesor cuando hay dos hijos y conserva el orden global.

**Auditoría propuesta.** Valide duplicados, hoja, nodo con un hijo y raíz con dos. Después de cada operación, el recorrido inorder debe permanecer ordenado.

**Escala real.** 5 archivos NativeSharp, 447 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

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

        public int Count { get { return count; } }
        public bool IsEmpty { get { return root == null; } }

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
            if (root == null) { return null; }
            StudentNode current = root;
            while (current.Left != null)
            {
                current = current.Left;
            }
            return current.Value;
        }

        public Student Maximum()
        {
            if (root == null) { return null; }
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
            if (node == null) { return; }
            InOrderNode(node.Left);
            PrintStudent(node.Value);
            InOrderNode(node.Right);
        }

        private void PreOrderNode(StudentNode node)
        {
            if (node == null) { return; }
            PrintStudent(node.Value);
            PreOrderNode(node.Left);
            PreOrderNode(node.Right);
        }

        private void PostOrderNode(StudentNode node)
        {
            if (node == null) { return; }
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
            if (node == null) { return; }
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
            if (node == null) { return; }
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

        public int Id { get { return id; } }
        public string Name { get { return name; } }
        public int Grade { get { return grade; } }

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

### 35.9 Lista doblemente enlazada

**Problema.** Obtener recorrido en ambos sentidos e inserciones y eliminaciones locales con enlaces anterior y siguiente.

**Diseño.** La redundancia de enlaces reduce costo de navegación pero duplica la invariante. Cada mutación debe actualizar pares simétricos y extremos.

**Auditoría propuesta.** Revise las cuatro fronteras: vacía, un nodo, cabeza y cola. Una mitad actualizada produce corrupción que puede no aparecer hasta recorrer en sentido contrario.

**Escala real.** 4 archivos NativeSharp, 323 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

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

        public int Count { get { return count; } }
        public bool IsEmpty { get { return count == 0; } }
        public DoublyNode<T> First { get { return head; } }
        public DoublyNode<T> Last { get { return tail; } }

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

### 35.10 Lista circular y problema de Josephus

**Problema.** Resolver eliminación periódica sobre un círculo sin transformar el problema en índices de un array.

**Diseño.** La estructura hace que el sucesor de la cola sea la cabeza. El solver consume nodos siguiendo la semántica del círculo y deja visible el orden de eliminación.

**Auditoría propuesta.** Pruebe paso uno, un participante y pasos mayores que el tamaño. Compruebe que el último enlace no retenga un ciclo después de limpiar.

**Escala real.** 4 archivos NativeSharp, 194 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

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

        public int Count { get { return count; } }
        public CircleNode First { get { return head; } }

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

### 35.11 Deque y análisis de palíndromos

**Problema.** Usar una estructura de doble extremo tanto como cola de trabajo como para comparar caracteres simétricos.

**Diseño.** El deque se implementa sobre array circular y comparte la aritmética de índices entre frente y fondo. El analizador es un consumidor que no conoce el almacenamiento.

**Auditoría propuesta.** Audite wrap-around, crecimiento, longitud impar, texto vacío y normalización. Un deque correcto debe conservar orden tras combinaciones de ambos extremos.

**Escala real.** 4 archivos NativeSharp, 218 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

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
            if (capacity < 2) { capacity = 2; }
            items = new T[capacity];
            head = 0;
            count = 0;
        }

        public int Count { get { return count; } }
        public bool IsEmpty { get { return count == 0; } }

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
            if (count == items.Length) { Grow(); }
            items[Index(count)] = value;
            count = count + 1;
        }

        public void AddFront(T value)
        {
            if (count == items.Length) { Grow(); }
            head = head - 1;
            if (head < 0) { head = items.Length - 1; }
            items[head] = value;
            count = count + 1;
        }

        public void RemoveFront(T[] result)
        {
            if (count == 0) { throw new InvalidOperationException(); }
            result[0] = items[head];
            head = (head + 1) % items.Length;
            count = count - 1;
            if (count == 0) { head = 0; }
        }

        public void RemoveBack(T[] result)
        {
            if (count == 0) { throw new InvalidOperationException(); }
            int index = Index(count - 1);
            result[0] = items[index];
            count = count - 1;
            if (count == 0) { head = 0; }
        }

        public T PeekFront()
        {
            if (count == 0) { throw new InvalidOperationException(); }
            return items[head];
        }

        public T PeekBack()
        {
            if (count == 0) { throw new InvalidOperationException(); }
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

### 35.12 Árbol AVL

**Problema.** Conservar búsqueda logarítmica mediante alturas y rotaciones después de insertar y eliminar.

**Diseño.** El árbol implementa rotaciones izquierda, derecha y dobles, recalcula alturas y expone una validación de balance. El programa muestra la estructura lateralmente tras operaciones reales.

**Auditoría propuesta.** Fuerce los cuatro patrones de rotación. Verifique simultáneamente orden BST, alturas almacenadas y factor de balance; comprobar sólo uno puede ocultar el bug.

**Escala real.** 4 archivos NativeSharp, 327 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

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

        public AvlTree() { root = null; count = 0; }
        public int Count { get { return count; } }
        public int Height { get { return NodeHeight(root); } }

        private int NodeHeight(AvlNode node)
        {
            if (node == null) { return 0; }
            return node.Height;
        }

        private int Max(int a, int b)
        {
            if (a > b) { return a; }
            return b;
        }

        private void UpdateHeight(AvlNode node)
        {
            node.Height = 1 + Max(NodeHeight(node.Left), NodeHeight(node.Right));
        }

        private int BalanceFactor(AvlNode node)
        {
            if (node == null) { return 0; }
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
            if (node == null) { return null; }
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
            if (result) { count = count + 1; }
            else { delete value; }
            return result;
        }

        public AvlRecord Find(int key)
        {
            AvlNode current = root;
            while (current != null)
            {
                if (key == current.Value.Key) { return current.Value; }
                if (key < current.Value.Key) { current = current.Left; }
                else { current = current.Right; }
            }
            return null;
        }

        private AvlNode MinimumNode(AvlNode node)
        {
            AvlNode current = node;
            while (current.Left != null) { current = current.Left; }
            return current;
        }

        private AvlNode RemoveNode(AvlNode node, int key, bool[] removed)
        {
            if (node == null) { return null; }
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
                    if (child == null) { child = node.Right; }
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
            if (result) { count = count - 1; }
            return result;
        }

        private void PrintNode(AvlNode node, int depth)
        {
            if (node == null) { return; }
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

        public void PrintSideways() { PrintNode(root, 0); }

        private bool ValidateNode(AvlNode node)
        {
            if (node == null) { return true; }
            int balance = BalanceFactor(node);
            if (balance < -1 || balance > 1) { return false; }
            int expected = 1 + Max(NodeHeight(node.Left), NodeHeight(node.Right));
            if (node.Height != expected) { return false; }
            return ValidateNode(node.Left) && ValidateNode(node.Right);
        }

        public bool IsBalanced() { return ValidateNode(root); }

        private void ClearNode(AvlNode node)
        {
            if (node == null) { return; }
            ClearNode(node.Left);
            ClearNode(node.Right);
            node.Left = null;
            node.Right = null;
            delete node.Value;
            delete node;
        }

        public void Clear() { ClearNode(root); root = null; count = 0; }
        ~AvlTree() { Clear(); }
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
            if (tree.IsBalanced()) { raii string yesBalanced = String.Copy("yes"); balanced = move(yesBalanced); }
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
            if (tree.IsBalanced()) { raii string yesAfter = String.Copy("yes"); after = move(yesAfter); }
            raii string afterLine = String.Concat("balanced after deletion: ", after);
            Console.WriteLine(afterLine);
            return 0;
        }
    }
}
```

### 35.13 Heap binario y planificador por prioridad

**Problema.** Elegir repetidamente el trabajo más prioritario sin ordenar toda la colección después de cada inserción.

**Diseño.** La cola prioritaria usa heap en array y operaciones sift-up/sift-down. El planificador demuestra que la estructura conserva una política útil, no sólo enteros abstractos.

**Auditoría propuesta.** Pruebe prioridades iguales, crecimiento y extracción hasta vacío. La invariante local padre-hijo debe mantenerse después de cada swap.

**Escala real.** 4 archivos NativeSharp, 213 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

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

        public static bool operator >(Job a, Job b)
        {
            if (a.Priority != b.Priority) { return a.Priority > b.Priority; }
            return a.Sequence < b.Sequence;
        }
        public static bool operator <(Job a, Job b) { return b > a; }
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
            if (capacity < 2) { capacity = 2; }
            heap = new Job[capacity];
            count = 0;
        }
        public int Count { get { return count; } }
        public bool IsEmpty { get { return count == 0; } }

        private void Grow()
        {
            Job[] grown = new Job[heap.Length * 2];
            int i = 0;
            while (i < count) { grown[i] = heap[i]; i = i + 1; }
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
                if (!(heap[child] > heap[parent])) { return; }
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
                if (left < count && heap[left] > heap[best]) { best = left; }
                if (right < count && heap[right] > heap[best]) { best = right; }
                if (best == parent) { done = true; }
                else { Swap(parent, best); parent = best; }
            }
        }

        public void Enqueue(Job job)
        {
            if (count == heap.Length) { Grow(); }
            heap[count] = job;
            SiftUp(count);
            count = count + 1;
        }

        public Job Peek()
        {
            if (count == 0) { throw new InvalidOperationException(); }
            return heap[0];
        }

        public Job Dequeue()
        {
            if (count == 0) { throw new InvalidOperationException(); }
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
                if (heap[i] != null) { delete heap[i]; }
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
        public void PrintHeap() { queue.PrintHeapArray(); }
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
        ~Scheduler() { delete queue; }
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

### 35.14 Trie de diccionario

**Problema.** Guardar palabras por prefijo, consultar pertenencia y enumerar coincidencias sin comparar cada palabra completa.

**Diseño.** Cada nivel representa un carácter y los caminos comparten prefijos. El dataset y la impresión permiten observar nodos internos que no son palabras completas.

**Auditoría propuesta.** Pruebe una palabra que es prefijo de otra, prefijo inexistente y caracteres de borde. La limpieza debe recorrer todo el árbol de hijos.

**Escala real.** 4 archivos NativeSharp, 231 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

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
        public int Count { get { return count; } }

        private int IndexOf(char c)
        {
            if (c >= 'A' && c <= 'Z') { c = (char)((int)c + 32); }
            if (c < 'a' || c > 'z') { return -1; }
            return (int)c - (int)'a';
        }

        public bool Insert(string word)
        {
            TrieNode current = root;
            int i = 0;
            while (i < word.Length)
            {
                int index = IndexOf(word[i]);
                if (index < 0) { throw new ArgumentException(); }
                if (current.Children[index] == null)
                {
                    current.Children[index] = new TrieNode();
                }
                current = current.Children[index];
                current.PrefixCount = current.PrefixCount + 1;
                i = i + 1;
            }
            if (current.Terminal) { return false; }
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
                if (index < 0) { return null; }
                current = current.Children[index];
                if (current == null) { return null; }
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
            if (node == null) { return 0; }
            if (prefix.Length == 0) { return count; }
            return node.PrefixCount;
        }

        private void PrintWordsNode(TrieNode node, string prefix, int[] printed, int limit)
        {
            if (node == null || printed[0] >= limit) { return; }
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
            if (node == null) { return; }
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
        ~TrieNode() { delete[] Children; }
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
            if (found) { raii string foundState = String.Copy("found"); state = move(foundState); }
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

### 35.15 Tabla hash de símbolos

**Problema.** Resolver nombres con colisiones controladas y crecimiento sin confundir hash con identidad.

**Diseño.** La tabla distribuye símbolos en buckets enlazados. La igualdad decide identidad después del hash; inserción, lookup y eliminación conservan cadenas.

**Auditoría propuesta.** Fuerce colisiones, actualizaciones y borrado en cabeza y medio de bucket. Una función hash pobre debe degradar rendimiento, no corrección.

**Escala real.** 4 archivos NativeSharp, 233 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

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
            if (bucketCount < 3) { bucketCount = 3; }
            buckets = new BucketNode[bucketCount];
            count = 0;
            collisions = 0;
        }
        public int Count { get { return count; } }
        public int Collisions { get { return collisions; } }
        public int BucketCount { get { return buckets.Length; } }

        public int Hash(string key)
        {
            long hash = 5381;
            int i = 0;
            while (i < key.Length)
            {
                hash = ((hash * 33) + (int)key[i]) % 2147483647;
                i = i + 1;
            }
            if (hash < 0) { hash = -hash; }
            return (int)(hash % buckets.Length);
        }

        public bool Add(Symbol symbol)
        {
            int index = Hash(symbol.Name);
            BucketNode current = buckets[index];
            if (current != null) { collisions = collisions + 1; }
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
                if (String.Equals(current.Value.Name, name)) { return current.Value; }
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
                    if (previous == null) { buckets[index] = current.Next; }
                    else { previous.Next = current.Next; }
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
            while (i < buckets.Length) { ClearBucket(i); i = i + 1; }
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

### 35.16 Grafo, BFS y DFS

**Problema.** Representar ciudades y conexiones, recorrer por amplitud y profundidad y reconstruir un camino no ponderado.

**Diseño.** Una matriz de adyacencia hace explícito el costo espacial. BFS usa cola y predecesores; DFS se muestra iterativo con pila y recursivo.

**Auditoría propuesta.** Compare órdenes, pruebe vértices desconectados y reconstruya el camino desde predecesores. Distinga visitar de descubrir para no insertar duplicados sin límite.

**Escala real.** 5 archivos NativeSharp, 315 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

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
        public City(int id, string name) { Id = id; Name = name; }
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
        public int VertexCount { get { return vertexCount; } }

        private int Cell(int from, int to) { return from * capacity + to; }

        public int AddCity(string name)
        {
            if (vertexCount == capacity) { throw new InvalidOperationException(); }
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

        public City GetCity(int id) { return vertices[id]; }
        public bool Connected(int a, int b) { return adjacency[Cell(a, b)] != 0; }

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
                        if (Connected(v, n) && !visited[n]) { stack.Push(n); }
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
                if (Connected(v, n) && !visited[n]) { DepthFirstNode(n, visited); }
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
            while (i < vertexCount) { previous[i] = -1; i = i + 1; }
            raii IntQueue queue = new IntQueue(4);
            visited[start] = true;
            queue.Enqueue(start);
            while (!queue.IsEmpty)
            {
                int v = queue.Dequeue();
                if (v == goal) { break; }
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
            while (i < vertexCount) { delete vertices[i]; i = i + 1; }
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
        public IntQueue(int capacity) { data = new int[capacity]; head = 0; count = 0; }
        public bool IsEmpty { get { return count == 0; } }
        private int Index(int logical) { return (head + logical) % data.Length; }
        private void Grow()
        {
            int[] grown = new int[data.Length * 2];
            int i = 0; while (i < count) { grown[i] = data[Index(i)]; i = i + 1; }
            delete[] data; data = grown; head = 0;
        }
        public void Enqueue(int value)
        {
            if (count == data.Length) { Grow(); }
            data[Index(count)] = value; count = count + 1;
        }
        public int Dequeue()
        {
            if (count == 0) { throw new InvalidOperationException(); }
            int value = data[head]; head = (head + 1) % data.Length; count = count - 1; return value;
        }
        ~IntQueue() { delete[] data; }
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
        public IntStack(int capacity) { data = new int[capacity]; count = 0; }
        public bool IsEmpty { get { return count == 0; } }
        private void Grow()
        {
            int[] grown = new int[data.Length * 2];
            int i = 0; while (i < count) { grown[i] = data[i]; i = i + 1; }
            delete[] data; data = grown;
        }
        public void Push(int value) { if (count == data.Length) { Grow(); } data[count] = value; count = count + 1; }
        public int Pop() { if (count == 0) { throw new InvalidOperationException(); } count = count - 1; return data[count]; }
        ~IntStack() { delete[] data; }
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

### 35.17 Caminos mínimos con Dijkstra

**Problema.** Encontrar el costo mínimo entre ciudades con pesos no negativos y reconstruir la ruta.

**Diseño.** El solver mantiene distancia tentativa, conjunto visitado y predecesor. La selección lineal del mínimo prioriza claridad; después puede sustituirse por heap.

**Auditoría propuesta.** Rechace conceptualmente pesos negativos, proteja el infinito contra suma y pruebe destino inalcanzable. La ruta se reconstruye sólo si existe distancia finita.

**Escala real.** 4 archivos NativeSharp, 207 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

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
                if (current < 0) { break; }
                visited[current] = true;

                raii string d = String.ToString(distances[current]);
                raii string a = String.Concat("settle ", graph.City(current).Name);
                raii string b = String.Concat(a, " at distance ");
                raii string line = String.Concat(b, d);
                Console.WriteLine(line);

                if (current == goal) { break; }

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
        public WeightedCity(int id, string name) { Id = id; Name = name; }
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

        public int Count { get { return count; } }
        private int Cell(int a, int b) { return a * capacity + b; }

        public int AddCity(string name)
        {
            if (count == capacity) { throw new InvalidOperationException(); }
            int id = count;
            cities[id] = new WeightedCity(id, name);
            count = count + 1;
            return id;
        }

        public void AddUndirectedEdge(int a, int b, int weight)
        {
            if (weight <= 0) { throw new ArgumentOutOfRangeException(); }
            weights[Cell(a,b)] = weight;
            weights[Cell(b,a)] = weight;
        }

        public int Weight(int a, int b) { return weights[Cell(a,b)]; }
        public WeightedCity City(int id) { return cities[id]; }

        ~WeightedGraph()
        {
            int i = 0;
            while (i < count) { delete cities[i]; i = i + 1; }
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
            graph.AddUndirectedEdge(a,b,4);
            graph.AddUndirectedEdge(a,c,2);
            graph.AddUndirectedEdge(b,c,1);
            graph.AddUndirectedEdge(b,d,5);
            graph.AddUndirectedEdge(c,d,8);
            graph.AddUndirectedEdge(c,e,10);
            graph.AddUndirectedEdge(d,e,2);
            graph.AddUndirectedEdge(d,f,6);
            graph.AddUndirectedEdge(e,f,3);
            graph.AddUndirectedEdge(e,g,5);
            graph.AddUndirectedEdge(f,h,4);
            graph.AddUndirectedEdge(g,h,2);
            DijkstraSolver.SolveAndPrint(graph, a, h);
            return 0;
        }
    }
}
```

### 35.18 Orden topológico

**Problema.** Programar tareas con dependencias dirigidas y detectar cuándo un ciclo impide un orden válido.

**Diseño.** El algoritmo de Kahn calcula grados de entrada, encola tareas disponibles y reduce dependencias. El resultado no es necesariamente único.

**Auditoría propuesta.** Pruebe dos órdenes válidos, grafo vacío y ciclo. Producir menos tareas que el total es evidencia de ciclo, no un resultado parcial exitoso.

**Escala real.** 4 archivos NativeSharp, 120 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

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
        public BuildTask(int id,string name) { Id=id; Name=name; }
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
            capacity=maxTasks; tasks=new BuildTask[maxTasks]; edges=new int[maxTasks*maxTasks]; count=0;
        }
        private int Cell(int a,int b){return a*capacity+b;}
        public int AddTask(string name){int id=count; tasks[id]=new BuildTask(id,name); count=count+1; return id;}
        public void AddDependency(int prerequisite,int dependent){edges[Cell(prerequisite,dependent)]=1;}
        public void TopologicalOrder()
        {
            int[] indegree=new int[count];
            int a=0;
            while(a<count)
            {
                int b=0;
                while(b<count)
                {
                    if(edges[Cell(a,b)]!=0){indegree[b]=indegree[b]+1;}
                    b=b+1;
                }
                a=a+1;
            }
            raii IntQueue ready=new IntQueue(count+1);
            int i=0;
            while(i<count){if(indegree[i]==0){ready.Enqueue(i);} i=i+1;}
            int processed=0;
            while(!ready.IsEmpty)
            {
                int task=ready.Dequeue();
                raii string number=String.ToString(processed+1);
                raii string x=String.Concat("  ",number);
                raii string y=String.Concat(x,". ");
                raii string line=String.Concat(y,tasks[task].Name);
                Console.WriteLine(line);
                processed=processed+1;
                int next=0;
                while(next<count)
                {
                    if(edges[Cell(task,next)]!=0)
                    {
                        indegree[next]=indegree[next]-1;
                        if(indegree[next]==0){ready.Enqueue(next);}
                    }
                    next=next+1;
                }
            }
            if(processed!=count)
            {
                Console.WriteLine("cycle detected: no complete topological order exists");
            }
            delete[] indegree;
        }
        ~DependencyGraph(){int i=0;while(i<count){delete tasks[i];i=i+1;}delete[] tasks;delete[] edges;}
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
        private int[] data; private int head; private int tail; private int count;
        public IntQueue(int capacity) { data=new int[capacity]; head=0; tail=0; count=0; }
        public bool IsEmpty { get { return count==0; } }
        public void Enqueue(int value) { data[tail]=value; tail=(tail+1)%data.Length; count=count+1; }
        public int Dequeue() { if(count==0){throw new InvalidOperationException();} int v=data[head]; head=(head+1)%data.Length; count=count-1; return v; }
        ~IntQueue(){delete[] data;}
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
            raii DependencyGraph graph=new DependencyGraph(10);
            int scan=graph.AddTask("scan source files");
            int lex=graph.AddTask("lex tokens");
            int parse=graph.AddTask("parse syntax tree");
            int sema=graph.AddTask("semantic analysis");
            int hir=graph.AddTask("lower to HIR");
            int mir=graph.AddTask("lower to MIR");
            int optimize=graph.AddTask("optimize MIR");
            int llvm=graph.AddTask("emit LLVM IR");
            int obj=graph.AddTask("emit COFF object");
            int link=graph.AddTask("link executable");
            graph.AddDependency(scan,lex);
            graph.AddDependency(lex,parse);
            graph.AddDependency(parse,sema);
            graph.AddDependency(sema,hir);
            graph.AddDependency(hir,mir);
            graph.AddDependency(mir,optimize);
            graph.AddDependency(optimize,llvm);
            graph.AddDependency(llvm,obj);
            graph.AddDependency(obj,link);
            graph.TopologicalOrder();
            return 0;
        }
    }
}
```

### 35.19 Kruskal y union-find

**Problema.** Construir un árbol de expansión mínimo eligiendo aristas por peso sin crear ciclos.

**Diseño.** Kruskal ordena aristas y union-find mantiene componentes con representantes. El ejemplo conecta estructura auxiliar, ordenamiento y propiedad global.

**Auditoría propuesta.** Compruebe conectividad, pesos repetidos y grafo desconectado. El resultado de un grafo desconectado es bosque, salvo que el contrato exija error.

**Escala real.** 4 archivos NativeSharp, 106 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

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
        private int[] parent; private int[] rank;
        public DisjointSet(int count)
        {
            parent=new int[count]; rank=new int[count]; int i=0; while(i<count){parent[i]=i;rank[i]=0;i=i+1;}
        }
        public int Find(int value)
        {
            int root=value;
            while(parent[root]!=root){root=parent[root];}
            int current=value;
            while(parent[current]!=current){int next=parent[current];parent[current]=root;current=next;}
            return root;
        }
        public bool Union(int a,int b)
        {
            int ra=Find(a);int rb=Find(b);if(ra==rb){return false;}
            if(rank[ra]<rank[rb]){parent[ra]=rb;}
            else if(rank[ra]>rank[rb]){parent[rb]=ra;}
            else{parent[rb]=ra;rank[ra]=rank[ra]+1;}
            return true;
        }
        ~DisjointSet(){delete[] parent;delete[] rank;}
    }
}
```

##### `book_examples/19_kruskal_mst_union_find/Edge.ns`

```csharp
namespace NativeSharp.BookExamples.Kruskal
{
    public class Edge
    {
        public int From; public int To; public int Weight;
        public Edge(int from,int to,int weight){From=from;To=to;Weight=weight;}
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
        private static void SortByWeight(Edge[] edges,int count)
        {
            int i=1;
            while(i<count)
            {
                Edge current=edges[i];
                int j=i-1;
                while(j>=0 && edges[j].Weight>current.Weight)
                {
                    edges[j+1]=edges[j];
                    j=j-1;
                }
                edges[j+1]=current;
                i=i+1;
            }
        }
        public static void Solve(string[] names,int vertexCount,Edge[] edges,int edgeCount)
        {
            SortByWeight(edges,edgeCount);
            raii DisjointSet sets=new DisjointSet(vertexCount);
            int selected=0;int total=0;int i=0;
            while(i<edgeCount && selected<vertexCount-1)
            {
                Edge edge=edges[i];
                if(sets.Union(edge.From,edge.To))
                {
                    selected=selected+1;total=total+edge.Weight;
                    raii string w=String.ToString(edge.Weight);
                    raii string a=String.Concat("  ",names[edge.From]);
                    raii string b=String.Concat(a," -- ");
                    raii string c=String.Concat(b,names[edge.To]);
                    raii string d=String.Concat(c,"  weight=");
                    raii string line=String.Concat(d,w);
                    Console.WriteLine(line);
                }
                i=i+1;
            }
            raii string t=String.ToString(total);
            raii string line=String.Concat("MST total weight: ",t);
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
            string[] names=new string[6];
            names[0]="A";names[1]="B";names[2]="C";names[3]="D";names[4]="E";names[5]="F";
            Edge[] edges=new Edge[9];
            edges[0]=new Edge(0,1,4); edges[1]=new Edge(0,2,2); edges[2]=new Edge(1,2,1);
            edges[3]=new Edge(1,3,5); edges[4]=new Edge(2,3,8); edges[5]=new Edge(2,4,10);
            edges[6]=new Edge(3,4,2); edges[7]=new Edge(3,5,6); edges[8]=new Edge(4,5,3);
            KruskalSolver.Solve(names,6,edges,9);
            int i=0;while(i<9){delete edges[i];i=i+1;}
            delete[] edges;delete[] names;
            return 0;
        }
    }
}
```

### 35.20 Mochila 0/1 con programación dinámica

**Problema.** Maximizar valor bajo una capacidad cuando cada artículo puede elegirse como máximo una vez.

**Diseño.** La tabla dinámica registra subproblemas por prefijo de artículos y capacidad. La reconstrucción distingue el valor óptimo de la selección que lo alcanza.

**Auditoría propuesta.** Pruebe capacidad cero, artículo demasiado pesado y empates. Explique por qué recorrer capacidades en el orden equivocado cambia 0/1 por mochila ilimitada.

**Escala real.** 3 archivos NativeSharp, 97 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

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
        public string Name; public int Weight; public int Value;
        public Item(string name,int weight,int value){Name=name;Weight=weight;Value=value;}
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
        private static int Cell(int row,int column,int width){return row*width+column;}
        private static int Max(int a,int b){if(a>b){return a;}return b;}
        public static void Solve(Item[] items,int itemCount,int capacity)
        {
            int width=capacity+1;
            int[] dp=new int[(itemCount+1)*width];
            int i=1;
            while(i<=itemCount)
            {
                Item item=items[i-1];
                int w=0;
                while(w<=capacity)
                {
                    int without=dp[Cell(i-1,w,width)];
                    int best=without;
                    if(item.Weight<=w)
                    {
                        int with=item.Value+dp[Cell(i-1,w-item.Weight,width)];
                        best=Max(without,with);
                    }
                    dp[Cell(i,w,width)]=best;
                    w=w+1;
                }
                i=i+1;
            }
            raii string best=String.ToString(dp[Cell(itemCount,capacity,width)]);
            raii string bestLine=String.Concat("maximum value: ",best);
            Console.WriteLine(bestLine);
            Console.WriteLine("chosen items (reconstructed by walking the DP table backward):");
            int remaining=capacity;
            int row=itemCount;
            int usedWeight=0;
            while(row>0)
            {
                int current=dp[Cell(row,remaining,width)];
                int previous=dp[Cell(row-1,remaining,width)];
                if(current!=previous)
                {
                    Item chosen=items[row-1];
                    usedWeight=usedWeight+chosen.Weight;
                    raii string weight=String.ToString(chosen.Weight);
                    raii string value=String.ToString(chosen.Value);
                    raii string a=String.Concat("  ",chosen.Name);
                    raii string b=String.Concat(a,"  weight=");
                    raii string c=String.Concat(b,weight);
                    raii string d=String.Concat(c,"  value=");
                    raii string line=String.Concat(d,value);
                    Console.WriteLine(line);
                    remaining=remaining-chosen.Weight;
                }
                row=row-1;
            }
            raii string uw=String.ToString(usedWeight);
            raii string cap=String.ToString(capacity);
            raii string a=String.Concat("weight used: ",uw);
            raii string b=String.Concat(a," / ");
            raii string line=String.Concat(b,cap);
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
            Item[] items=new Item[6];
            items[0]=new Item("medical kit",4,12);
            items[1]=new Item("water",2,8);
            items[2]=new Item("radio",3,10);
            items[3]=new Item("rope",2,7);
            items[4]=new Item("tool kit",5,14);
            items[5]=new Item("flare",1,4);
            KnapsackSolver.Solve(items,6,10);
            int i=0;while(i<6){delete items[i];i=i+1;}delete[] items;
            return 0;
        }
    }
}
```

### 35.21 Subsecuencia común más larga

**Problema.** Encontrar una secuencia ordenada común a dos textos sin exigir contigüidad.

**Diseño.** La matriz dinámica compara prefijos y la reconstrucción camina decisiones. El caso muestra que obtener sólo la longitud es una especificación incompleta para muchas aplicaciones.

**Auditoría propuesta.** Pruebe texto vacío, caracteres repetidos y soluciones múltiples. No confunda subsecuencia con substring.

**Escala real.** 2 archivos NativeSharp, 70 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

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
        private static int Cell(int row,int col,int width){return row*width+col;}
        private static int Max(int a,int b){if(a>b){return a;}return b;}
        public static owned string Solve(string left,string right)
        {
            int rows=left.Length+1;int cols=right.Length+1;
            int[] dp=new int[rows*cols];
            int i=1;
            while(i<rows)
            {
                int j=1;
                while(j<cols)
                {
                    if(left[i-1]==right[j-1]){dp[Cell(i,j,cols)]=dp[Cell(i-1,j-1,cols)]+1;}
                    else{dp[Cell(i,j,cols)]=Max(dp[Cell(i-1,j,cols)],dp[Cell(i,j-1,cols)]);}
                    j=j+1;
                }
                i=i+1;
            }
            int length=dp[Cell(left.Length,right.Length,cols)];
            char[] reverse=new char[length];
            int pos=0;int r=left.Length;int c=right.Length;
            while(r>0 && c>0)
            {
                if(left[r-1]==right[c-1])
                {
                    reverse[pos]=left[r-1];pos=pos+1;r=r-1;c=c-1;
                }
                else if(dp[Cell(r-1,c,cols)]>=dp[Cell(r,c-1,cols)]){r=r-1;}
                else{c=c-1;}
            }
            raii string result=String.Copy("");
            int k=length-1;
            while(k>=0)
            {
                raii string ch=String.ToString(reverse[k]);
                raii string next=String.Concat(result,ch);
                result=move(next);
                k=k-1;
            }
            delete[] reverse;delete[] dp;
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
            raii string left=String.Copy("NATIVESHARP");
            raii string right=String.Copy("NATIVEGRAPH");
            raii string lcs=LcsSolver.Solve(left,right);
            raii string a=String.Concat("left : ",left);Console.WriteLine(a);
            raii string b=String.Concat("right: ",right);Console.WriteLine(b);
            raii string c=String.Concat("LCS  : ",lcs);Console.WriteLine(c);
            raii string len=String.ToString(lcs.Length);
            raii string d=String.Concat("length: ",len);Console.WriteLine(d);
            return 0;
        }
    }
}
```

### 35.22 Distancia de edición de Levenshtein

**Problema.** Medir el mínimo de inserciones, eliminaciones y sustituciones para transformar un texto en otro.

**Diseño.** La recurrencia llena una tabla por prefijos y usa bordes como costo de convertir desde o hacia vacío. El programa contrasta pares con resultados conocidos.

**Auditoría propuesta.** Pruebe simetría, igualdad, vacío y caracteres repetidos. Si se agregan costos distintos, documente que la métrica cambia.

**Escala real.** 2 archivos NativeSharp, 70 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

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
        private static int Cell(int r,int c,int width){return r*width+c;}
        private static int Min3(int a,int b,int c){int m=a;if(b<m){m=b;}if(c<m){m=c;}return m;}
        public static int Solve(string source,string target)
        {
            int rows=source.Length+1;int cols=target.Length+1;
            int[] dp=new int[rows*cols];
            int r=0;while(r<rows){dp[Cell(r,0,cols)]=r;r=r+1;}
            int c=0;while(c<cols){dp[Cell(0,c,cols)]=c;c=c+1;}
            r=1;
            while(r<rows)
            {
                c=1;
                while(c<cols)
                {
                    int cost=1;if(source[r-1]==target[c-1]){cost=0;}
                    int deletion=dp[Cell(r-1,c,cols)]+1;
                    int insertion=dp[Cell(r,c-1,cols)]+1;
                    int replacement=dp[Cell(r-1,c-1,cols)]+cost;
                    dp[Cell(r,c,cols)]=Min3(deletion,insertion,replacement);
                    c=c+1;
                }
                r=r+1;
            }
            int result=dp[Cell(source.Length,target.Length,cols)];
            Console.WriteLine("one optimal edit script, reconstructed backward:");
            r=source.Length;c=target.Length;
            while(r>0 || c>0)
            {
                if(r>0 && c>0 && source[r-1]==target[c-1]){r=r-1;c=c-1;}
                else if(r>0 && c>0 && dp[Cell(r,c,cols)]==dp[Cell(r-1,c-1,cols)]+1)
                {
                    raii string s=String.ToString(source[r-1]);raii string t=String.ToString(target[c-1]);
                    raii string a=String.Concat("  replace '",s);raii string b=String.Concat(a,"' with '");
                    raii string d=String.Concat(b,t);raii string line=String.Concat(d,"'");Console.WriteLine(line);
                    r=r-1;c=c-1;
                }
                else if(r>0 && dp[Cell(r,c,cols)]==dp[Cell(r-1,c,cols)]+1)
                {
                    raii string s=String.ToString(source[r-1]);raii string a=String.Concat("  delete '",s);raii string line=String.Concat(a,"'");Console.WriteLine(line);r=r-1;
                }
                else
                {
                    raii string t=String.ToString(target[c-1]);raii string a=String.Concat("  insert '",t);raii string line=String.Concat(a,"'");Console.WriteLine(line);c=c-1;
                }
            }
            delete[] dp;return result;
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
            raii string source=String.Copy("kitten");raii string target=String.Copy("sitting");
            int distance=EditDistanceSolver.Solve(source,target);
            raii string d=String.ToString(distance);raii string line=String.Concat("edit distance: ",d);Console.WriteLine(line);
            return 0;
        }
    }
}
```

### 35.23 Álgebra de polinomios

**Problema.** Representar coeficientes, sumar, restar, multiplicar, derivar y evaluar sin esconder la complejidad de cada operación.

**Diseño.** El indexador abstrae potencias, los operadores producen polinomios nuevos y Horner evalúa en tiempo lineal. La impresión materializa strings temporales bajo RAII.

**Auditoría propuesta.** Revise coeficientes cero, polinomio constante, grado resultante y ownership de cada resultado algebraico. Compare Horner con potencias repetidas.

**Escala real.** 3 archivos NativeSharp, 130 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

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
            if (negative) { value = -value; }
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
            if(degree<0){degree=0;}
            this.degree=degree;coefficients=new double[degree+1];
        }
        public int Degree { get { return degree; } }
        public double this[int power]
        {
            get { if(power<0 || power>degree){return 0.0;} return coefficients[power]; }
            set { if(power<0 || power>degree){throw new ArgumentOutOfRangeException();} coefficients[power]=value; }
        }
        public double Evaluate(double x)
        {
            double result=coefficients[degree];int p=degree-1;
            while(p>=0){result=result*x+coefficients[p];p=p-1;}
            return result;
        }
        public Polynomial Derivative()
        {
            if(degree==0){return new Polynomial(0);}
            Polynomial result=new Polynomial(degree-1);int p=1;
            while(p<=degree){result[p-1]=coefficients[p]*(double)p;p=p+1;}
            return result;
        }
        public static Polynomial operator +(Polynomial a,Polynomial b)
        {
            int d=a.degree;if(b.degree>d){d=b.degree;}Polynomial r=new Polynomial(d);int p=0;
            while(p<=d){r[p]=a[p]+b[p];p=p+1;}return r;
        }
        public static Polynomial operator -(Polynomial a,Polynomial b)
        {
            int d=a.degree;if(b.degree>d){d=b.degree;}Polynomial r=new Polynomial(d);int p=0;
            while(p<=d){r[p]=a[p]-b[p];p=p+1;}return r;
        }
        public static Polynomial operator *(Polynomial a,Polynomial b)
        {
            Polynomial r=new Polynomial(a.degree+b.degree);int i=0;
            while(i<=a.degree){int j=0;while(j<=b.degree){r[i+j]=r[i+j]+a[i]*b[j];j=j+1;}i=i+1;}return r;
        }
        public void Print(string name)
        {
            raii string line=String.Concat(name,"(x) = ");
            int p=degree;bool first=true;
            while(p>=0)
            {
                double c=coefficients[p];
                if(c!=0.0)
                {
                    if(!first)
                    {
                        raii string plus=String.Concat(line," + ");line=move(plus);
                    }
                    raii string value=NumberText.Format(c);raii string term=String.Concat(line,value);line=move(term);
                    if(p>0){raii string x=String.Concat(line,"*x");line=move(x);}
                    if(p>1){raii string power=String.ToString(p);raii string caret=String.Concat(line,"^");line=move(caret);raii string withPower=String.Concat(line,power);line=move(withPower);}
                    first=false;
                }
                p=p-1;
            }
            if(first){raii string zero=String.Concat(line,"0");line=move(zero);}
            Console.WriteLine(line);
        }
        ~Polynomial(){delete[] coefficients;}
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
            Polynomial p=new Polynomial(3);p[0]=2.0;p[1]=-1.0;p[2]=0.0;p[3]=3.0;
            Polynomial q=new Polynomial(2);q[0]=-4.0;q[1]=2.0;q[2]=1.0;
            p.Print("P");q.Print("Q");
            Polynomial sum=p+q;Polynomial difference=p-q;Polynomial product=p*q;Polynomial derivative=p.Derivative();
            sum.Print("P+Q");difference.Print("P-Q");product.Print("P*Q");derivative.Print("P'");
            double x=2.0;double value=product.Evaluate(x);
            raii string xs=NumberText.Format(x);raii string vs=NumberText.Format(value);
            raii string a=String.Concat("(P*Q)(",xs);raii string b=String.Concat(a,") = ");raii string line=String.Concat(b,vs);Console.WriteLine(line);
            delete derivative;delete product;delete difference;delete sum;delete q;delete p;
            return 0;
        }
    }
}
```

### 35.24 Vectores y matrices 3x3

**Problema.** Componer transformaciones lineales y operar vectores con una interfaz algebraica legible.

**Diseño.** `Vector3` implementa suma, resta, producto punto y cruz. `Matrix3` almacena nueve valores, compone matrices y transforma vectores. El programa conserva todos los resultados para imprimirlos y destruirlos.

**Auditoría propuesta.** Verifique orden de multiplicación, matriz identidad, signos del producto cruz y layout de índices. Audite que cada objeto creado se destruya una sola vez.

**Escala real.** 4 archivos NativeSharp, 124 líneas de fuente, proyecto completo verificado en `-O0` y `-O2`.

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
        public Matrix3(){m=new double[9];}
        private int Cell(int r,int c){return r*3+c;}
        public double Get(int r,int c){return m[Cell(r,c)];}
        public void Set(int r,int c,double value){m[Cell(r,c)]=value;}
        public static Matrix3 Identity()
        {
            Matrix3 r=new Matrix3();r.Set(0,0,1.0);r.Set(1,1,1.0);r.Set(2,2,1.0);return r;
        }
        public static Matrix3 operator +(Matrix3 a,Matrix3 b)
        {
            Matrix3 r=new Matrix3();int row=0;while(row<3){int col=0;while(col<3){r.Set(row,col,a.Get(row,col)+b.Get(row,col));col=col+1;}row=row+1;}return r;
        }
        public static Matrix3 operator *(Matrix3 a,Matrix3 b)
        {
            Matrix3 r=new Matrix3();int row=0;
            while(row<3){int col=0;while(col<3){double sum=0.0;int k=0;while(k<3){sum=sum+a.Get(row,k)*b.Get(k,col);k=k+1;}r.Set(row,col,sum);col=col+1;}row=row+1;}return r;
        }
        public Vector3 Transform(Vector3 v)
        {
            return new Vector3(
                Get(0,0)*v.X+Get(0,1)*v.Y+Get(0,2)*v.Z,
                Get(1,0)*v.X+Get(1,1)*v.Y+Get(1,2)*v.Z,
                Get(2,0)*v.X+Get(2,1)*v.Y+Get(2,2)*v.Z);
        }
        public void Print(string label)
        {
            Console.WriteLine(label);int row=0;
            while(row<3)
            {
                raii string a=NumberText.Format(Get(row,0));raii string b=NumberText.Format(Get(row,1));raii string c=NumberText.Format(Get(row,2));
                raii string x=String.Concat("  [ ",a);raii string y=String.Concat(x,", ");raii string z=String.Concat(y,b);raii string q=String.Concat(z,", ");raii string w=String.Concat(q,c);raii string line=String.Concat(w," ]");Console.WriteLine(line);row=row+1;
            }
        }
        ~Matrix3(){delete[] m;}
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
            if (negative) { value = -value; }
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
        public double X;public double Y;public double Z;
        public Vector3(double x,double y,double z){X=x;Y=y;Z=z;}
        public double this[int index]
        {
            get { if(index==0){return X;} if(index==1){return Y;} return Z; }
            set { if(index==0){X=value;} else if(index==1){Y=value;} else{Z=value;} }
        }
        public static Vector3 operator +(Vector3 a,Vector3 b){return new Vector3(a.X+b.X,a.Y+b.Y,a.Z+b.Z);}
        public static Vector3 operator -(Vector3 a,Vector3 b){return new Vector3(a.X-b.X,a.Y-b.Y,a.Z-b.Z);}
        public static Vector3 operator -(Vector3 a){return new Vector3(-a.X,-a.Y,-a.Z);}
        public double Dot(Vector3 other){return X*other.X+Y*other.Y+Z*other.Z;}
        public Vector3 Cross(Vector3 other){return new Vector3(Y*other.Z-Z*other.Y,Z*other.X-X*other.Z,X*other.Y-Y*other.X);}
        public void Print(string label)
        {
            raii string x=NumberText.Format(X);raii string y=NumberText.Format(Y);raii string z=NumberText.Format(Z);
            raii string a=String.Concat(label,"(");raii string b=String.Concat(a,x);raii string c=String.Concat(b,", ");
            raii string d=String.Concat(c,y);raii string e=String.Concat(d,", ");raii string f=String.Concat(e,z);raii string line=String.Concat(f,")");Console.WriteLine(line);
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
            Vector3 a=new Vector3(1.0,2.0,3.0);Vector3 b=new Vector3(4.0,-1.0,2.0);
            Vector3 sum=a+b;Vector3 difference=a-b;Vector3 cross=a.Cross(b);double dot=a.Dot(b);
            a.Print("a = ");b.Print("b = ");sum.Print("a+b = ");difference.Print("a-b = ");cross.Print("a x b = ");
            raii string dotText=NumberText.Format(dot);raii string dotLine=String.Concat("a dot b = ",dotText);Console.WriteLine(dotLine);
            Matrix3 scale=new Matrix3();scale.Set(0,0,2.0);scale.Set(1,1,3.0);scale.Set(2,2,1.0);
            Matrix3 shear=Matrix3.Identity();shear.Set(0,1,1.0);
            Matrix3 composed=scale*shear;composed.Print("scale * shear:");
            Vector3 transformed=composed.Transform(a);transformed.Print("transformed a = ");
            delete transformed;delete composed;delete shear;delete scale;delete cross;delete difference;delete sum;delete b;delete a;
            return 0;
        }
    }
}
```

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
- `public int Increment()`
- `public int Decrement()`
- `public int Add(int value)`
- `public int Exchange(int value)`
- `public int CompareExchange(int value, int comparand)`
- `public class AtomicInt64`
- `public AtomicInt64(long initialValue)`
- `public long Value`
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

Esta edición se generó desde el árbol de NativeSharp situado en `A:\\nsc-devel`. Los 24 proyectos
de `book_examples` fueron emitidos como PE32+ y ejecutados en Windows con código de salida 0. La
validación cubrió `-O0` y `-O2`. El objetivo no es congelar el lenguaje alfa, sino hacer que cada
edición pueda decir exactamente qué compilador presentó al mundo.
