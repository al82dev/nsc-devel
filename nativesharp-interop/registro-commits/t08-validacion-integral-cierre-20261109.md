# T08: validación integral tras las decisiones T08-D (2026-11-09, tarde)

Compilador: `A:\nsc-interop-build\tanda1-ON`, con los commits hasta `a979e7b0e` (T08-D24). Script:
`A:\nsc-interop-build\claude\scripts\integral-cierre.cmd`. Ejecuta los dos gates seguidos y deja su salida en
`claude\logs\integral-historico.log` e `integral-complementario.log`.

## Gate complementario ampliado

`validar-tanda-08-complementaria.cmd` también ejecuta ahora las suites nuevas de las decisiones T08-D que el gate
histórico no ejecuta. Antes eran 59 suites Python; ahora son 71.

| Suite | Decisión |
|---|---|
| `arreglos-c\validar-arreglos-punteros-campo.py` | D23 |
| `callbacks-c\validar-identidad-funcion-c.py` | D18 |
| `callbacks-c\validar-vectorcall.py` | D22 |
| `colisiones-c\validar-dependencias-privadas.py` | D19 |
| `globales-c\validar-atomicas-record.py` | D07 |
| `incompletos-c\validar-opaco-consumidor.py` | D16 |
| `infraestructura\validar-finales-cmd.py` | D08 |
| `layout-c\validar-alineacion-efectiva.py` | D15 |
| `punteros-c\validar-puntero-a-puntero-const.py` | D17 |
| `typedef-c\validar-record-dos-encabezados.py` | D09 |
| `typedef-c\validar-typedef-productores.py` | D10 |
| `union-c\validar-miembro-sin-etiqueta.py` | D14 |

Las suites de D01–D06 ya estaban en la lista.

## Primera ejecución

- **Gate complementario: código 0.**
- **Gate histórico: un fallo.** En `identidad-c\typedef-funcion-record\validar.cmd`, el caso
  `validar-enum.py 0 --mismatch` falló con «se esperaba rechazar enums incompatibles».

  - El caso usa dos productores precompilados. Cada uno tiene su `VarFn` sobre un `enum CKind` con valores
    distintos.
  - La prueba esperaba «conflicting C/C++ type alias 'VarFn'». Es el contrato que T08-D10 reemplazó: cada plantilla
    precompilada resuelve el typedef desde el catálogo de su productor.
  - T08-D10 actualizó las variantes de `validar.py` (`--incompatible`, `--same-size-incompatible`, `--const-mismatch`
    y `--header-changed`), pero no esta variante de `validar-enum.py`.
  - Se descartó T08-D24 como causa: con el `LibraryMetadataConstruction.cpp` anterior a D24, el caso falla igual.

**Corrección (punto 6 del usuario).** En `validar-enum.py --mismatch` (también con `--returns`), el consumidor
compone los dos productores y debe ejecutarse, como en las demás variantes incompatibles de D10.

## Segunda ejecución

- **Gate histórico `validar-tanda-08.cmd`: código 0.**
- **Gate complementario `validar-tanda-08-complementaria.cmd`: código 0.**

Por eso T08-GATE-03 queda cerrada. T08-D20 resolvió T08-GATE-03a con la actualización de los validadores
históricos.

## Fuera de los gates

ctest, con el entorno de las pruebas:

- pasan `nsc.cpp-interop.session`, `nsc.item343` a `nsc.item345`, `nsc.namespace-function-metadata`,
  `nsc.stage50_9`, `nsc.stage48_1`, `nsc.stage48_3`, `nsc.stage48_4`, `nsc.stage48-2.*`,
  `nsc.library-metadata-version-tolerance` y `nsc.library-metadata-merger`;
- `nsc.stage48_2.library-metadata` pasa 442 de 444. Las dos comprobaciones que fallan leen el fixture
  `historical-1.46.nsmeta`, que no está versionado.

## Qué sigue abierto

| Pendiente | Qué falta |
|---|---|
| D04b | aprobación de la copia distribuida con directorio de importación vacío |
| D05b | aprobación del enlace en ejecución del alias importado directamente |
| D11, D12 y D13 | confirmación de la interpretación enviada al usuario. D13 bloquea T08-010-05/07/08/09, la mayoría de T08-015 y T08-020-03c |
| D21 / T08-GATE-05a | criterio de reagrupación de commits. El historial no se reescribió; el respaldo es el bundle |
