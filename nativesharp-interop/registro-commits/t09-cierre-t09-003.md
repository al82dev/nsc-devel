# T09-003: cierre

## Criterio

T09-003 pide conservar el símbolo y la convención de llamada que elige Clang. Se cierra cuando:

- cada capacidad registrada como pendiente en la validación agrupada tiene evidencia;
- cada declaración que NSC acepta usa el símbolo y la convención de Clang;
- cada forma que NSC no proyecta se rechaza con diagnóstico, sin emitir otra convención.

La lista de capacidades sin evidencia de
[TANDA-09-VALIDACION-AGRUPADA.md](../TANDA-09-VALIDACION-AGRUPADA.md) se vació con los cambios
registrados del 2026-11-14. Los últimos fueron:

- [records C++ en punteros `regcall`](t09-cpp-regcall-puntero-records.md);
- [records empaquetados y campos `char` en el replay](t09-replay-empaquetado-char.md);
- [atributos de alineación en el replay](t09-replay-alineacion.md).

## Evidencia de conjunto

- [Matriz de convenciones](t09-matriz-convenciones-win64.md): con los 15 atributos de convención
  de Clang 23.1 en Windows x64, en C y C++, NSC no acepta ninguna convención distinta de la emitida
  por Clang, en llamada directa ni por puntero.
- [Matriz de firmas C](t09-matriz-firmas-c.md): 7 convenciones por 18 clases de firma, directa y
  por puntero. Pasan las 252 celdas con la convención de Clang y valores correctos.
- La regresión final de la tanda, con todos los fixtures `pruebas-tanda-09-*` y los controles
  CTest, se resume en la validación agrupada.

## Límites que quedan fuera de T09-003

Cada uno se rechaza con diagnóstico. Ninguno cambia la convención emitida.

| Límite | Motivo | Continúa en |
|---|---|---|
| Punteros a función `swiftcall`, `swiftasynccall` e `intel_ocl_bicc` | NativeSharp no tiene tipos de función con esas convenciones; requiere una extensión del lenguaje | fuera de la interoperabilidad: extensión del lenguaje |
| Records por valor y variádicas con `swiftcall`, `swiftasynccall` e `intel_ocl_bicc` | Solo se valida el plan escalar y de punteros ([registro](t09-convenciones-planificadas.md)) | límite documentado de convenciones |
| `intel_ocl_bicc` en C++ | Clang 23.1.0 no lo decora (`cannot mangle this calling convention yet`) | ninguno: no hay símbolo que consumir |
| Variádicas C++ `preserve_none` | El propio control de Clang falla con `0xC0000005` ([observación](t09-preserve-none-variadica-observacion.md)); no se modifica Clang | ninguno mientras Clang no cambie |
| Enums anidados en clases | Nombres anidados C++ | tanda 10 |
| Records C++ no triviales por valor, uniones C++ por valor y acceso a campos de un record C++ devuelto sin guardar | Clases C++ y vida de objetos | tanda 11 |
| `__unaligned`, atributos distintos de la alineación, `#pragma pack` y alineación con campos de bits, sin encabezado | El catálogo no los guarda y el replay no los reconstruye; con encabezado pasan | límite documentado del replay |
| Tamaños de objeto ocultos (`pass_object_size`) en formas no registradas | Cada forma comprobada tiene su registro (`t09-tamano-*.md`) | límite documentado |
| Flujos de receptor C++ en plantillas precompiladas no registrados | Solo los flujos comprobados (`t09-receptor-*.md`, `t09-fabrica*.md`) | tanda 11 |

Los dos guiones de Clang `pruebas-tanda-09-cpp-variadica-preserve-none` fallan por diseño: reproducen
el fallo del control de Clang. No son pruebas de soporte NSC.

No se modificó Clang ni NSLink. El contrato NSMeta vigente es el único.
