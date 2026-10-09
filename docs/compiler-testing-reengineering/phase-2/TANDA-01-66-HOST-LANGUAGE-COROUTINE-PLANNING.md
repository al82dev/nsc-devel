# Tanda 01.66 - Host.Language: planificación básica de corrutinas

Fecha: 2026-10-09

## Alcance cerrado

| Campo | Resultado |
|---|---|
| Componente | `Host.Language` |
| Contrato | `CL-LANG-005` |
| Casos implementados | `TL-LANG-050` a `TL-LANG-056` (7/28 del contrato) |
| Producto modificado | No |
| Suite histórica modificada | No |
| Cobertura instrumental | Fuera de alcance de Fase 2 |
| Pendiente del contrato | `TL-LANG-057` a `TL-LANG-077` |

Esta subtanda incorpora pruebas contractuales para la planificación básica de funciones
asíncronas. No cambia la implementación del compilador: el ejecutable consolidado enlaza
las fuentes de producto existentes de `IterationSemantics` y `CoroutineSemantics` para
observar directamente sus contratos públicos.

## Contratos comprobados

| Caso | Comportamiento verificado |
|---|---|
| `TL-LANG-050` | Inicio perezoso y ausencia de heap implícito en los tres almacenamientos de frame. |
| `TL-LANG-051` | Cuatro combinaciones de pruebas independientes para préstamos a través de `await`. |
| `TL-LANG-052` | Plan síncrono sin `await`, local inactivo aceptado y rechazo exacto de un local vivo sin suspensión. |
| `TL-LANG-053` | Suspensiones contiguas, huecos, duplicados y matriz de evidencia de préstamos. |
| `TL-LANG-054` | Retorno vacío y válido, mapeo de almacenamientos y requisito de fijación por autorreferencia. |
| `TL-LANG-055` | Matriz completa de `FutureProtocol` y prioridad estable de diagnósticos. |
| `TL-LANG-056` | Contextos de `await` y prioridad de rechazo entre contexto no asíncrono, `comptime` y limpieza. |

## Integración

- Se añadió `LanguageCoroutinePlanningModule.cpp` al único ejecutable unitario consolidado.
- El registro central publica los siete casos con identidad estable.
- El manifiesto contiene una fila por caso y conserva cero identificadores duplicados.
- El controlador dirigido selecciona exclusivamente `CL-LANG-005` y exige la secuencia
  exacta `TL-LANG-050..056`.
- No se añadió otro ejecutable C++ ni se duplicó infraestructura.
- La implementación usa iteración; no introduce recursión.

## Evidencia reproducible

- Build dirigido `nsc_component_tests`: Debug y Release correctos, con warnings como errores.
- Gate dirigido: Debug 7/7 en 2.94 s; Release 7/7 en 0.47 s.
- Suite nueva completa: Debug 65/65 en 64.74 s; Release 65/65 en 11.29 s.
- Controlador Python: 51/51 en 0.793 s.
- Fuentes Python de la suite: 86/86 compiladas en memoria.
- Manifiesto: 498 casos, 0 duplicados y 7 casos de `CL-LANG-005`.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos; 143 avisos de
  dependencias no propias fueron suprimidos por la configuración estándar.
- Dependencias PE Debug y Release: únicamente `KERNEL32.dll`.
- `git diff --check`: correcto; fuentes con longitud máxima de 120 columnas.
- Diferencial bajo `include/` y `src/`: 0 archivos.
- Los seis archivos preexistentes protegidos conservaron exactamente sus SHA-256.

## Estado

La planificación básica de corrutinas queda verificada sin hallazgos de producto en estos
siete casos. `CL-LANG-005` permanece abierto porque aún faltan estado y cancelación,
generadores asíncronos y contratos de concurrencia (`TL-LANG-057..077`).
