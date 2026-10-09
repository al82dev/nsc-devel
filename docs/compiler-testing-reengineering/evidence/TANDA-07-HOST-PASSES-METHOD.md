# Evidencia de método — Tanda 07, Host.Passes

## SCOPE LOCK aplicado

Trabajo exclusivamente documental dentro de `docs/compiler-testing-reengineering/**`. Código, pruebas,
infraestructura, C:, remotos y los seis cambios preexistentes permanecieron protegidos.

## Universo leído

| Grupo | Archivos | Líneas | Bytes |
|---|---:|---:|---:|
| headers Passes | 23 | 912 | 17,211 |
| sources Passes | 19 | 3,044 | 96,200 |
| total | 42 | 3,956 | 113,411 |

Se leyeron completos todos los headers y sources de `include/nativesharp/Passes/**` y `src/Passes/**`. La lectura
incluyó contratos de valor, ownership, cloning, plegado, efectos, reescritura, cinco transforms, análisis,
manager, dumps y verifier.

## Extracción y contraste

1. El manifiesto fijó 42 `PF-*` y sus huellas.
2. Se identificaron 25 tipos y 133 funciones/helpers: 158 símbolos físicos.
3. La extracción de miembros se limitó a la clase propietaria de cada `.cpp`; así se excluyeron llamadas
   estáticas que una búsqueda textual habría confundido con definiciones.
4. Se diseñaron 56 contratos observables.
5. Cada contrato recibió ocho matrices: 448 casos, 392 `PR` y 56 `NIGHTLY`.
6. La segunda pasada reconstruyó por separado la cadena builder → analyses → passes → verifier y contrastó cada
   transformación con sus invariantes declarados.

## Límites

No hubo build, ejecución, baseline, cobertura ni modificación de producto. `DESIGNED` no significa implementado.
