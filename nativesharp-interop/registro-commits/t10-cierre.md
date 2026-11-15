# Tanda 10: cierre

## Criterio

Una casilla T10 se cierra cuando:

- la capacidad que pide tiene evidencia propia frente a un control compilado por Clang 23.1.0, en
  biblioteca estática y DLL, O0–O3 y, donde aplica, desde plantilla precompilada con y sin
  encabezado;
- cada declaración que NSC acepta usa la declaración, el símbolo y la convención de Clang;
- cada forma que NSC no proyecta se rechaza con diagnóstico, sin emitir otra llamada ni otro
  almacenamiento, y queda registrada abajo como pendiente, con la tanda o el ítem que la continúa.

Un límite registrado no es interoperabilidad terminada (T18-007).

## Casillas

| Ítem | Capacidad comprobada | Registro |
|---|---|---|
| T10-001 | Identidad de declaración por USR, símbolo de Clang, nombre calificado y alternativos | [identidad](t10-identidad-sobrecargas-simbolos.md) |
| T10-002 | Namespaces anidados y homónimos, inline namespaces, alias, declaraciones y directivas using, `extern "C"` en namespace, enums y records anidados | [nombres](t10-nombres-namespace.md), [directivas](t10-directivas-using.md), [anidados](t10-tipos-anidados.md) |
| T10-003 | Escalares, punteros, enums, records triviales por valor, referencias lvalue y variádicas | [referencias](t10-referencias-lvalue.md); variádicas en la tanda 9 |
| T10-004 | Globales escalares, de record, enum y arreglo, `dllimport`, `thread_local constinit` | [globales](t10-globales-y-estaticos.md) |
| T10-005 | Métodos estáticos, también de records anidados, con enums y records triviales por valor | [anidados](t10-tipos-anidados.md), [records por valor](t10-estaticos-records-valor.md) |
| T10-006 | Campos estáticos escalares, de enum, record y arreglo; `constexpr` sin símbolo | [globales](t10-globales-y-estaticos.md), [inline](t10-inline-emitidas-por-clang.md) |
| T10-007 | Acceso público, protegido y privado de métodos y campos estáticos | [acceso](t10-acceso-miembros.md) |
| T10-008 | Resolución de sobrecargas por NativeSharp | [identidad](t10-identidad-sobrecargas-simbolos.md) |
| T10-009 | Conversiones implícitas de NativeSharp, no de C++ | [identidad](t10-identidad-sobrecargas-simbolos.md) |
| T10-010 | Predeterminados constantes evaluados por Clang | [predeterminados](t10-argumentos-predeterminados.md) |
| T10-011 | Símbolo tomado del `MangleContext` de Clang | [identidad](t10-identidad-sobrecargas-simbolos.md) |
| T10-012 | Llamadas a funciones libres; definiciones `inline` emitidas por Clang | [inline](t10-inline-emitidas-por-clang.md) y todos los fixtures |
| T10-013 | Llamadas a métodos estáticos | [anidados](t10-tipos-anidados.md), [records por valor](t10-estaticos-records-valor.md) |
| T10-014 | Lectura de campos estáticos | [globales](t10-globales-y-estaticos.md) |
| T10-015 | Escritura de campos estáticos y rechazo de `const` | [globales](t10-globales-y-estaticos.md) |
| T10-016 | Operadores libres como operadores de extensión NativeSharp | [operadores](t10-operadores-libres.md) |
| T10-017 | Biblioteca integral de Clang con sobrecargas, namespaces y miembros estáticos | [identidad](t10-identidad-sobrecargas-simbolos.md) |

## Pendientes que quedan fuera de la tanda 10

Cada uno se rechaza con diagnóstico o deja el nombre sin proyectar.

| Pendiente | Ítem | Continúa en |
|---|---|---|
| Operadores miembro y métodos de instancia | T10-016, T10-007 | T11-005 |
| Records C++ no triviales por valor (constructor de copia propio) | T10-003, T10-005 | T11-015 |
| Referencias rvalue `T&&` | T10-003 | T11-016 |
| Retorno `const T&` | T10-003 | T11-019 |
| Records declarados con `class` en el catálogo precompilado | T10-007 | T11-002 |
| Constructores y destructores definidos en el encabezado | T10-012 | T11-006, T11-007 |
| Acceso protegido desde una clase NativeSharp derivada | T10-007 | T12-001 |
| Plantillas de función y de clase, records anidados en plantillas | T10-003, T10-005, T10-012 | tanda 13 |
| Globales y campos estáticos C++ en el cuerpo de una plantilla precompilada | T10-004, T10-006 | T13-001 |
| `thread_local` sin `constinit` (inicialización bajo demanda del CRT) y campos estáticos `thread_local` | T10-004, T10-006 | T16-006 |
| Ejecutables con TLS enlazados con NSLink, que no escribe el directorio TLS | T10-004 | T16-006 |
| Nombres alternativos de variables y tipos (alias, using); directiva using en el ámbito global | T10-002 | pendiente documentado |
| Global de tipo enum sin que el programa nombre el enum | T10-004 | pendiente documentado |
| Predeterminados de enum, de record o no constantes | T10-010 | pendiente documentado |
| Records y enums anidados privados o protegidos; enumerador no scoped nombrado por el record | T10-002, T10-007 | pendiente documentado |
| Operandos `T&` en operadores, retornos por referencia; operando temporal con `const T&` | T10-016 | pendiente documentado |
| Funciones `inline` declaradas `dllimport` | T10-012 | pendiente documentado |
| Referencias `T&` y `const T&` en métodos estáticos | T10-005, T10-013 | pendiente documentado |

No se modificó Clang ni NSLink. El contrato NSMeta vigente es el único.
