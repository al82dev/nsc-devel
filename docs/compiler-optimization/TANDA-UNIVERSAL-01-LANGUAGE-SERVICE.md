# Tanda universal 01: Language Service sin edición

## Objetivo

Eliminar la edición del contrato público y de la identidad de caché del Language Service.

## Cambios

- La identidad pública conserva versión del compilador, etapa, target, build y ABI de análisis.
- Se retiraron la versión del lenguaje y las listas de ediciones soportadas.
- Los snapshots ya no publican una edición.
- La caché del último modelo semántico válido se identifica únicamente por el proyecto canónico.
- Las pruebas conservan sus nombres históricos, pero sus proyectos y expectativas son universales.

## Archivos

- `include/nativesharp/LanguageService/LanguageService.h`
- `src/LanguageService/LanguageService.cpp`
- `tests/unit/LanguageServiceIdentityTests.cpp`
- `tests/unit/LanguageServiceSnapshotTests.cpp`

## Verificación

Después del commit se construirá desde cero y se ejecutará la batería completa O0 con NSLink.
