# Tanda 22: contrato de idioma e identidad de consola

## Objetivo

Establecer una identidad oficial y localizable para `nsc`, con inglés como idioma predeterminado y selección explícita de español mediante `--language en|es`.

## Diagnóstico

La identidad anterior de `--version` estaba orientada a consumidores técnicos y la ayuda tenía un encabezado distinto. No existía un punto único para seleccionar idioma antes de analizar comandos, por lo que agregar traducciones directamente en cada ruta habría producido comportamiento diferente entre el ejecutable, el `Driver` usado como biblioteca y los comandos de proyecto anidados.

## Cambios

- Se agregó un contexto de idioma limitado a la ejecución actual, seguro para invocaciones anidadas del `Driver`.
- `--language en`, `--language es`, `--language=en` y `--language=es` se consumen antes de despachar cualquier comando.
- Inglés es el valor predeterminado y los idiomas desconocidos, ausentes o duplicados terminan con error de línea de comandos.
- El banner y `--version` comparten una sola función de identidad.
- `nsc.exe` muestra el banner una vez al compilar; `--quiet`, `--no-banner`, salidas JSON y trabajadores internos lo suprimen.
- La ayuda usa el banner oficial y localiza su encabezado como `Usage` o `Uso`.
- La terminación de fases de progreso cambia `finished` por `finalizada` en español sin traducir los identificadores técnicos de fase.
- La identidad muestra versión estable, titular de derechos y el identificador corto de construcción existente; no agrega SHA completo.

## Archivos modificados

- `CMakeLists.txt`: incorpora el módulo nuevo a `nsc_driver`.
- `src/Driver/Driver.cpp`: activa el idioma antes del despacho y reutiliza el banner en ayuda y versión.
- `src/nsc/main.cpp`: integra el banner con silencio, logs, JSON y procesos internos sin contaminar salidas de máquina.
- `tests/unit/DriverTests.cpp`: cubre ayuda española e idioma no compatible.

## Archivos nuevos

- `include/nativesharp/Driver/CompilerConsole.h`: contrato público mínimo de idioma y banner.
- `src/Driver/CompilerConsole.cpp`: parser, ámbito por hilo, catálogo binario y render del banner.
- `docs/2.0/development/namespace-functions/TANDA-22-CONSOLA-IDIOMA-E-IDENTIDAD.md`: memoria de esta tanda.

El módulo separado evita acoplar la selección de idioma con LLVM, el frontend, el ABI o el sistema de proyectos.

## Mejora obtenida

La consola tiene una identidad única y el idioma queda resuelto antes del comando. La selección no altera artefactos, claves de caché, ABI, códigos de salida, nombres de opciones ni formatos técnicos.

## Verificación

- Construcción incremental exclusiva del target `nsc`.
- Ejecución individual de `nsc_driver_tests`.
- Inspección directa de `nsc --version`, `nsc --language es --version` y rechazo de idioma inválido.
- No se ejecuta baseline ni CTest general.
