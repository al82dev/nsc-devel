# Tanda 01.07 — Ciclo de vida del staging de Host.Basic

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Componente | `Host.Basic` |
| Contrato | Segunda mitad y cierre de `CB-BAS-011` |
| Casos | `TB-BAS-054` a `TB-BAS-057` |
| Código de producto | Sólo compilar y observar `ArtifactTransaction.cpp`; no modificar |
| Pruebas históricas | Protegidas; no modificar ni ejecutar |
| Configuraciones | Debug y Release del ejecutable consolidado |
| Acciones prohibidas | NSC, NSLink, baseline, batería histórica, cobertura, despliegue, remoto y `C:` |

## Implementación

La segunda mitad del módulo de staging cubre los cien candidatos ocupados, validación de rutas vacías en
`addStaged`, registro diferido sin existencia inmediata y limpieza del staging al destruir una transacción sin
commit.

El agotamiento no supone un ordinal inicial: crea primero una transacción sonda, deriva el siguiente ordinal y
ocupa iterativamente los cien nombres exactos que consumirá el algoritmo. Cada colisión es un archivo para
producir `file_exists`; la prueba exige fallo, stage vacío y prefijo de error contractual. El caso de destructor
escribe contenido real dentro del stage y comprueba que desaparece la raíz completa al salir del alcance.

El gate dedicado selecciona exactamente `CB-BAS-011` con perfil `WEEKLY`, por lo que ejecuta `049..057` caso por
caso en workspaces nuevos de `A:`.

## Exclusiones

- No se modificó `ArtifactTransaction.cpp` ni su cabecera pública.
- No se ejecutó todavía ningún contrato de preflight, commit o rollback.
- No se ejecutó el compilador NativeSharp ni ninguna prueba histórica.

## Evidencia

- Paridad exacta de IDs, contrato, ejecutor, capa y perfil contra la asignación canónica: 4/4 para esta tanda y
  9/9 acumulados en `CB-BAS-011`.
- Pruebas Python del controlador: 51/51 aprobadas.
- Debug: build correcto y 7/7 gates CTest aprobados.
- Release: build correcto y 7/7 gates CTest aprobados.
- Gate aislado de `CB-BAS-011`: 9/9 `PASS` en ambos modos.
- Runtime host: cero imports dinámicos de VCRUNTIME, MSVCP o UCRT en Debug y Release.
- Residuos de staging creados por estos casos: 0.
- Los seis archivos protegidos conservaron exactamente sus hashes registrados en el cursor.
