# Validación del bloque T08-010 (struct C)

Fecha: 2026-11-08. Compilador con los cambios de `544b39aa9` y de T08-010-10 (carril `tanda1-ON`).

## Resultado

Bloque validado. Su estado:

- **Cerradas:** T08-010-01 a 04, 06 y 10.
- **Bloqueadas:** T08-010-05, 07, 08 y 09, por la decisión pendiente T08-010-05a: qué modo volatile rige un
  alias NativeSharp de almacenamiento C `volatile`.

## Ejecución

Se usaron dos scripts:

- `A:\nsc-interop-build\claude\scripts\regresion-t08-010b.cmd`: pruebas nuevas, records, enum, typedef e
  identidad.
- `A:\nsc-interop-build\claude\scripts\validar-bloque-t08-010.cmd`: layout, plantillas y sombras.

| Prueba | Resultado |
|---|---|
| `record-c/validar-miembros.py`, `validar-mascara-bits.py`, `validar-etiqueta.py` (O0–O3 y rechazos) | correcto |
| `layout/validar.cmd`, `layout-clang/validar.cmd` (T08-010-01) | correcto |
| `anonimos-c/validar.cmd` (T08-010-02) | correcto |
| `flexible-c/validar.cmd`, `flexible-c/validar-decay.cmd` (T08-010-03) | correcto |
| `record-c/validar.cmd`, `union/validar.cmd`, `enum-campo/validar.cmd` | correcto |
| `record-template`, `global-template`, `alias-template`, `dependencias-template`, `funcion-template` | correcto |
| `sombras-plantilla-c/validar-ambito.cmd`, `validar-o0-inicializadores.cmd`, `validar-o0-sombras.cmd` | correcto |
| `volatile-campos-c`, `volatile-punteros-c`, `bool-campos-c`, `bool-puntero-c`, `global-puntero-const` | correcto |
| `bitfields-c` (`validar`, `validar-volatile`, `validar-volatile-metadata`) | correcto |
| `void-c/validar-void.py`, `globales-c/validar-enum-record.py`, `validar-atomicas.py` | correcto |
| `enteros-c/validar-anchos.py` | correcto |
| Regresión typedef: `typedef-c/*.cmd`, `enum-funcion/*.cmd`, identidad y plantillas | correcto |
| `pruebas-tanda-08-identidad-c/validar.cmd` (copia CRLF) | correcto |
| `tls-c/atomic/validar.cmd` | falla conocida, ajena al bloque |

## Fallo conocido de `tls-c/atomic/validar.cmd`

El paso de caché espera el mensaje «C/C++ project references require --no-artifact-cache…», que el commit
`03bf12e42` retiró. Sin ese paso, el resto pasa. Está anotado para la validación integral de la tanda 8.
