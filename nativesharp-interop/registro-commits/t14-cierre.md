# Tanda 14: cierre

## Criterio

El mismo de las tandas [10](t10-cierre.md) a [13](t13-cierre.md). Una casilla T14 se cierra cuando:

- la capacidad que pide tiene evidencia propia frente a un control compilado por Clang 23.1.0, en
  biblioteca estática y DLL, O0–O3;
- el preprocesador, el evaluador de constantes y el layout los aplica Clang en la sesión, y cada
  declaración que NSC llama usa el símbolo y los tipos físicos de Clang;
- cada forma que NSC no proyecta se rechaza con diagnóstico (los de Clang cuando la rechaza Clang), sin
  emitir otra llamada ni otro almacenamiento, y queda registrada abajo como pendiente, con la tanda o el
  ítem que la continúa.

Un límite registrado no es interoperabilidad terminada (T18-007).

## Casillas

| Ítem | Capacidad comprobada | Registro |
|---|---|---|
| T14-001 | Macros de objeto del encabezado y sus inclusiones clasificadas con el preprocesador de Clang; de función, del sistema y de la línea de órdenes distinguidas | [macros](t14-macros.md) |
| T14-002 | Macros de objeto con valor escalar evaluado por Clang, con su tipo, en C y C++ | [macros](t14-macros.md) |
| T14-003 | Macros de función y sin valor escalar rechazadas al usarlas, con el motivo y la alternativa | [macros](t14-macros.md) |
| T14-004 | `Define` y `Undefine` deciden las condiciones `#if`, igual que en el control de Clang | [configuración](t14-configuracion.md) |
| T14-005 | Cada definición entra en la identidad de la sesión; con caché, cada cambio da el resultado vigente | [configuración](t14-configuracion.md) |
| T14-006 | Bit-fields de records C++ con las coordenadas de Clang | [registros](t14-registros.md) |
| T14-007 | Punteros a miembro sin tipo NativeSharp; la llamada, calificada o no, dice por qué | [especiales](t14-especiales.md) |
| T14-008 | `[[gnu::packed]]`, `#pragma pack`, `alignas`, `__declspec(align)`, `[[msvc::no_unique_address]]`; los atributos de llamada, en la tanda 9 | [registros](t14-registros.md) |
| T14-009 | La sesión no lee `.pch`; una biblioteca compilada con header precompilado se consume desde su encabezado | [especiales](t14-especiales.md) |
| T14-010 | `import` de un módulo rechazado con el diagnóstico de Clang; diseño del consumo de interfaces | [especiales](t14-especiales.md) |
| T14-011 | Un cambio en un encabezado incluido indirectamente invalida el resultado guardado | [configuración](t14-configuracion.md) |

## Pendientes que quedan fuera de la tanda 14

Cada uno se rechaza con diagnóstico o deja el nombre sin proyectar.

| Pendiente | Ítem | Continúa en |
|---|---|---|
| Expandir macros de función en código NativeSharp | T14-003 | pendiente documentado |
| Macros que se expanden a cadenas, punteros, records o tipos | T14-002 | pendiente documentado |
| Bit-fields de tipo enum o de más de 64 bits | T14-006 | pendiente documentado |
| Un portador NativeSharp para punteros a miembro | T14-007 | pendiente documentado |
| Bibliotecas que solo distribuyen el `.pch`, sin encabezado | T14-009 | pendiente documentado |
| Consumo de interfaces de módulo `.pcm` (diseño en [especiales](t14-especiales.md)) | T14-010 | pendiente documentado |

No se modificó Clang ni NSLink. El contrato NSMeta vigente es el único.
