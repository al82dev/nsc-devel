# Tanda operativa 42 — pointers, constancia, casts y linkage

**Fecha:** 1 de octubre de 2026

**Rama:** `development`

**Plataforma:** Windows x64

**Configuración validada:** Debug/O0

**Baseline:** no ejecutado; Alberto lo ejecutará en T53

## Objetivo

Cerrar las limitaciones §30, §31, §32, §33, §34, §36, §44, §82 y §101 con un
contrato explícito y fail-closed:

- separar la inmutabilidad del binding de la inmutabilidad del objeto apuntado;
- permitir obtener la dirección de almacenamiento `const` sin volverlo escribible;
- aprobar casts nombrados con capacidades distintas y diagnósticos causales;
- admitir exclusivamente `&array[length]` como dirección one-past de un array fijo;
- hacer que `volatile` local llegue a cargas y escrituras LLVM `volatile`;
- decidir de forma explícita el contrato de linkage para variables de namespace;
- conservar el ABI único Windows x64: los calificadores no cambian tamaño, alineación ni
  representación física del puntero.

## Contrato de constancia aprobado

NativeSharp 3.0 distingue dos ejes con una sintaxis deliberadamente no ambigua:

| Declaración | Binding | Pointee |
|---|---|---|
| `int* p` | reasignable | escribible |
| `const int* p` | no reasignable | escribible |
| `int const* p` | reasignable | sólo lectura |
| `const int const* p` | no reasignable | sólo lectura |

El `const` inicial continúa siendo el calificador histórico de la variable. El `const` entre el
tipo base y `*` es nuevo en Language 3 y califica el pointee. La AST conserva este segundo eje,
pero el tipo canónico y el ABI siguen siendo el mismo puntero Win64 de 64 bits.

El calificador viaja por locales, parámetros, fields, retornos, llamadas, constructores, casts,
condicionales y aritmética de punteros. Una conversión implícita puede añadir sólo lectura, nunca
quitarla. Inicialización, asignación, argumento y retorno que intenten descartarla se rechazan. La
única salida explícita aprobada es `const_cast<T*>`.

## Dirección y one-past

`&constLocal` es legal porque el almacenamiento tiene dirección. El resultado queda marcado como
puntero a pointee de sólo lectura, de modo que `*p = value` sigue siendo ilegal.

Para un array fijo `T[N]`, el índice constante `N` continúa fuera del rango de valores. Sólo se
admite dentro del operando inmediato de `&`: `&array[N]`. Ese resultado lleva provenance
one-past y puede compararse, restarse o usarse como centinela, pero una desreferencia directa o
posterior se rechaza. La propiedad se conserva de forma conservadora al copiar o hacer aritmética
desde ese puntero. Índices negativos, mayores que `N`, o `array[N]` fuera de `&` mantienen el
diagnóstico de bounds existente.

## Casts nombrados aprobados

Los nombres son contextuales y sólo adquieren significado ante la forma `<tipo>(expresión)`:

- `const_cast<T*>(p)` sólo cambia constancia; exige exactamente el mismo tipo base y profundidad
  de puntero.
- `reinterpret_cast<T>(value)` opera exclusivamente entre punteros y enteros del tamaño de una
  dirección (`nint`/`nuint`). No puede retirar constancia.
- `static_cast<T>(value)` admite conversiones numéricas, enum/integral, conversiones implícitas y
  recuperación explícita mediante `void*`. No es un reinterpret genérico y no retira constancia.

El cast C existente permanece para conversiones ordinarias, pero no puede usarse para evadir el
calificador del pointee: `(int*)readonlyPointer` indica que debe escribirse `const_cast<int*>`.

## `volatile` local

`volatile T local` se admite para escalares nativos, enums y raw pointers. No se admite sobre
aggregates, referencias administradas ni en combinación con `const`, `raii`, `owned`, `borrowed`
o una reference-local. El calificador significa acceso observable: cada lectura y escritura del
slot se emite con la marca LLVM `volatile`. No promete atomicidad, fences ni sincronización entre
hilos.

HIR marca tanto `LocalValue` como `LocalAddress`; MIR conserva la marca ya existente y LLVM la
aplica al `load` o al `store` que consume la dirección. El verificador inspecciona ambas
instrucciones en el IR, además de ejecutar el programa O0.

## Decisión de linkage de variables de namespace

