# Tanda 01.09 — Preflight Windows controlado de Host.Basic

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Componente | `Host.Basic` |
| Contrato | Segunda mitad y cierre de `CB-BAS-012` |
| Casos | `TB-BAS-064` a `TB-BAS-067` |
| Código de producto | Sólo compilar y observar `ArtifactTransaction.cpp`; no modificar |
| Pruebas históricas | Protegidas; no modificar ni ejecutar |
| Configuraciones | Debug y Release del ejecutable consolidado |
| Acciones prohibidas | NSC, NSLink, baseline, batería histórica, cobertura, despliegue, remoto y `C:` |

## Implementación

La tanda cubre cuatro fronteras Windows que requieren fixtures controlados: destino reparse point, límite de
ruta extendida, espacio insuficiente y fallo de copia del último sibling. Cada caso usa funciones de prueba
nombradas y una raíz aislada en `A:`. La limpieza conserva un inventario explícito, ordena hojas antes que
padres y utiliza llamadas Win32 de ruta extendida; no existe recorrido recursivo.

El reparse point es un junction creado mediante `FSCTL_SET_REPARSE_POINT`, sin depender de Developer Mode ni
del privilegio para symbolic links. El caso de espacio usa un stage acotado de nueve bytes y hace que el
ejecutable de prueba reporte cuatro bytes disponibles sólo para el directorio del fixture. El último caso
deriva el ordinal real con una transacción sonda y ocupa el nombre
del segundo sibling con un directorio. Así fuerza el fallo de copia después de preparar el primero y demuestra
que ningún destino entra en la fase de instalación; el directorio es parte del fixture, no un temporal creado
por el producto.

Un gate de controlador dedicado selecciona el contrato completo `CB-BAS-012` con perfil `WEEKLY` y exige los
ocho IDs `TB-BAS-060..067` en orden canónico.

## Exclusiones

- No se modificó `ArtifactTransaction.cpp` ni su cabecera pública.
- No se implementaron todavía casos de commit, rollback o helpers transaccionales posteriores.
- `TB-BAS-058..059` permanecen asignados al futuro ejecutor `nsc_suite_controller+nsc`.
- No se ejecutó el compilador NativeSharp ni ninguna prueba histórica.

## Incidencias cerradas durante la validación

El primer inyector de `TB-BAS-067` abrió el sibling con delete sharing. `DeleteFileW` retiró ese nombre y permitió
que `CopyFileW` creara otro archivo, por lo que no produjo el fallo diseñado. Se reemplazó únicamente el
inyector por un directorio preexistente: `DeleteFileW` no lo retira, la copia falla de forma determinista y el
directorio sigue siendo distinguible del sibling temporal que crea el producto.

La primera revisión del manifiesto usó dos valores fuera del esquema: `small` como timeout y `resource` como
clase. La auditoría completa de enumeraciones los corrigió a `normal` y `boundary`; no se amplió el esquema ni
se relajó su validador.

## Evidencia de cierre

- Paridad exacta de IDs, contrato, ejecutor, capa y perfil: 4/4 para esta tanda y 8/8 en `CB-BAS-012`.
- Pruebas Python del controlador: 51/51 aprobadas.
- Debug: build con `/W4 /WX` y CTest 8/8 aprobadas.
- Release: build con `/W4 /WX` y CTest 8/8 aprobadas.
- Gate aislado de `CB-BAS-012`: 8/8 en ambos modos.
- Imports dinámicos de VCRUNTIME, MSVCP o UCRT: 0 en Debug y 0 en Release.
- Residuos de fixtures, junctions o siblings: 0.
- Líneas mayores a 120 caracteres en código y documentación nuevos: 0; el manifiesto conserva JSONL canónico.
- Diferencial de código de producto bajo `src` e `include/nativesharp`: 0 archivos.
- Los hashes SHA-256 de los seis archivos protegidos coinciden con `CURSOR.json`.

La validación fue dirigida exclusivamente a la infraestructura y los casos nuevos. No se ejecutaron el
compilador NativeSharp, la batería histórica, minibaselines, baseline, cobertura, instalación ni despliegue.
