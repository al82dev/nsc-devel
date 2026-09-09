# NativeSharp 1.0.1 — baseline certificado de strings y enums

## Alcance

Esta acta registra el baseline completo posterior a los cambios de NativeSharp 1.0.1
relacionados con:

- expresiones constantes en inicializadores enum;
- asignación de auto-propiedades `get` dentro del constructor;
- comparación de enums con el literal cero;
- unión condicional de strings nativos;
- préstamos de literales y strings owned frescos a parámetros `borrowed string` de
  métodos y constructores;
- ejecución del baseline G00 desde consola.

## Primera corrida: defecto descubierto

La corrida `20260908-204545-ec8fea99` terminó en `failed` después de 768.457 segundos.
Los seis fallos de CTest/fixture en los dos enlazadores y optimizaciones reducían al
mismo defecto: un literal positivo sin sufijo por encima de `long.MaxValue`, incluido
`18446744073709551615`, era clasificado como `long` antes de validar un enum con base
`ulong`. El Gate 0 también falló, elevando el total del reporte a siete.

No se ocultó el caso ni se modificó su valor. La corrección quedó registrada en
`161_1_0_1_literal_positivo_ulong_y_rango_enum.md` y conserva el caso especial
`-9223372036854775808` como `long`.

## Corrida certificada

- Run ID: `20260908-210330-3a9390d3`.
- Inicio: `2026-09-08T21:03:30-06:00`.
- Fin: `2026-09-08T21:14:53-06:00`.
- Duración: `682.304 s`.
- Fallos: `0`.
- Gate: `G00`.
- Estado: `LANDED`.
- Certificado: `true`.

### Matriz

| Grupo | lld-link | NSLink |
|---|---:|---:|
| CTest | 306/306 | 306/306 |
| Fixtures (`-O0` y `-O2`) | 148/148 | 148/148 |
| Bug triage (`-O0` y `-O2`) | 18/18 | 18/18 |

### Etapas

| Etapa | Resultado | Tiempo |
|---|---:|---:|
| Construir | passed | 0.916 s |
| Instalar | passed | 0.302 s |
| Construir CTest [lld-link] | passed | 2.457 s |
| Construir CTest [NSLink] | passed | 2.431 s |
| Sincronizar bibliotecas | passed | 0.056 s |
| CTest [lld-link] | passed | 137.962 s |
| Fixtures [lld-link] | passed | 167.223 s |
| Bug triage [lld-link] | passed | 28.628 s |
| CTest [NSLink] | passed | 129.326 s |
| Fixtures [NSLink] | passed | 173.632 s |
| Bug triage [NSLink] | passed | 33.560 s |
| Certificar baseline (Gate 0) | passed | 2.614 s |

## Identidad reproducible

- Compilador construido e instalado:
  `nsc 0.9.0+build.20260909T030331Z.cbacb6c12a1e60f5 (NativeSharp 0.1, stage 67)`.
- SHA-256 de `build-clang/bin/nsc.exe`:
  `c80b474240e895423a1f19d877946eff89a594ab8ff5ea5053547ca0c0ae4486`.
- SHA-256 de `C:\native-sharp\bin\nsc.exe`:
  `c80b474240e895423a1f19d877946eff89a594ab8ff5ea5053547ca0c0ae4486`.
- Archivos fuente medidos en S0 y S1: `1039`.
- Digest S0 y S1:
  `9a867a52543a509237fff3ec44d1a455f3778306387421aebf4a34f0a7e266ca`.

La igualdad S0/S1 demuestra que la matriz no modificó el conjunto de fuentes durante
la certificación. La igualdad de los binarios construido e instalado demuestra que
las pruebas se asociaron al compilador colocado en `C:\native-sharp`.

## Evidencia

- Registro versionado del gate: `_abi/gates/G00/gate.json`.
- Reporte completo:
  `_nscbuilder/history/20260908-210330-3a9390d3/report.md`.
- Metadata de la corrida:
  `_nscbuilder/history/20260908-210330-3a9390d3/metadata.json`.
- Logs por etapa:
  `_nscbuilder/history/20260908-210330-3a9390d3/stages/`.
- Reporte rojo conservado:
  `_nscbuilder/history/20260908-204545-ec8fea99/report.md`.

Los directorios de construcción y los ejecutables generados no forman parte del
commit del acta. Son productos reproducibles; la evidencia estable que se versiona es
el registro G00 y este resumen humano.

## Referencia de commit / Commit reference

**Título ES:** `Registra el baseline certificado de NativeSharp 1.0.1`

**English title:** `Record the certified NativeSharp 1.0.1 baseline`

**Descripción ES:**

> Registra la corrida G00 completa de strings y enums con ambos enlazadores y niveles
> de optimización. Conserva la corrida roja que descubrió el rango `ulong`, la identidad
> exacta del compilador, los hashes S0/S1 y el resultado final `LANDED`.

**English description:**

> Record the complete G00 strings-and-enums run with both linkers and optimization
> levels. Preserve the red run that exposed the `ulong` range issue, the exact compiler
> identity, the S0/S1 hashes, and the final `LANDED` result.
