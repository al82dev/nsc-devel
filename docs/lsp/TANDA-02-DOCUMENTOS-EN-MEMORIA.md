# Tanda 2: documentos versionados en memoria

## Objetivo

Permitir que el futuro servidor LSP entregue a NSC el contenido aún no guardado de un archivo y
evitar que una actualización vieja reemplace texto más reciente.

## Diagnóstico

`ProjectCompilation` leía cada fuente directamente del disco dentro de sus workers. Esa ruta es
correcta para la CLI, pero un editor necesita analizar el buffer abierto. Tampoco existía un dueño
de versiones de documento ni una identidad de ruta compatible con Windows.

## Cambios

- `LanguageService` puede abrir, actualizar, consultar y cerrar documentos en memoria.
- Cada actualización exige una versión estrictamente mayor.
- Las claves usan rutas absolutas normalizadas y comparación sin distinción de mayúsculas.
- El almacén está protegido para consultas concurrentes.
- `ProjectCompilation` acepta un proveedor concurrente de buffers; si no hay overlay conserva la
  lectura ordinaria desde disco.

## Archivos modificados

- `include/nativesharp/LanguageService/LanguageService.h`.
- `src/LanguageService/LanguageService.cpp`.
- `include/nativesharp/Frontend/ProjectCompilation.h`.
- `src/Frontend/ProjectCompilation.cpp`.
- `CMakeLists.txt`.

## Archivos nuevos

- `tests/unit/LanguageServiceDocumentTests.cpp`: versiones, rutas y sustitución real de una fuente
  válida del proyecto por un buffer inválido en memoria.
- `docs/lsp/TANDA-02-DOCUMENTOS-EN-MEMORIA.md`: memoria de la tanda.

## Mejora obtenida

El análisis del editor ya no necesitará guardar el archivo ni fabricar copias temporales. El flujo
normal de NSC permanece intacto porque la ausencia de overlay cae explícitamente a la lectura del
archivo real.

## Verificación

- Construcción exclusiva de `nsc_language_service_document_tests`.
- Ejecución directa de esa prueba.
- Construcción exclusiva de `nsc`.
- No se ejecutó baseline ni CTest general.
