# Tanda 13: cierre

## Criterio

El mismo de las tandas [10](t10-cierre.md), [11](t11-cierre.md) y [12](t12-cierre.md). Una casilla T13 se
cierra cuando:

- la capacidad que pide tiene evidencia propia frente a un control compilado por Clang 23.1.0, en
  biblioteca estática y DLL, O0–O3;
- la instanciación, la deducción, la selección de especializaciones y las restricciones las aplica Sema de
  Clang en la sesión, y cada declaración que NSC llama usa el símbolo y los tipos físicos de Clang;
- cada forma que NSC no proyecta se rechaza con diagnóstico (los de Clang cuando la rechaza Clang), sin
  emitir otra llamada ni otro almacenamiento, y queda registrada abajo como pendiente, con la tanda o el
  ítem que la continúa.

Un límite registrado no es interoperabilidad terminada (T18-007).

## Casillas

| Ítem | Capacidad comprobada | Registro |
|---|---|---|
| T13-001 | Auditoría de genéricos, argumentos e instanciaciones | [auditoría](../TANDA-13-AUDITORIA-INICIAL.md) |
| T13-002 | `CheckTemplateIdType`, `RequireCompleteType`, `DeduceTemplateArguments` (explícitos y desde la llamada), `InstantiateFunctionDefinition`, `InstantiateVariableDefinition` | [plantillas](t13-plantillas-ns.md), [deducción](t13-deduccion.md), [constexpr](t13-constexpr.md) |
| T13-003 | La definición es la que Clang ve en la unidad del encabezado con la configuración de la sesión | [especializaciones](t13-especializaciones.md) |
| T13-004 | Especializaciones de clase nombradas en el encabezado o escritas en NativeSharp, con nombre de su tipo canónico | [especializaciones](t13-especializaciones.md), [plantillas](t13-plantillas-ns.md) |
| T13-005 | Plantillas de función con argumentos explícitos, deducidos y sobrecargadas; la que no es plantilla gana el empate | [plantillas](t13-plantillas-ns.md), [deducción](t13-deduccion.md) |
| T13-006 | Tipos básicos, records, enums y alias C++, especializaciones anidadas y punteros como argumentos | [plantillas](t13-plantillas-ns.md) |
| T13-007 | Struct NativeSharp como argumento mediante un espejo C++, con layout comprobado | [structs](t13-structs-ns.md) |
| T13-008 | Solo structs triviales; destructor, cadena o propiedad rechazados con el motivo | [structs](t13-structs-ns.md) |
| T13-009 | Clases de referencia rechazadas como argumento, con diagnóstico | [structs](t13-structs-ns.md) |
| T13-010 | Instanciación en las sesiones que declaran la plantilla, al final de su unidad | [especializaciones](t13-especializaciones.md), [plantillas](t13-plantillas-ns.md) |
| T13-011 | Campos, métodos, constructores y destructor de la especialización; definiciones de Clang | [especializaciones](t13-especializaciones.md) |
| T13-012 | Parámetros no tipo enteros, negativos y bool | [especializaciones](t13-especializaciones.md), [plantillas](t13-plantillas-ns.md) |
| T13-013 | Parámetro plantilla de plantilla | [plantillas](t13-plantillas-ns.md) |
| T13-014 | Paquetes con argumentos explícitos y deducidos | [plantillas](t13-plantillas-ns.md), [deducción](t13-deduccion.md) |
| T13-015 | Especializaciones explícitas y parciales elegidas por Clang | [especializaciones](t13-especializaciones.md), [plantillas](t13-plantillas-ns.md) |
| T13-016 | Diagnósticos de sustitución de Clang en el uso escrito | [plantillas](t13-plantillas-ns.md), [deducción](t13-deduccion.md) |
| T13-017 | Concepts y `requires` comprobados por Sema, con su explicación | [plantillas](t13-plantillas-ns.md), [deducción](t13-deduccion.md) |
| T13-018 | Una definición `weak_odr` por miembro y el mismo nombre en toda sesión | [especializaciones](t13-especializaciones.md), [COMDAT](t13-comdat.md) |
| T13-019 | COMDAT `Any` en la biblioteca C++ y en NativeSharp; NSLink y lld-link enlazan una sola definición | [COMDAT](t13-comdat.md) |
| T13-020 | La misma especialización en un módulo precompilado y en su consumidor | [COMDAT](t13-comdat.md) |
| T13-021 | Funciones y expresiones `constexpr` evaluadas por Clang; miembro `static constexpr` de una especialización | [constexpr](t13-constexpr.md) |
| T13-022 | Constantes con enlace interno leídas con el valor del evaluador de Clang | [constexpr](t13-constexpr.md) |
| T13-023 | Plantilla de biblioteca con tipos C++ como argumento | [especializaciones](t13-especializaciones.md) |
| T13-024 | Struct NativeSharp trivial como argumento: campo, valor, puntero, retorno, anidamiento | [structs](t13-structs-ns.md) |
| T13-025 | Structs NativeSharp de vida no trivial rechazados, con contraparte trivial | [structs](t13-structs-ns.md) |

## Pendientes que quedan fuera de la tanda 13

Cada uno se rechaza con diagnóstico o deja el nombre sin proyectar.

| Pendiente | Ítem | Continúa en |
|---|---|---|
| Plantillas miembro o anidadas en un record, escritas desde NativeSharp | T13-004, T13-005 | pendiente documentado |
| Un uso de plantilla con un nombre importado por `using` | T13-006 | pendiente documentado |
| Plantillas de variable (`template <class T> constexpr T zero`) | T13-021 | pendiente documentado |
| Constantes de tipo record, arreglo o puntero como valores | T13-022 | pendiente documentado |
| Ordenación parcial entre dos plantillas deducibles con la misma firma | T13-005 | pendiente documentado |
| Deducción con un struct NativeSharp como argumento de la llamada | T13-005, T13-007 | pendiente documentado |
| Struct NativeSharp con campo C++, arreglo fijo o enum; structs genéricos | T13-007 | pendiente documentado |
| Especialización C++ en la API pública de un módulo precompilado; objetos C++ de vida no trivial y métodos heredados en una plantilla precompilada (pendientes de las tandas 11 y 12) | T13-001, T13-020 | pendiente documentado |
| Excepciones que atraviesan una especialización | T13-011 | T15-003 |

No se modificó Clang ni NSLink. El contrato NSMeta vigente es el único.
