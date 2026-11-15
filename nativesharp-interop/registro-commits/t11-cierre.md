# Tanda 11: cierre

## Criterio

El mismo de la [tanda 10](t10-cierre.md). Una casilla T11 se cierra cuando:

- la capacidad que pide tiene evidencia propia frente a un control compilado por Clang 23.1.0, en
  biblioteca estática y DLL, O0–O3;
- cada declaración que NSC acepta usa la declaración, el símbolo, el layout y la convención de Clang;
- cada forma que NSC no proyecta se rechaza con diagnóstico, sin emitir otra llamada ni otro
  almacenamiento, y queda registrada abajo como pendiente, con la tanda o el ítem que la continúa.

Un límite registrado no es interoperabilidad terminada (T18-007).

## Casillas

| Ítem | Capacidad comprobada | Registro |
|---|---|---|
| T11-001 | Auditoría del modelo de valor, referencia, propiedad y destrucción | [auditoría](../TANDA-11-AUDITORIA-INICIAL.md) |
| T11-002 | Instancia C++ como valor struct con layout de Clang, trivial copiable o move-only | [objetos](t11-objetos-y-propiedad.md) |
| T11-003 | Tamaño y alineación de Clang, también `alignas(32)` | [objetos](t11-objetos-y-propiedad.md) |
| T11-004 | Campos públicos de instancia, de `struct` y `class`, con relleno | [objetos](t11-objetos-y-propiedad.md) |
| T11-005 | Métodos de instancia con escalares, enums, records, punteros y referencias; nombres reservados | [métodos](t11-metodos-instancia.md) |
| T11-006 | Constructores con escalares, enums, records, punteros y referencias, con acceso | [constructores](t11-constructores.md) |
| T11-007 | Destructor `noexcept` implícito, trivial, por defecto o explícito, y su acceso | [destructores](t11-destructores-noexcept.md), [constructores](t11-constructores.md) |
| T11-008 | Almacenamiento alineado de un local por valor | [objetos](t11-objetos-y-propiedad.md) |
| T11-009 | Construcción en el almacenamiento NativeSharp | [objetos](t11-objetos-y-propiedad.md), [constructores](t11-constructores.md) |
| T11-010 | Receptor implícito y retorno en memoria tras `this` | [métodos](t11-metodos-instancia.md) |
| T11-011 | Lectura de campos con el layout de Clang | [objetos](t11-objetos-y-propiedad.md) |
| T11-012 | Escritura de campos | [objetos](t11-objetos-y-propiedad.md) |
| T11-013 | Destrucción al final del ámbito, en orden inverso, también de miembros | [destructores](t11-destructores-noexcept.md) |
| T11-014 | Sin construcción parcial: solo constructores no lanzadores en locales | ver abajo |
| T11-015 | Copia explícita con el constructor de copia | [constructores](t11-constructores.md) |
| T11-016 | Movimiento explícito con el constructor de movimiento | [constructores](t11-constructores.md) |
| T11-017 | Copia eliminada rechazada | [constructores](t11-constructores.md) |
| T11-018 | Objetos dinámicos creados y liberados por la biblioteca | [propiedad](t11-objetos-y-propiedad.md) |
| T11-019 | Punteros y referencias que no poseen el objeto | [propiedad](t11-objetos-y-propiedad.md) |
| T11-020 | Orden de constructores y destructores con contadores | [destructores](t11-destructores-noexcept.md), [constructores](t11-constructores.md) |
| T11-021 | Copia y movimiento con efectos observables | [constructores](t11-constructores.md) |
| T11-022 | Propiedad de objetos creados en una DLL | [propiedad](t11-objetos-y-propiedad.md) |

### T11-014: construcción parcial

Un local C++ solo se acepta si su constructor y su destructor no lanzan (`C++ local lifetime currently
requires a non-throwing constructor and destructor`). Así la construcción de un local no falla a
medias. La de los miembros y bases de un objeto la hace el propio constructor de Clang, que destruye
los ya construidos si uno posterior lanza. Destruir locales NativeSharp ya construidos cuando un
constructor posterior lanza necesita el desenrollado de la tanda 15 (T15-004).

## Pendientes que quedan fuera de la tanda 11

Cada uno se rechaza con diagnóstico o deja el nombre sin proyectar.

| Pendiente | Ítem | Continúa en |
|---|---|---|
| Constructores y destructores que pueden lanzar en locales | T11-006, T11-014 | T15-003, T15-004 |
| Bases, métodos virtuales y operadores miembro | T11-005 | tanda 12; operadores miembro en T12 con los métodos heredados |
| Objetos C++ de vida no trivial y métodos de instancia en una plantilla precompilada | T11-005, T11-009 | T13-001 |
| CRT de C++ en el ejecutable (`new`, `delete`, estáticas con guarda en una biblioteca estática) | T11-018 | T16-002, T16-003 |
| Copia implícita de un record no trivial, también si es trivialmente copiable | T11-015 | pendiente documentado |
| Movimiento en un record con constructor de copia | T11-016 | pendiente documentado |
| Retornos `const T&`, parámetros `T&&` distintos del movimiento | T11-005, T11-006 | pendiente documentado |
| Campos y métodos de instancia protegidos o privados: no se importan, sin diagnóstico de acceso | T11-004, T11-005 | T17-002 |
| Campos de una `union` C++ | T11-004 | pendiente documentado |
| `new T` de un tipo C++ en el heap desde NativeSharp | T11-018 | pendiente documentado |

No se modificó Clang ni NSLink. El contrato NSMeta vigente es el único.
