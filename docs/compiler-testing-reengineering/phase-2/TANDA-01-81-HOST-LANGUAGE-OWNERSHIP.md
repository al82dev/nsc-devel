# Tanda 01.81 - Host.Language: ownership, lifetimes y escape summaries

Fecha: 2026-10-09

## Alcance implementado

| Campo | Resultado |
|---|---|
| Componente | `Host.Language` |
| Contrato | `CL-LANG-017` |
| Casos implementados | 13/14: `TL-LANG-217..229` |
| Caso bloqueado | `TL-LANG-230` |
| Producto modificado | No |
| Suite histórica modificada | No |
| Estado del contrato | PARCIAL POR DEFECTOS DE PRODUCTO |

La tanda comprueba moves completos y parciales, estados y drop flags, overlap de
borrows, inferencia y orden de lifetimes, cleanup, pinning y serialización estable de
escape summaries. Los oráculos se derivan de los contratos publicados y no del resultado
de otra llamada al mismo helper.

## Contratos comprobados

| Casos | Comportamiento verificado |
|---|---|
| `TL-LANG-217..220` | No-op Copy, whole move y field move válidos, más fallos sin mutación. |
| `TL-LANG-221..222` | Clasificación exacta y drop flags sólo para estados parciales. |
| `TL-LANG-223..224` | Matriz de roots/projections y negación contractual para exclusividad. |
| `TL-LANG-225..226` | Prioridad de receiver, input único y orden de primera coincidencia. |
| `TL-LANG-227..228` | Cleanup inverso y whitelist exacta de operaciones que preservan pin. |
| `TL-LANG-229` | Bytes exactos y round-trip para identidades vacías/delimitadas y 0/N clases. |

## Bloqueo confirmado

### `TL-LANG-230`: el decoder acepta tokens parciales y conserva salida parcial al fallar

El caso exige rechazar versiones, longitudes, separadores, clases y trailing inválidos,
dejando siempre el output reiniciado. `decodeEscapeSummary` usa `std::from_chars`, pero
comprueba solamente `ec`: no exige que `ptr` alcance el final del token. Por ello una
longitud como `1x` o una clase como `0x` puede aceptar el prefijo numérico y omitir la
basura restante.

Además, el decoder agrega cada clase al output conforme la consume. Si después encuentra
un trailing sin coma, devuelve `false` pero deja en `summary.parameters` las clases ya
decodificadas. Esto contradice la garantía de output reiniciado en cualquier fallo.

- `BLOQUEADO_POR`: validación incompleta del consumo de `from_chars` y commit parcial del output.
- Resolverlo exige validar `ptr` y decodificar transaccionalmente en un valor temporal.
- La Fase 2 prohíbe cambiar producto; no se eligieron entradas inválidas débiles para simular éxito.
- El caso no fue agregado al manifiesto ni contado como implementado.

## Evidencia reproducible

- Build dirigido `nsc_component_tests`: Debug y Release correctos, con warnings como errores.
- Gate dirigido realizable: Debug 13/13 en 8.37 s; Release 13/13 en 0.78 s.
- Suite nueva completa: Debug 77/77 en 421.77 s; Release 77/77 en 42.12 s.
- Controlador Python: 51/51 en 0.705 s.
- Fuentes Python de la suite: 98/98 parseadas en memoria.
- Manifiesto: 667 casos, 0 duplicados y 13 casos implementados de `CL-LANG-017`.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos en los dos módulos nuevos.
- Dependencias PE Debug y Release: únicamente `KERNEL32.dll`; CRT dinámico: 0.
- `git diff --check`: correcto; código y configuración modificados con máximo de 120 columnas.
- Diferencial bajo `include/` y `src/`: 0 archivos.
- Los seis archivos preexistentes protegidos conservaron exactamente sus SHA-256.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Estado

`CL-LANG-017` queda con 13 casos implementados y verificados y un bloqueo de producto
documentado. La Fase 2 puede continuar con `CL-LANG-018`, pero no declarar esta ola
completa mientras `TL-LANG-230` siga bloqueado.
