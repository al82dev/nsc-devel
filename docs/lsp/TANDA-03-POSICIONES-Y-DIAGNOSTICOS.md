# Tanda 3: posiciones UTF-16 y diagnósticos estructurados

## Objetivo

Traducir con precisión los offsets de bytes usados por NSC a las posiciones UTF-16 exigidas por
LSP y publicar diagnósticos estructurados con archivo, rango, severidad, código y mensaje.

## Diagnóstico

`SourceManager` conserva offsets UTF-8 y sus columnas históricas cuentan bytes. VS Code interpreta
el campo `character` como unidades UTF-16. Usar directamente la columna del compilador desplazaría
los rangos después de acentos, texto japonés o caracteres representados por pares sustitutos.

## Cambios

- Se añadieron conversiones bidireccionales entre offsets UTF-8 y posiciones LSP UTF-16.
- Se reconocen CRLF, LF, Unicode BMP, caracteres fuera del BMP y UTF-8 inválido sin lecturas fuera
  de límites.
- Se rechazan posiciones que parten una secuencia UTF-8 o un par sustituto.
- Cada `DiagnosticId` tiene ahora un nombre estable legible por herramientas.
- Los diagnósticos internos se convierten a una estructura pública sin exponer `SourceManager` al
  servidor LSP.

## Archivos modificados

- `include/nativesharp/Diagnostics/DiagnosticId.h`.
- `CMakeLists.txt`.

## Archivos nuevos

- `include/nativesharp/LanguageService/ProtocolTypes.h`.
- `src/LanguageService/ProtocolTypes.cpp`.
- `tests/unit/LanguageServiceProtocolTypesTests.cpp`.
- `docs/lsp/TANDA-03-POSICIONES-Y-DIAGNOSTICOS.md`.

## Mejora obtenida

Los subrayados del editor pueden señalar el rango exacto independientemente del alfabeto usado en
el archivo. Los consumidores reciben códigos estables y ya no necesitan interpretar texto humano.

## Verificación

- Construcción exclusiva de `nsc_language_service_protocol_types_tests`.
- Ejecución directa con CRLF, acento, japonés y emoji.
- Construcción exclusiva de `nsc`.
- No se ejecutó baseline ni CTest general.
