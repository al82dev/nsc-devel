# Tanda 01.59 — Marcador de Host.Diagnostics

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\\nsc-devel` |
| Contrato | `CD-DIA-010`, casos `TD-DIA-053..063` |
| Producto | Observar línea y marcador de `DiagnosticRenderer::render`; no modificar |
| Excluido | Etiquetas, notas, renderizado de motores completos y correcciones del producto |
| Protegido | 684 pruebas históricas, seis cambios preexistentes, `C:` y remotos |

## Cobertura contractual

- columna uno y columna posterior con espaciado exacto;
- conservación de tabuladores antes del marcador;
- rangos de longitud uno, múltiples bytes y longitud cero;
- rango multilínea reducido al marcador mínimo;
- rango que termina después del texto visible sin recorte del marcador;
- línea vacía en columna uno y retorno temprano en una columna posterior;
- caracterización acotada del riesgo de underflow cuando un modelo directo entrega el fin del rango antes de
  la ubicación.

El renderizado observado es lineal en la longitud de la línea y del marcador. Los casos son iterativos y no
usan recursión. `TD-DIA-063` no ejecuta una salida potencialmente no acotada: demuestra únicamente las
precondiciones exactas que producirían el cálculo unsigned incoherente.

## Implementación de pruebas

- `DiagnosticRendererMarkerModule.cpp` registra once casos dentro del ejecutable consolidado.
- `run_host_diagnostics_renderer_marker_gate.py` exige exactamente `TD-DIA-053..063` y once aprobaciones.
- `cases.jsonl` conserva la trazabilidad `case_id -> CD-DIA-010 -> Host.Diagnostics` en Debug y Release.
- El renderizador se observa mediante la misma fuente de producto ya enlazada, sin modificarla.
- El caso `NIGHTLY` caracteriza el modelo directo incoherente sin materializar una cadena de tamaño no acotado.

## Evidencia

- Gate `CD-DIA-010`: 11/11 en Debug (3.46 s) y Release (0.56 s).
- Suite nueva completa: Debug 58/58 en 45.18 s; Release 58/58 en 8.97 s.
- Controlador Python: 51/51 en 0.711 s; fuentes Python: 79/79 compiladas en memoria.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos en código de usuario.
- Manifiesto: 433 casos, 0 duplicados y 11 casos del contrato.
- Builds con warnings como errores; diferencial de producto: 0 archivos.
- Dependencias dinámicas: sólo `KERNEL32.dll`; CRT dinámico: 0.
- Los seis archivos preexistentes conservaron exactamente sus SHA-256 registrados.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Trazabilidad

| Caso | Propósito |
|---|---|
| `TD-DIA-053` | línea ASCII en columna uno y marcador mínimo |
| `TD-DIA-054` | espacios exactos antes de una columna posterior |
| `TD-DIA-055` | tabulador de fuente conservado antes del marcador |
| `TD-DIA-056` | rango unitario con sólo caret |
| `TD-DIA-057` | rango ancho con caret y tildes |
| `TD-DIA-058` | rango vacío con marcador mínimo |
| `TD-DIA-059` | rango multilínea con marcador mínimo |
| `TD-DIA-060` | fin posterior al texto visible sin recorte |
| `TD-DIA-061` | línea vacía en columna uno |
| `TD-DIA-062` | línea visible vacía en columna posterior y retorno temprano |
| `TD-DIA-063` | riesgo unsigned de un modelo directo incoherente, sin renderizar |

Diagramas: no aplica; el contrato es una secuencia textual lineal.
