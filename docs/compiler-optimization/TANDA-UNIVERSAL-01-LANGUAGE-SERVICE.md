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

Ejecutada después del commit `d69031e5f` mediante una reconstrucción limpia y la batería completa O0 con NSLink.

- Corrida: `universal-o0-t01-20261007`.
- Evidencia: `A:\\nsc-devel\\_nscbuilder\\history\\universal-o0-t01-20261007\\report.md`.
- Construcción del compilador: correcta.
- Construcción de los 429 ejecutables CTest: correcta (`1151/1151` pasos).
- CTest completo: `636/684` aprobados; `48` fallos preexistentes o pertenecientes a tandas todavía pendientes.
- Fixtures O0: `89/90` aprobados; permanece `cc1_c23_views_and_lifetimes` con salida `8` en vez de `42`.
- Bug triage O0: `9/9` aprobado.
- Language Service: `8/8` aprobado, incluidas `nsc.language-service.identity` y `nsc.language-service.snapshots`.
- El fallo anterior de `nsc.language-service.snapshots` quedó eliminado.
- No se modificó `C:\\native-sharp` y no se certificó un baseline.

La tanda queda implementada y verificada en su alcance. La corrida integral no queda limpia todavía porque conserva los contratos de edición, `unsafe`, ownership obligatorio y cierres derivados que corresponden a las siguientes tandas.
