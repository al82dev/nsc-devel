# Tanda 01.04 — SHA-256 de archivos de Host.Basic

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Componente | `Host.Basic` |
| Contrato | `CB-BAS-008` |
| Casos | `TB-BAS-035` a `TB-BAS-040` |
| Código de producto | Sólo compilar y observar `src/Basic/Sha256.cpp`; no modificar |
| Pruebas históricas | Protegidas; no modificar ni ejecutar |
| Configuraciones | Debug y Release del ejecutable consolidado |
| Acciones prohibidas | NSC, NSLink, baseline, batería histórica, cobertura, despliegue, remoto y `C:` |

## Implementación

`BasicSha256FileModule.cpp` materializa seis casos de filesystem en raíces aisladas bajo el directorio de
trabajo de cada ejecución. Comprueba archivo vacío, bytes binarios con NUL, ruta ausente, ruta a directorio,
archivo bloqueado de forma exclusiva y dos mediciones separadas por una mutación del archivo.

Los casos exitosos verifican simultáneamente resultado, tamaño, digest independiente y limpieza del error. Los
casos fallidos inicializan digest y tamaño con sentinelas y exigen que no se publiquen valores nuevos. El caso
de acceso rechazado usa un handle Win32 con exclusión de compartición; no depende de ACLs, cuenta de usuario ni
permisos accidentales del equipo.

La limpieza recorre iterativamente las rutas creadas y nunca aplica borrado recursivo a una ruta calculada. Un
gate del controlador selecciona exactamente `CB-BAS-008` con perfil `WEEKLY`, por lo que ejecuta los seis casos
individualmente dentro de workspaces nuevos en `A:`.

## Exclusiones

- No se modificó `Sha256.cpp` ni su cabecera pública.
- No se simularon fallos de filesystem ni se añadieron hooks al producto.
- No se cambió la infraestructura histórica ni se ejecutó el compilador NativeSharp.
- El caso de mutación cubre mediciones completas antes y después del cambio; no afirma atomicidad frente a una
  escritura concurrente durante una misma lectura, porque el contrato vigente no la ofrece.

## Evidencia

- Paridad exacta de IDs, contrato, ejecutor, capa y perfil contra la asignación canónica: 6/6.
- Pruebas Python del controlador: 51/51 aprobadas.
- Debug: build correcto y 6/6 gates CTest aprobados.
- Release: build correcto y 6/6 gates CTest aprobados.
- Gate aislado del contrato `CB-BAS-008`: 6/6 `PASS` en ambos modos.
- Runtime host: cero imports dinámicos de VCRUNTIME, MSVCP o UCRT en Debug y Release.
- Residuos de fixtures `.nsc-suite-TB-BAS-*` después de ambos gates: 0.
- Los seis archivos protegidos conservaron exactamente sus hashes registrados en el cursor.
