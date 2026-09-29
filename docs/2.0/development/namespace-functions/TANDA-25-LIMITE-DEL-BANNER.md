# Tanda 25: límite seguro del banner

## Objetivo

Garantizar que el banner aparezca al compilar, pero nunca contamine comandos de consulta, diagnóstico o salida técnica.

## Diagnóstico

La primera clasificación reconocía una compilación por la presencia de un argumento terminado en `.ns` o `.nsproj`. Esa regla también coincidía con `restore`, `--check`, `--dump-*` y `--verify-*`, aunque esos comandos no deben recibir texto adicional en su salida.

## Cambios

- `build` y `run` conservan banner.
- Las acciones `--emit-*` conservan banner.
- La compilación directa de un archivo fuente conserva banner.
- `new`, `restore`, `clean` y comandos de paquete no muestran banner.
- `--check*`, `--dump-*`, `--verify-*`, `--print-*`, `--write-*`, `--compare-*` y `--apply-*` no muestran banner.
- JSON, `--quiet`, `--no-banner` y trabajadores internos continúan suprimiéndolo.

## Archivo modificado

- `src/nsc/main.cpp`: clasificación explícita entre compilación y comandos técnicos.

## Archivo nuevo

- `docs/2.0/development/namespace-functions/TANDA-25-LIMITE-DEL-BANNER.md`: documenta el defecto encontrado y su cierre.

## Mejora obtenida

Los usuarios ven la identidad durante una construcción real sin romper volcados, validadores, restauración ni herramientas que consumen stdout.

## Verificación

- Construcción incremental exclusiva del target `nsc`.
- `--check` sobre una fuente válida no imprime banner.
- Una emisión con entrada inexistente sí imprime banner antes del diagnóstico.
- `--no-banner` suprime ese mismo banner.
- No se ejecuta baseline ni CTest general.
