# Tanda 01.08 — Preflight ordinario de Host.Basic

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Componente | `Host.Basic` |
| Contrato | Primera mitad de `CB-BAS-012` |
| Casos | `TB-BAS-060` a `TB-BAS-063` |
| Código de producto | Sólo compilar y observar `ArtifactTransaction.cpp`; no modificar |
| Pruebas históricas | Protegidas; no modificar ni ejecutar |
| Configuraciones | Debug y Release del ejecutable consolidado |
| Acciones prohibidas | NSC, NSLink, baseline, batería histórica, cobertura, despliegue, remoto y `C:` |

## Implementación

`BasicArtifactPreflightModule.cpp` inicia el contrato de preflight con cuatro casos deterministas: stage ausente,
stage que es directorio, padre de destino ausente y destino existente que es directorio. Los dos primeros
preservan bytes previos del destino; el tercero demuestra creación del padre y publicación; el cuarto conserva
intactos tanto el directorio rechazado como el stage.

Cada caso usa una raíz única en el workspace actual y redirige temporales a `A:`. La limpieza ordena rutas por
profundidad textual y las elimina iterativamente, sin borrado recursivo. Los errores de stage se comparan de
forma exacta, incluida la ruta; el rechazo de destino verifica la categoría contractual
`destination-not-regular`.

## Exclusiones

- No se modificó `ArtifactTransaction.cpp` ni su cabecera pública.
- `TB-BAS-064..067` quedan para la tanda de preflight Windows controlado.
- `TB-BAS-058..059` permanecen asignados al futuro ejecutor `nsc_suite_controller+nsc`.
- No se ejecutó el compilador NativeSharp ni ninguna prueba histórica.

## Evidencia de cierre

- Paridad exacta de IDs, contrato, ejecutor, capa y perfil contra la asignación canónica: 4/4.
- Pruebas Python del controlador: 51/51 aprobadas.
- CTest de la suite nueva en Debug: 7/7 aprobadas.
- CTest de la suite nueva en Release: 7/7 aprobadas.
- Imports dinámicos de VCRUNTIME, MSVCP o UCRT: 0 en Debug y 0 en Release.
- Residuos de fixtures o staging: 0.
- Los hashes SHA-256 de los seis archivos protegidos coinciden con `CURSOR.json`.

La validación fue dirigida exclusivamente a la infraestructura y los casos nuevos. No se ejecutaron el
compilador NativeSharp, la batería histórica, minibaselines, baseline, cobertura, instalación ni despliegue.
