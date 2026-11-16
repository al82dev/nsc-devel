# Guía: usar una biblioteca C o C++ desde un proyecto NativeSharp

Esta guía resume cómo se declara y se usa hoy una biblioteca C/C++ desde NativeSharp con NSC y Clang 23.1.0
(`x86_64-pc-windows-msvc`). Lo que no aparece aquí no está comprobado; los límites de cada área están en los
registros de cierre (`registro-commits/t1x-cierre.md`).

## 1. Declarar la biblioteca

En el `.nsproj`, una `CppLibraryReference` por encabezado:

```xml
<Project>
  <PropertyGroup><Name>App</Name></PropertyGroup>
  <ItemGroup>
    <Compile Include="Main.ns" />
    <CppLibraryReference Header="api.h" Language="C++" Standard="c++20" Library="api.lib" Runtime="api.dll">
      <IncludeDirectory Path="include" Kind="User" />
      <IncludeDirectory Path="A:/vc2026-runtime/toolsets/14.52.36815/include" Kind="System" />
      <Define Value="API_LEVEL=2" />
      <Undefine Value="NDEBUG" />
    </CppLibraryReference>
  </ItemGroup>
</Project>
```

| Atributo o hijo | Uso |
|---|---|
| `Header` (obligatorio) | encabezado que analiza Clang |
| `Language` (obligatorio) | `C` o `C++` |
| `Standard` (obligatorio) | `c11`, `c17`, `c++17`, `c++20`, … |
| `Library` | `.lib` estática, o biblioteca de importación si hay `Runtime` |
| `Runtime` | la DLL, que NSC copia junto al producto |
| `VolatileMode` | `Default`, `Microsoft` o `ISO` (C) |
| `RuntimeTls` | `ImportDirectory`, para una DLL con TLS estático sin directorio de importación |
| `PublicTypes` | `Header`: una biblioteca NativeSharp publica en su catálogo NSMeta los tipos C de este encabezado |
| `IncludeDirectory` | `Path` y `Kind` (`User` o `System`) |
| `Define`, `Undefine` | `Value` (`NOMBRE` o `NOMBRE=valor`) |

Compilar con la raíz de Clang: `nsc --emit-project-oop-exe App.nsproj --cpp-clang-root=C:\clang-llvm -o app.exe`
(también `--emit-project-oop-lib` y `--emit-project-oop-dll`).

## 2. Nombres y tipos

- Un namespace C++ es un namespace NativeSharp: `A::B::f(x)` se llama `A.B.f(x)`; una función C, por su
  nombre.
- Los enteros, flotantes, punteros, enums, structs y clases C++ tienen su tipo NativeSharp; una referencia
  `T&` es un parámetro `ref` y `const T&` uno `in`.
- Un objeto de clase C++ se crea con `new`: `A.T x = new A.T();`, y su destructor corre al salir del ámbito.
- Una plantilla se usa con argumentos explícitos: `A.Box<int>`, `A.twice<int>(3)`, o deducidos desde la
  llamada (`A.twice(3)`).
- Un callback es una lambda sin capturas asignada antes a un local del tipo puntero a función:
  `A.Cb f = (int v) => v * 2; A.run(f);`. El estado viaja por un `void*` de contexto.
- `int` y `long` de C++ son ambos `int` de NativeSharp: una sobrecarga que solo difiere en ellos es ambigua.

## 3. Enlace y arranque

- `NSC_LINKER` elige `nslink.exe` o `lld-link.exe`; sin él, NSC elige lld-link para entradas con bitcode o TLS.
- Una biblioteca C++ que usa excepciones, `operator new`, la STL o constructores globales necesita el runtime
  de C de MSVC: compilar el ejecutable con `--crt-startup` y las bibliotecas del CRT por `--link-lib`
  (`libcmt`, `libvcruntime`, `libcpmt`, `libucrt`, `kernel32`, y `synchronization` si usa la espera
  atómica). El enlace es con lld-link.
- Sin `--crt-startup`, un ejecutable o una DLL NativeSharp que toma un miembro con inicializadores dinámicos
  se rechaza nombrando el miembro. La alternativa es la biblioteca como DLL, que lleva su propio runtime.
- Cada imagen (ejecutable o DLL) tiene su propia copia del runtime NativeSharp y del CRT estático: la memoria
  la libera la imagen que la reserva y los recursos del CRT (`FILE*`, `errno`, `locale`) no cruzan una DLL.
- Una excepción C++ atraviesa los frames NativeSharp y ejecuta su limpieza; un `catch` NativeSharp no la
  atrapa.

## 4. Diagnósticos

- Una función que no se puede importar se rechaza en el uso con el motivo y `note: 'A::f' is declared at
  api.h:<línea>:<columna>`.
- Una llamada que ninguna sobrecarga acepta lista cada candidato con su declaración C++ y por qué no sirve.
- Una plantilla que Clang no puede instanciar conserva las notas de Clang con su ubicación.
- Tras un enlace fallido, una función que ninguna biblioteca define se nombra con su declaración; si la
  biblioteca define el mismo nombre con otra firma, se dice que encabezado y biblioteca discrepan.

## 5. Depuración y caché

- Con `-g` o `-gline-tables-only`, el PDB tiene las líneas de NativeSharp, de las funciones inline del
  encabezado que emite NSC y de la biblioteca si se compiló con `-g -gcodeview`.
- Con la caché de artefactos (por defecto `obj/nativesharp/artifact-cache`, o `--artifact-cache=<dir>`), una
  compilación con las mismas entradas se reutiliza: opciones de cada `CppLibraryReference`, contenido de cada
  archivo que cargó Clang, bibliotecas, DLL, fuentes NativeSharp y opciones de NSC. `--verbose` informa
  `compilation-bundle-cache: hit` o el motivo del fallo.
