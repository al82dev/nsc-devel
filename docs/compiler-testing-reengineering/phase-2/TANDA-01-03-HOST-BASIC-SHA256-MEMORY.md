# Tanda 01.03 — SHA-256 en memoria de Host.Basic

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Componente | `Host.Basic` |
| Contratos | `CB-BAS-007` y `CB-BAS-009` |
| Casos | `TB-BAS-024` a `TB-BAS-034` |
| Código de producto | Sólo compilar y observar `src/Basic/Sha256.cpp`; no modificar |
| Pruebas históricas | Protegidas; no modificar ni ejecutar |
| Configuraciones | Debug y Release del ejecutable consolidado |
| Acciones prohibidas | NSC, NSLink, baseline, batería histórica, cobertura, despliegue, remoto y `C:` |

## Implementación

`BasicSha256Module.cpp` incorpora once casos canónicos para SHA-256 en memoria y su formato hexadecimal.
Los vectores ejercitan mensaje vacío, `abc`, el vector NIST de 56 bytes, las fronteras de padding 55/56,
las fronteras de bloque 63/64/65, bytes binarios con NUL y bit alto, un millón de `a`, repetición sin estado
residual y formato hexadecimal de ancho fijo en minúsculas.

El módulo compara tanto los 32 bytes del digest como sus 64 caracteres hexadecimales. Los valores esperados
están fijados en el manifiesto y en el ejecutable de prueba; no se derivan en ejecución mediante la función bajo
prueba. Los vectores vacío, `abc`, 56 bytes y un millón de `a` son vectores publicados; los demás se obtuvieron
con `hashlib.sha256` de Python como segunda implementación independiente.

El gate PR existente de `Host.Basic` se amplía únicamente con los diez casos PR de esta tanda. El caso del millón
de `a` conserva perfil `NIGHTLY`, aunque el CTest directo del componente lo ejecuta al validar toda la unidad.

## Exclusiones

- No se modificó `Sha256.cpp` ni su cabecera pública.
- No se implementaron todavía los casos de archivo `TB-BAS-035..040`.
- No se agregó descubrimiento por conteos globales ni por glob.
- No se ejecutó el compilador NativeSharp ni ninguna prueba histórica.

## Evidencia

- Paridad exacta de IDs, contratos, ejecutor, capa y perfil contra la asignación canónica: 11/11.
- Registro consolidado: 33 casos totales en Debug y Release; 11/11 casos de esta tanda presentes en cada uno.
- Pruebas Python del controlador: 51/51 aprobadas.
- Debug: build correcto y 5/5 gates CTest aprobados.
- Release: build correcto y 5/5 gates CTest aprobados.
- Runtime host: cero imports dinámicos de VCRUNTIME, MSVCP o UCRT en Debug y Release.
- Los seis archivos protegidos conservaron exactamente sus hashes registrados en el cursor.
