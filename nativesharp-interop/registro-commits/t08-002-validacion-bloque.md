# T08-002: validación del bloque

Fecha: 2026-11-08.

## Estado de las casillas

Las casillas comprobables de T08-002 están cerradas. Las que siguen abiertas quedan **bloqueadas** por una
decisión pendiente del usuario, y así se anota en el tracking:

- DLL NativeSharp con nombre no ASCII (T08-002-09m7c0g2d).
- `const`/`volatile` de alias C en firmas públicas materializadas desde metadata: requiere NSMeta.
- Firma de callback con `volatile` en la superficie pública materializada: contrato del lenguaje.
- TLS en una DLL sin directorio de importación (limitación del cargador de Windows).
- Dependencias transitivas homónimas de DLL homónimas.

El «traslado a otro equipo» se verifica como traslado de carpeta. En este entorno no hay otro equipo
disponible.

## Batería ejecutada

1. `pruebas-tanda-08-identidad-c/validar.cmd`, el validador del bloque: **aprobado** («T08-002 C identity
   checks passed»).
   - Se ejecutó sobre una copia con fines de línea CRLF de los `.cmd`
     (`claude/scripts/copia-crlf.py` → `A:\nsc-interop-build\claude\crlf`). En el repositorio,
     `externa-dos-bibliotecas/validar.cmd` está versionado con LF, y cmd no encuentra la etiqueta
     `:setup_atomic` («The system cannot find the batch label specified»). Es un defecto de la
     infraestructura de prueba, no del compilador. No se modificó la prueba histórica; el arreglo
     (guardarla con CRLF) se deja a decisión del usuario.
2. Pruebas nuevas de esta sesión, aprobadas:
   - `rutas-unicode/validar-equivalencias.py` y `validar-nombre-salida.py`;
   - `typedef-calificadores/validar-callback-const.py`, `validar-puntero-funcion-calificado.py`,
     `validar-lambda-calificada.py` y `validar-delegate-alias-calificado.py`;
   - `tls-dll-distintas/validar-variadicas-agregados.py`;
   - `global-externa-variantes/validar.py`, `validar-arreglo-incompleto.py` y
     `validar-global-no-importable.py`;
   - `typedef-funcion-record/validar-enum.py`;
   - `plantillas/validar-enum-conflicto-o0.cmd`.
3. `claude/scripts/regresion-callbacks.cmd`: aprobada salvo `pruebas-tanda-08-callback/validar-record-
   precompilado.cmd`, que es el fallo conocido T08-017-02a (alias `RecordCallback` en conflicto) y queda para
   su casilla.

## Regresión encontrada y corregida

`plantillas/validar-enum-conflicto-o0.cmd` fallaba desde `c9511d0ab`: el consumidor que lee su propio
enumerador ya no veía la ambigüedad. Se corrigió en `7735294f3`.
[Detalle](t08-enum-productor-codigo-consumidor.md).
