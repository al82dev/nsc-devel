# Tanda 01.87 - Host.Basic: rollback de artefactos

Fecha: 2026-10-09

## Alcance implementado

| Campo | Resultado |
|---|---|
| Componente | `Host.Basic` |
| Contrato | `CB-BAS-014` |
| Casos implementados | 5/5: `TB-BAS-073..077` |
| Casos bloqueados | Ninguno |
| Producto modificado | No |
| Suite histórica modificada | No |
| Estado del contrato | CERRADO |

La tanda comprueba el rollback inverso de transacciones de artefactos cuando
falla un movimiento Win32 durante el commit. La inyección de fallos pertenece
exclusivamente al ejecutable consolidado de pruebas: reemplaza temporalmente su
importación de `MoveFileExW`, registra llamadas por ordinal y restaura la IAT
antes de inspeccionar resultados. No cambia el producto.

## Contratos comprobados

| Caso | Comportamiento verificado |
|---|---|
| `TB-BAS-073` | Si falla el segundo destino, el primero recupera sus bytes originales y no quedan residuos. |
| `TB-BAS-074` | Si falla el primer backup, las entradas posteriores permanecen intactas y el rollback queda completo. |
| `TB-BAS-075` | Si también falla la restauración, se conserva el backup recuperable y se informa `rollback-incomplete`. |
| `TB-BAS-076` | Un destino inicialmente ausente instalado antes del fallo se elimina durante el rollback. |
| `TB-BAS-077` | Dos destinos ya instalados se restauran en orden inverso comprobado por ordinal de llamada. |

## Frontera de la suite

- El manifiesto activo contiene 733 casos distribuidos únicamente entre
  `Host.Basic`, `Host.Source`, `Host.Target`, `Host.Diagnostics` y
  `Host.Language`.
- Componentes `Library.*`, SDK, bindings, NativeForms y repositorios externos:
  0 casos activos.
- La Fase 2 prueba exclusivamente NSC y el runtime básico alojado en este
  repositorio. Una eventual suite de bibliotecas debe vivir en el repositorio
  propietario y no se conserva aquí como opción.

## Evidencia reproducible

- Gate dirigido Debug: 5/5 en 0.57 s.
- Suite nueva completa final: Debug 83/83 en 51.31 s; Release 83/83 en 50.55 s.
- Controlador Python: 51/51 en 0.722 s.
- Fuentes Python de la suite: 104/104 parseadas en memoria.
- Manifiesto: 733 casos, 0 componentes externos y 5 casos de `CB-BAS-014`.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos en el módulo nuevo.
- Dependencias PE Debug y Release: únicamente `KERNEL32.dll`; CRT dinámico: 0.
- `git diff --check`: correcto; archivos nuevos con máximo de 120 columnas.
- Diferencial bajo `include/` y `src/`: 0 archivos.
- Los seis archivos preexistentes protegidos conservaron exactamente sus SHA-256.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Estado

`CB-BAS-014` queda cerrado con sus cinco casos implementados y verificados. Con
él terminan los casos ejecutables conocidos de la ola 1; permanecen visibles
los 14 casos bloqueados documentados, sin alterar el producto para forzarlos.
