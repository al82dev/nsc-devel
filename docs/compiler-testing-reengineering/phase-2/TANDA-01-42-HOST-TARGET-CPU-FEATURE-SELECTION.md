# Tanda 01.42 — Selección y fallback de CPU x64 en Host.Target

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Contrato | `CF-TGT-023`, casos `TF-TGT-177..184` |
| Producto | Observar selección de niveles CPU de `TargetInfo`; no modificar |
| Excluido | Identidad general, nombres, strings LLVM y soporte de instrucciones |
| Protegido | 684 pruebas históricas, seis cambios preexistentes, `C:` y remotos |

## Cobertura contractual

- baseline solicitado y disponible selecciona baseline sin fallback;
- x86-64-v4 solicitado con v2 disponible selecciona v2 y activa fallback;
- el factory predeterminado funciona sin configuración externa y usa baseline;
- valores enum desconocidos conservan el orden por valor subyacente vigente;
- los extremos baseline/v4 y los niveles superiores iguales respetan límite y bandera;
- las 16 combinaciones de niveles conocidos seleccionan el mínimo ordinal;
- repetición y cambio del orden de invocación producen resultados idénticos;
- 65,536 selecciones recorren 25 combinaciones, incluido el máximo del tipo subyacente.

El oráculo calcula de forma independiente el mínimo por ordinal y deriva la bandera comparando nivel solicitado
contra seleccionado. El valor desconocido se construye por su representación subyacente para conservar el contrato
sin desactivar diagnósticos del analizador. Toda la ejecución es iterativa.

## Evidencia

- Gate `CF-TGT-023`: 8/8 en Debug (1.81 s) y Release (0.48 s).
- Suite nueva completa: Debug 41/41 en 16.28 s; Release 41/41 en 7.75 s.
- Controlador Python: 51/51; fuentes Python: 62/62 compiladas en memoria.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos.
- Manifiesto: 314 casos, 0 duplicados y 8 casos del contrato.
- Builds con warnings como errores; diferencial de producto: 0 archivos.
- Dependencias dinámicas: sólo `KERNEL32.dll`; CRT dinámico: 0.
- Los seis archivos preexistentes conservaron exactamente sus SHA-256 registrados.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Trazabilidad

| Caso | Propósito |
|---|---|
| `TF-TGT-177` | selección baseline mínima |
| `TF-TGT-178` | fallback representativo v4 a v2 |
| `TF-TGT-179` | configuración predeterminada sin prerrequisito |
| `TF-TGT-180` | orden vigente de valores enum desconocidos |
| `TF-TGT-181` | límites inferior, superior e igualdad |
| `TF-TGT-182` | matriz completa de niveles conocidos |
| `TF-TGT-183` | determinismo y orden de invocación |
| `TF-TGT-184` | escala y valor subyacente máximo |

Diagramas: no aplica; el contrato es una selección escalar sin transición de estado persistente.
