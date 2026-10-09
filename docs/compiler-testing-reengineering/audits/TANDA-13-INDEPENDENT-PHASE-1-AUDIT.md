# Tanda 13 — Segunda auditoría independiente de la Fase 1

## Dictamen

`HECHO COMPROBADO`: la Fase 1 queda **técnicamente lista para revisión del usuario**. La segunda auditoría
terminó con `PASS` y cero hallazgos abiertos después de reparar las inconsistencias documentales que detectó.

Este dictamen no declara cerrada la tarea: `SCOPE-LOCK.md` reserva esa decisión al usuario. Tampoco afirma que
existan pruebas nuevas implementadas, que el compilador haya sido construido o que alguna prueba haya sido
ejecutada.

## Alcance verificado

| Superficie | Resultado independiente |
|---|---:|
| Archivos de producción y soporte reenumerados desde Git | 974/974 |
| Componentes documentales | 57/57 |
| Símbolos con identificador único | 8,418/8,418 |
| Contratos con fuente resoluble | 1,309/1,309 |
| Casos lógicos diseñados | 10,193/10,193 |
| Casos asignados exactamente una vez | 10,193/10,193 |
| Olas de componentes | 57 componentes en 11 olas |
| Archivos self-host auditados | 29/29 |
| Superficie restante auditada | 421/421 |
| Archivos históricos congelados y con huella vigente | 1,658/1,658 |
| Archivos ajenos protegidos | 6/6 byte-idénticos |
| Hallazgos abiertos | 0 |

## Método independiente

El verificador `evidence/TANDA-13-INDEPENDENT-AUDIT.ps1` no consume como universo una salida de los generadores
de Tandas 11 o 12. Vuelve a enumerar con Git las raíces autorizadas por `TANDA-01-INVENTORY-METHOD.md`, ordena
las rutas de forma estable y calcula otra vez tamaño y SHA-256. Después coteja:

1. ruta, identificador, tamaño y huella de los 974 archivos;
2. igualdad propiedad por propiedad entre `production-files.csv` y `production-files.json`;
3. partición de archivos y líneas entre los 57 componentes;
4. unicidad y resolución de archivos, rutas y contratos de cada símbolo;
5. fuente, componente, documento y existencia de casos para cada contrato;
6. correspondencia 1:1 entre caso diseñado y asignación de suite;
7. conteos por componente de las 11 olas de implementación;
8. trazabilidad documental de todos los archivos de producción;
9. subconjuntos self-host y superficie restante contra el manifiesto físico;
10. existencia y SHA-256 de los 1,658 archivos históricos congelados;
11. huellas publicadas y los seis archivos modificados antes de esta fase.

La reenumeración queda en `inventory/independent-file-reenumeration.csv`; el resultado legible por máquina está
en `inventory/independent-audit-summary.json`.

## Hallazgos reparados antes del dictamen

La primera pasada rechazó el cierre. La reparación quedó automatizada en
`evidence/TANDA-13-INVENTORY-INTEGRITY-REPAIR.ps1` y es repetible.

- Se separó el identificador `SYM-005151`, que estaba duplicado entre Driver y LanguageService; el segundo
  símbolo quedó como `SYM-008402`.
- Se expandieron cuatro referencias humanas abreviadas por rangos a identificadores contractuales explícitos.
- Se incorporó trazabilidad física para 15 cabeceras públicas y dos artefactos self-host que no tenían una fila
  de símbolo propia.
- Se completaron 40 relaciones inversas contrato→símbolo que estaban vacías, usando exclusivamente símbolos
  que ya declaraban pertenecer al contrato.
- Se añadió la fila de cobertura omitida para `PF-000271` y se retiraron cinco marcadores `NO LEIDO` ya
  reemplazados por evidencia trazada.
- Se sincronizó el espejo JSON del inventario y se recalcularon sus huellas canónicas.
- La primera ejecución independiente detectó tres fuentes concatenadas sin separador; se corrigió la
  serialización y la segunda ejecución terminó con cero hallazgos.

## Disposiciones no directas

`HECHO COMPROBADO`: los 273 símbolos sin contrato directo pertenecen únicamente a archivos cuya disposición es
`UPSTREAM` o `EXCLUIDA`. El auditor rechaza cualquier símbolo sin contrato fuera de esas dos disposiciones. No se
promovió una dependencia vendorizada a contrato propio del compilador ni se ocultó un símbolo primario bajo esa
excepción.

## Frontera de la afirmación

- La evidencia es estática y corresponde al universo congelado de la Fase 1.
- No se modificó código del compilador, pruebas ni infraestructura histórica.
- No se construyó NSC, no se ejecutó CTest, baseline, minibaseline ni cobertura.
- Los 10,193 casos están diseñados y asignados; `implemented=0` y `executed=0`.
- La cobertura instrumental sigue perteneciendo a una fase posterior.
- La Fase 2 no empieza automáticamente con este dictamen.

## Resultado

No quedan omitidos, duplicados, desconocidos ni referencias sin tratamiento dentro del alcance autorizado. La
matriz documental permite pasar a revisión del usuario y, sólo después de su decisión, usar las 11 olas como
orden de implementación de la suite nueva.
