# Inventario de características C y C++ consumidas desde NativeSharp

Este inventario cubre T18-001 a T18-007. Cada entrada nombra la característica, su fuente (cláusula de la
norma y familia de nodos de Clang 23.1.0, `DeclNodes.td`/`TypeNodes.td`/`StmtNodes.td`), las reglas de
tipos, ABI y vida que la rigen, y la evidencia (tanda e ítems, con su registro de cierre) o el pendiente que
la continúa. Un pendiente o un rechazo con diagnóstico no es interoperabilidad terminada (T18-007). El número
de entradas no es un porcentaje de cobertura.

## Línea base (T18-001, T18-002)

- **C**: ISO/IEC 9899:2018 (C17), `Language="C" Standard="c17"`; los fixtures de las tandas 8 y 9 usan
  también `c11`, que C17 no cambia en lo que importa en la frontera.
- **C++**: ISO/IEC 14882:2020 (C++20), `Language="C++" Standard="c++20"`; `c++17` se acepta y se prueba en la
  caché (T17) y con ICU (T18).
- Compilador de referencia: Clang 23.1.0, objetivo `x86_64-pc-windows-msvc`, ABI de Microsoft; runtime de C
  y C++: MSVC estático (`/MT`), toolset 14.52.36815, SDK 10.0.28000.0.
- Fuera de la línea base: C23, C++23 y posteriores, extensiones Objective-C, OpenMP, HLSL y CUDA, el ABI
  Itanium y el runtime dinámico `/MD`.

## Entradas

Estado: **C** comprobado con un fixture frente a Clang en estática y DLL, O0–O3 salvo que se indique; **P**
pendiente documentado (con el registro que lo nombra); **R** rechazado con diagnóstico; **N** no aplica, con
el motivo.

### C (T08, T09)

| Entrada | Fuente | Reglas | Estado y evidencia |
|---|---|---|---|
| Funciones y prototipos | C17 6.7.6.3; `Function`, `ParmVar` | convención y símbolo de Clang (T09-003) | C: T08-003, T09-004..006 |
| Funciones sin prototipo `f()` | C17 6.7.6.3p14 | promociones por defecto | C: T08 ([prototipos](registro-commits/prueba-prototipos-c.md)) |
| Variádicas | C17 7.16; `isVariadic` | promociones y ABI x64 | C: T09-011, T09-012 |
| Globales y `extern` | C17 6.9.2; `Var` | lectura, escritura, `const` | C: T08-004, T09-009, T09-010 |
| `_Thread_local` | C17 6.7.1 | directorio TLS del CRT | C: T08 (TLS), T10-004; T16-006 |
| `typedef` | C17 6.7.8; `Typedef` | identidad del alias | C: T08-005 |
| Enteros, `_Bool`, `char` | C17 6.2.5 | tamaño y signo efectivos | C: T08-006 |
| Flotantes, `long double` | C17 6.2.5 | `long double` es binario64 en MSVC | C: T08-007 |
| `_Complex` | C17 6.2.5p11 | sin portador NativeSharp | P: sin prueba en las tandas 8 y 9 |
| `void` | C17 6.2.5p19 | parámetro y resultado | C: T08-008 |
| `enum` | C17 6.7.2.2; `Enum`, `EnumConstant` | tipo subyacente | C: T08-009 |
| `struct` | C17 6.7.2.1; `Record`, `Field` | tamaño, alineación y desplazamientos de Clang | C: T08-010, T08-012, T08-020, T09-007/008 |
| `union` | C17 6.7.2.1; `Record` | ídem | C: T08-011, T09-007 |
| Bit-fields C | C17 6.7.2.1p10 | disposición de Clang | C: T08 ([bit-fields](registro-commits/validacion-bitfields-c.md), [anónimos](registro-commits/t08-anonimos-c-bitfields-validacion.md)) |
| Miembro de arreglo flexible | C17 6.7.2.1p18 | tamaño sin el arreglo | C: T08 ([flexibles](registro-commits/t08-flexible-c-calificados.md)) |
| Tipos incompletos | C17 6.2.5p1 | sin tamaño inventado | C: T08-013, T08-019 |
| Punteros y calificadores | C17 6.7.3; `Pointer`, `Qualified` | `const`, `volatile`, `restrict` en el acceso | C: T08-014, T08-015 (volatile: T08-D13) |
| Arreglos de tamaño conocido | C17 6.7.6.2; `ConstantArray` | elemento y longitud | C: T08-016 |
| VLA | C17 6.7.6.2p4 | sin tamaño estático | P: no comprobado |
| Punteros a función y callbacks | C17 6.7.6.3; `FunctionProto` | adaptador con la convención de Clang | C: T08-017, T15-008..012; P: `null` a un puntero a función (T18 zlib) |
| `_Atomic` | C17 6.7.2.4; `Atomic` | campos y globales atómicos | C: T08 (`_Atomic` TLS); P: dirección de un global atómico TLS (R) |
| `_Alignas` y `#pragma pack` | C17 6.7.5 | alineación de Clang | C: T08-012, T14-008 |
| `static inline` en el encabezado | C17 6.7.4 | definición propia de la unidad | C: T08 ([enlace inline C](registro-commits/enlace-inline-c.md)) |
| Macros de objeto | C17 6.10.3 | valor evaluado por Clang | C: T14-001..003 |
| Macros de función | C17 6.10.3 | no se expanden en NativeSharp | P: t14-cierre |

