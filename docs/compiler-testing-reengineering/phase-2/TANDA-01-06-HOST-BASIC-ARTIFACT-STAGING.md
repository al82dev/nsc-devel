# Tanda 01.06 — Creación de staging de Host.Basic

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Componente | `Host.Basic` |
| Contrato | Primera mitad de `CB-BAS-011` |
| Casos | `TB-BAS-049` a `TB-BAS-053` |
| Código de producto | Sólo compilar y observar `ArtifactTransaction.cpp`; no modificar |
| Pruebas históricas | Protegidas; no modificar ni ejecutar |
| Configuraciones | Debug y Release del ejecutable consolidado |
| Acciones prohibidas | NSC, NSLink, baseline, batería histórica, cobertura, despliegue, remoto y `C:` |

## Implementación

`BasicArtifactStagingModule.cpp` comienza el contrato de staging con cinco casos: destino vacío, primer destino
válido, dos destinos con el mismo basename, rutas Unicode y colisión controlada del nombre de raíz temporal.

Cada caso redirige `TEMP` y `TMP` durante su alcance al workspace actual, que el host y el controlador mantienen
en `A:`. Los valores anteriores se restauran al terminar. La prueba de colisión obtiene la secuencia vigente a
partir de una raíz creada por el propio producto, ocupa exactamente el siguiente candidato con un archivo y
comprueba que el algoritmo avanza al intento uno ante `file_exists`. No se asume que el ordinal global inicia
en cero.

La limpieza de directorios creados por la prueba es explícita, iterativa y en orden inverso. No se usa borrado
recursivo ni se toca una ruta fuera del workspace actual.

## Exclusiones

- No se modificó `ArtifactTransaction.cpp` ni su cabecera pública.
- `TB-BAS-054..057` permanecen para la segunda mitad del contrato.
- No se probaron todavía preflight, commit, rollback ni helpers transaccionales.
- No se ejecutó el compilador NativeSharp ni ninguna prueba histórica.

## Evidencia

- Paridad exacta de IDs, contrato, ejecutor, capa y perfil contra la asignación canónica: 5/5.
- Pruebas Python del controlador: 51/51 aprobadas.
- Debug: build correcto y 6/6 gates CTest aprobados.
- Release: build correcto y 6/6 gates CTest aprobados.
- Runtime host: cero imports dinámicos de VCRUNTIME, MSVCP o UCRT en Debug y Release.
- Residuos de staging creados por estos casos: 0.
- Los seis archivos protegidos conservaron exactamente sus hashes registrados en el cursor.