NativeSharp 3.0 no incorpora dos modelos parciales de ODR/linkage antes de que NSMETA y el cierre
de paquetes puedan representarlos sin ambigüedad:

- `static T state;` a nivel namespace se rechaza como file-local no soportado;
- `extern T state;` a nivel namespace se rechaza como declaración externa no soportada;
- el estado de namespace normal continúa siendo una definición única con identidad calificada;
- `extern C` continúa reservado para fronteras nativas de funciones/delegates, no para importar
  almacenamiento global.

Los diagnósticos indican la alternativa admitida, en vez de caer en errores genéricos de parser.
Esta es una decisión de estabilidad para 3.0, no aceptación silenciosa ni implementación parcial.

## Archivos modificados

- `include/nativesharp/AST/TypeSyntax.h`, `src/AST/TypeSyntax.cpp` y
  `src/AST/AstDumper.cpp`: almacenan, clonan y muestran `T const*`.
- `src/Parser/Parser.cpp`: reconoce postfix const en parseo/lookahead, los tres casts nombrados,
  `volatile` local y diagnósticos explícitos de `static`/`extern` namespace.
- `include/nativesharp/Frontend/ProjectMemberModel.h`,
  `src/Frontend/ProjectMemberModel.cpp` y
  `src/Frontend/ProjectNamespaceFunctionModel.cpp`: llevan constancia de parámetros, fields y
  retornos sin modificar identidad ABI.
- `include/nativesharp/Frontend/ProjectMethodBodyModel.h` y
  `src/Frontend/ProjectMethodBodyModel.cpp`: verifican conversiones, direcciones, one-past,
  desreferencia, llamadas, retornos, asignaciones, constructor arguments y volatile local.
- `src/Frontend/ProjectConstructionModel.cpp`: impide que la selección de constructor descarte
  constancia del argumento.
- `src/Frontend/ProjectGenericModel.cpp`: conserva el calificador al monomorfizar tipos.
- `src/Frontend/ProjectOopHirModel.cpp` y `src/LLVM/ProjectOopLlvmLowerer.cpp`: propagan y emiten
  accesos volatile de locales.
- `CMakeLists.txt`: registra la prueba dirigida T42, sin convertirla en baseline automático de
  esta tanda.

## Archivos nuevos y razón

- `tests/integration/ns3_t42_pointers_linkage/T42Pointers.nsproj` y `Main.ns`: fixture O0 que
  ejercita las cuatro combinaciones de constancia, los tres casts, round-trip de dirección,
  volatile y one-past.
- `tests/integration/ns3_t42_pointers_linkage/negative/*`: doce fixtures pequeños para descarte de
  constancia en inicialización/cast/call/return, escritura de pointee, reasignación de binding,
  desreferencia one-past, casts fuera de contrato y linkage namespace rechazado.
- `cmake/VerifyNativeSharpT42PointersLinkage.cmake`: compila y ejecuta sólo el positivo, inspecciona
  `load volatile`/`store volatile` y comprueba cada rechazo causal.
- `docs/3.0/TANDA-OPERATIVA-42-POINTERS-CONSTANCIA-CASTS-LINKAGE.md`: este expediente de diseño,
  archivos, mejora y evidencia.

## Validación dirigida

- Construcción: `cmake --build A:\nsc-devel\build-clang --config Debug --target nsc -j 12`.
- Verificador único: `VerifyNativeSharpT42PointersLinkage.cmake`.
- Positivo: compilación O0 y ejecución física con exit `0`.
- Backend: el dump LLVM contiene carga y escritura `volatile` del slot local.
- Negativos: doce proyectos independientes validan el diagnóstico esperado.

No se ejecutó CTest completo, baseline, O1/O2/O3 ni trabajo remoto; no se instaló ni modificó
`C:\native-sharp`.

## Mejora obtenida

Un raw pointer ya no pierde información de escritura al pasar por una variable, llamada, retorno,
constructor, condicional o cast. Tomar una dirección de sólo lectura deja de estar prohibido y pasa
a ser seguro por tipo. One-past deja de ser una excepción de bounds que luego pudiera
desreferenciarse, y `volatile` local deja de ser sólo sintaxis: produce accesos observables reales.
Los casos de linkage todavía no soportados fallan en el frontend con una decisión estable y
accionable, manteniendo intacta la representación Windows x64.