### C++ (T10 a T16)

| Entrada | Fuente | Reglas | Estado y evidencia |
|---|---|---|---|
| Namespaces, alias, `using`, inline namespaces | C++20 [basic.namespace]; `Namespace`, `NamespaceAlias`, `Using*` | nombres alternativos de una misma función | C: T10-002; P: alias de variables y tipos, `using` global (t10-cierre); alias como prefijo de tipo en NativeSharp (`icu.UnicodeString`, T18) |
| Sobrecargas | [over.match] | conversiones NativeSharp; desempate de Clang en especializaciones | C: T10, T13-005, T17-003; P: `int`/`long` ambiguos (t17-cierre) |
| Argumentos predeterminados | [dcl.fct.default] | constantes evaluadas por Clang | C: T10-010; P: no constantes, enum o record |
| Referencias `T&`, `const T&` | [dcl.ref] | `ref` e `in` | C: T10-003; P: retorno `const T&` (`std::min`), `T&&` (t10, t11-cierre) |
| Operadores libres | [over.oper] | operador de extensión | C: T10-016; P: operadores miembro no const o de asignación (t12-cierre) |
| Funciones y variables `inline` | [dcl.inline] | NSC emite la definición del encabezado | C: T10-012, T15 (closure de definiciones) |
| `enum class` | [dcl.enum] | tipo subyacente | C: T10 |
| Clases: campos y métodos | [class.mem]; `CXXRecord`, `CXXMethod` | layout de Clang; `this` del ABI de Microsoft | C: T11-004, T11-005; P: privados y protegidos sin diagnóstico de acceso |
| Constructores y destructores | [class.ctor], [class.dtor] | construcción en el lugar, destructor al salir | C: T11-006, T11-007; C: constructores que lanzan (T18-011); P: `new T` en el heap |
| Copia y movimiento | [class.copy] | constructores de Clang | C: T11-015, T11-016 (parcial); P: copia implícita no trivial (t11-cierre) |
| Herencia simple y múltiple | [class.derived] | ajuste de `this` por adaptador de Clang | C: T12-001..004 |
| Despacho virtual y clases abstractas | [class.virtual] | vtable del ABI de Microsoft | C: T12-005..008 |
| Bases virtuales | [class.mi] | adaptador de Clang | C: T12-009 (métodos); P: campos y construcción |
| RTTI, `typeid`, `dynamic_cast` | [expr.typeid] | dentro de la biblioteca con el CRT | C: T12-010/011; P: escritos en NativeSharp |
| `union` C++ | [class.union] | — | P: campos (t11-cierre) |
| Plantillas de clase y de función | [temp]; `ClassTemplate`, `FunctionTemplate` | Sema instancia; COMDAT | C: T13-001..006, T18-013 (STL) |
| Parámetros no tipo, paquetes, conceptos | [temp.param], [temp.constr] | instanciación de Clang | C: T13 |
| Especialización parcial y explícita | [temp.spec] | elige Clang | C: T13 |
| Plantillas de variable | [temp.pre] | — | P: t13-cierre |
| `constexpr`, `consteval` | [dcl.constexpr] | valor evaluado por Clang | C: T13-021..022; P: constantes record, arreglo o puntero |
| Excepciones y `noexcept` | [except] | modelo de Microsoft, `__CxxFrameHandler3` | C: T15-001..007, T15-013, T18-011; P: atrapar en NativeSharp |
| Lambdas y `std::function` | [expr.prim.lambda] | puntero a función sin capturas | C: T15-008..012; P: `std::function` |
| Punteros inteligentes y allocators | [smartptr] | propiedad en la biblioteca | C: T15-014..016; P: construir `std::unique_ptr` (constructor plantilla) |
| Corutinas | [dcl.fct.def.coroutine] | pipeline de Clang | C: T15-017/018 |
| Punteros a miembro | [dcl.mptr]; `MemberPointer` | sin portador | R: T14-007 |
| Bit-fields C++ y atributos de layout | [class.bit] | disposición de Clang | C: T14-006, T14-008 |
| Atributos | [dcl.attr]; `Attr.td` | los que cambian ABI o layout | C: T09-003 (convenciones), T14-008 |
| PCH y módulos | [module] | la sesión no los lee | R: T14-009, T14-010 |
| `thread_local` C++ | [basic.stc.thread] | `constinit` en NativeSharp; dinámico en la biblioteca | C: T10-004, T16-006/007 |
| Constructores globales | [basic.start.dynamic] | `_initterm` del CRT | C: T16-003..005 |
| `std::atomic` | [atomics] | operado por la biblioteca | C: T16-017 |
| Objective-C, OpenMP, HLSL, CUDA | `ObjC*`, `OMP*`, `HLSL*` | — | N: fuera de la línea base |

