# Tanda 01.70 - Host.Language: lifetime entre módulos

Fecha: 2026-10-09

## Alcance cerrado

| Campo | Resultado |
|---|---|
| Componente | `Host.Language` |
| Contrato | `CL-LANG-006` |
| Casos implementados | `TL-LANG-078` a `TL-LANG-087` (10 casos) |
| Producto modificado | No |
| Suite histórica modificada | No |
| Estado del contrato | LISTO |

La tanda verifica la construcción y validación del contrato de lifetime, su identidad
FNV-1a, el registro de objetos vivos, las barreras de descarga y la serialización del
estado compartido. El acceso blanco de prueba queda limitado exclusivamente a sembrar
`SIZE_MAX` en `TL-LANG-084`; no se añadió una puerta de pruebas a la API de producto.

## Contratos comprobados

| Caso | Comportamiento verificado |
|---|---|
| `TL-LANG-078` | ABI, identidades, política y tres nombres de thunk derivados con FNV-1a exacto. |
| `TL-LANG-079` | Contrato válido y cada identidad o thunk requerido ausente, con diagnóstico exacto. |
| `TL-LANG-080` | Compatibilidad del ABI: major incompatible tiene prioridad; minor permanece en el fingerprint. |
| `TL-LANG-081` | Fingerprint congelado, repetible y sensible a cada uno de los nueve campos contractuales. |
| `TL-LANG-082` | Primer registro, repeticiones, conteos 1..3 y fingerprint fijado por productor. |
| `TL-LANG-083` | Rechazo de contrato incompatible o módulo descargándose, sin mutar el conteo. |
| `TL-LANG-084` | Límite `SIZE_MAX`: error exacto, sin wrap ni mutación del contador. |
| `TL-LANG-085` | Liberación válida, productor desconocido, fingerprint distinto y contador en cero. |
| `TL-LANG-086` | Descarga con cero o dos objetos, diagnóstico exacto y transición final a unloading. |
| `TL-LANG-087` | Ocho hilos registran, liberan, rechazan mezclas y solicitan descarga bajo el mismo mutex. |

## Propiedades adicionales verificadas

- Los thunks para `Producer::Payload` terminan en `3e0a1ce29ea72ce1`.
- El contrato canónico tiene fingerprint `dcb5c8234fafc747`.
- Una revisión minor futura se acepta, pero cambia la identidad contractual.
- Los errores de validación no dejan diagnósticos anteriores en una operación exitosa.
- Un contrato incompatible nunca sustituye el fingerprint ya fijado por el productor.
- Las liberaciones inválidas no decrementan ni desbordan el contador.
- Las solicitudes concurrentes de descarga observan el conteo serializado y no lo mutan.
- No se introdujo recursión.

## Evidencia reproducible

- Build dirigido `nsc_component_tests`: Debug y Release correctos, con warnings como errores.
- Gate dirigido completo: Debug 10/10 en 4.39 s; Release 10/10 en 0.57 s.
- Suite nueva completa: Debug 66/66 en 72.45 s; Release 66/66 en 8.75 s.
- Controlador Python: 51/51 en 0.770 s.
- Fuentes Python de la suite: 87/87 compiladas en memoria.
- Manifiesto: 529 casos, 0 duplicados y 10 casos de `CL-LANG-006`.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos; 143 avisos de
  dependencias no propias suprimidos.
- Dependencias PE Debug y Release: únicamente `KERNEL32.dll`; CRT dinámico: 0.
- `git diff --check`: correcto; fuentes nuevas o modificadas con máximo de 120 columnas.
- Diferencial bajo `include/` y `src/`: 0 archivos.
- Los seis archivos preexistentes protegidos conservaron exactamente sus SHA-256.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Estado

`CL-LANG-006` queda completo: 10 casos implementados y verificados, sin hallazgos de
producto ni pendientes internos. La continuación de Ola 1 debe tomar `CL-LANG-007`.
