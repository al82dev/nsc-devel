# Tanda 01.05 — Clasificación de fallos de salida de Host.Basic

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Componente | `Host.Basic` |
| Contrato | `CB-BAS-010` |
| Casos | `TB-BAS-041` a `TB-BAS-048` |
| Código de producto | Sólo compilar y observar `ArtifactTransaction.cpp`; no modificar |
| Pruebas históricas | Protegidas; no modificar ni ejecutar |
| Configuraciones | Debug y Release del ejecutable consolidado |
| Acciones prohibidas | NSC, NSLink, baseline, batería histórica, cobertura, despliegue, remoto y `C:` |

## Implementación

`BasicOutputFailureModule.cpp` registra ocho casos unitarios sobre la API pública de clasificación. Cada grupo
enumera todos los códigos Win32 declarados por el contrato y verifica tanto el enum resultante como su nombre
estable: éxito, límite de ruta, componente ausente, permiso, bloqueo, espacio insuficiente e I/O genérico.

El último caso convierte deliberadamente un valor fuera del rango de `OutputFailureKind` y exige el fallback
`io-error`. La prueba de código Win32 desconocido usa `0xdeadbeef`, que no coincide con ninguna rama enumerada.
No se provocan fallos reales de filesystem porque esta tanda verifica únicamente la función pura de
clasificación; los fallos observables de transacciones pertenecen a contratos posteriores.

## Exclusiones

- No se modificó `ArtifactTransaction.cpp` ni su cabecera pública.
- No se implementaron staging, preflight, commit, rollback o escritura transaccional.
- No se ejecutó el compilador NativeSharp ni ninguna prueba histórica.

## Evidencia

- Paridad exacta de IDs, contrato, ejecutor, capa y perfil contra la asignación canónica: 8/8.
- Pruebas Python del controlador: 51/51 aprobadas.
- Debug: build correcto y 6/6 gates CTest aprobados.
- Release: build correcto y 6/6 gates CTest aprobados.
- Runtime host: cero imports dinámicos de VCRUNTIME, MSVCP o UCRT en Debug y Release.
- Los seis archivos protegidos conservaron exactamente sus hashes registrados en el cursor.