### Enlace, diagnóstico y herramientas (T16, T17)

| Entrada | Reglas | Estado y evidencia |
|---|---|---|
| Estática, DLL, varias DLL | cada imagen con su runtime | C: T09-013/014, T16-009..012 |
| NSLink y lld-link; ThinLTO | lld-link para bitcode y TLS | C: T16-014..016 |
| Diagnósticos con ubicaciones C++ | — | C: T17-001..006 |
| Depuración CodeView | — | C: T17-007/008 |
| Caché con sesiones C++ | identidad de cada archivo cargado | C: T17-009..011 |

## Combinaciones (T18-006)

Las combinaciones comprobadas en un mismo fixture, no por separado:

| Combinación | Fixture |
|---|---|
| Plantilla con struct NativeSharp como argumento | `pruebas-tanda-13-*` |
| Excepción C++ a través de un callback NativeSharp con un local C++ | `pruebas-tanda-15-excepciones` |
| Constructor que lanza con otro local ya construido, dentro de un callback | `pruebas-tanda-18-constructores` |
| TLS dinámico + hilo de la biblioteca + callback NativeSharp | `pruebas-tanda-16-tls` |
| Atómicos + cuatro hilos + callbacks | `pruebas-tanda-16-atomicos` |
| Dos bibliotecas + módulo NativeSharp precompilado + COMDAT, con dos enlazadores | `pruebas-tanda-16-enlace` |
| Struct C con punteros a función + asignador del CRT + API de flujo (zlib) | `pruebas-tanda-18-zlib` |
| Plantillas de clase con parámetro no tipo + miembros estáticos + funciones plantilla (STL) | `pruebas-tanda-18-stl` |
| Biblioteca C++ de compilación separada: clases con destructor virtual, memoria propia y constructores que no son `noexcept`, estática y DLL (ICU) | `pruebas-tanda-18-icu` |
| Caché + `Define` + encabezado transitivo + `Standard` + DLL | `pruebas-tanda-17-cache` |
| Paquete instalado + entorno limpio + C, C++ con el CRT, DLL y biblioteca estática | `pruebas-tanda-18-paquete` |
| Herencia + despacho virtual + DLL | `pruebas-tanda-12-*` |

Las combinaciones que no aparecen aquí (por ejemplo, corutina + excepción + DLL, o plantilla + herencia
virtual + caché) no están comprobadas.

## Pendientes consolidados (T18-007, T18-021)

Los pendientes de cada tanda están en su cierre: [t10](registro-commits/t10-cierre.md),
[t11](registro-commits/t11-cierre.md), [t12](registro-commits/t12-cierre.md),
[t13](registro-commits/t13-cierre.md), [t14](registro-commits/t14-cierre.md),
[t15](registro-commits/t15-cierre.md), [t16](registro-commits/t16-cierre.md),
[t17](registro-commits/t17-cierre.md) y [t18](registro-commits/t18-cierre.md). La lista revisada, con una tarea
específica para cada uno (T19-001 a T19-026), está en [PENDIENTES-INTEROP-C-CPP.md](PENDIENTES-INTEROP-C-CPP.md).
Ninguno se cuenta como cubierto.
