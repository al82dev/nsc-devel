# Validación 259 — identidad independiente del baseline (`TB-BAS-022`)

## Estado

`CORRECCIÓN IMPLEMENTADA; VALIDACIÓN FOCAL DEBUG/RELEASE COMPLETA; MATRIZ COMPLETA PENDIENTE DEL USUARIO EN GUI`.

## Alcance

La corrida de baseline `20261016-002156-4e066a71` ejecutó `13,134` variantes:

- `13,132` terminaron en `PASS`;
- `TB-BAS-022|Debug|NA` terminó en `FAIL`;
- `TB-BAS-022|Release|NA` terminó en `FAIL`.

Las dos fallas fueron idénticas:

```text
$.build_ids_distinct: value-mismatch; expected=True; actual=False
$.versions_distinct: value-mismatch; expected=True; actual=False
```

Este expediente cubre exclusivamente la causa común de esas dos variantes. No cambia el lenguaje, NSMETA,
manifiestos, contratos, oráculos ni los otros `13,132` resultados.

## Causa demostrada

El worker de baseline reconstruía e instalaba el compilador sobre la misma ubicación seleccionada como
`comparison_nsc`. Cuando se ejecutaba `TB-BAS-022`, el compilador primario y el comparador eran el mismo archivo:

- ruta observada: `A:\native-sharp-3.0-baseline\bin\nsc.exe`;
- SHA-256 observado: `2b7c63eb24da419eb7d454903c1f7571f0abcd24c885d946b7eea9660728be21`;
- identidad observada: `3.0.0+build.19700101T000000Z.543c12840ed57b2d`.

Además, el worker sustituía `-DNSC_BUILD_ID_OVERRIDE=` por los primeros 16 dígitos de la huella del árbol fuente.
Dos construcciones del mismo árbol recibían por diseño la misma identidad, contrario al contrato de
`TB-BAS-022`, que exige identidad física y lógica distinta por build.

## Corrección mínima

- `BaselinePlan` conserva la fuente de comparación previa al rebuild.
- El worker copia ese compilador a la evidencia de preparación antes de configurar, reconstruir o reiniciar el
  staging.
- La matriz recibe el snapshot inmutable como `comparison_nsc`.
- Se retiró la sustitución de `NSC_BUILD_ID_OVERRIDE`; el valor vacío conserva la generación ordinaria de una
  identidad única por construcción.
- El snapshot exige origen existente en `A:`, destino en `A:`, rutas distintas y paridad SHA-256 después de la
  copia.

No se debilitó ni modificó el oráculo de `TB-BAS-022`.

## Validación ejecutada

### Builder

- pruebas focales de operaciones de release/baseline: `9/9`, `OK`;
- suite modular completa del Builder: `72/72`, `OK`;
- `git diff --check`: sin errores.

### Reproducción real de `TB-BAS-022`

Se preservó el comparador previo en:

```text
A:\nsc-devel\_nscbuilder3\history\20261016-codex-tb-bas-022\preparation\comparison-nsc.exe
```

Después se configuró y construyó únicamente el target `nsc` en
`A:\nsc-devel\build-clang`, sin instalar ni promover el resultado.

Identidades observadas:

| Rol | SHA-256 | `compiler_version` | `build` |
|---|---|---|---|
| Primario reconstruido | `7e8fee5466828031daf5f448633f6462314616ae787f9f59cf4b4aae66e0ba50` | `3.0.0+build.20261016T074909Z.076c670cee7875df` | `076c670cee7875df` |
| Comparador preservado | `2b7c63eb24da419eb7d454903c1f7571f0abcd24c885d946b7eea9660728be21` | `3.0.0+build.19700101T000000Z.543c12840ed57b2d` | `543c12840ed57b2d` |

Resultados focales:

| Variante | Estado | Conteo |
|---|---|---:|
| `TB-BAS-022|Debug|NA` | `PASS` | `1/1` |
| `TB-BAS-022|Release|NA` | `PASS` | `1/1` |

Recibos:

- `_nscbuilder3/history/20261016-codex-tb-bas-022/debug/codex-tb-bas-022-debug/results.json`;
- `_nscbuilder3/history/20261016-codex-tb-bas-022/release/codex-tb-bas-022-release/results.json`.

## Límites de la afirmación

- No se ejecutó nuevamente la matriz completa de `13,134` variantes.
- No se instaló el compilador reconstruido en el staging fijo del baseline.
- No se ejecutó promoción ni escritura en `C:`.
- El cierre amplio permanece pendiente de la siguiente corrida del usuario desde la GUI actualizada.

