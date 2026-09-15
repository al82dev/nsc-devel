# Ítem 337 — Diagnósticos de SDK y librerías faltantes / SDK and missing-library diagnostics

## Resultado / Result

El manifiesto puede declarar dependencias `Library` mediante un nombre lógico y una ruta `.lib`
relativa y segura. Al cargar un proyecto, NativeSharp comprueba físicamente cada dependencia antes
de iniciar el frontend. / A manifest can declare `Library` dependencies through a logical name and
a safe relative `.lib` path. While loading a project, NativeSharp physically validates every
dependency before starting the frontend.

## Diagnósticos estables / Stable diagnostics

- `NSC-SDK-001`: el manifiesto solicitado no existe. / The requested manifest is missing.
- `NSC-SDK-003`: no existe una variante para arquitectura y versión. / No variant matches the
  architecture and version.
- `NSC-SDK-004`: falta una biblioteca declarada por el SDK. / An SDK-declared library is missing.

Los mensajes incluyen la identidad lógica y la ruta física relevante; no dependen del texto del
enlazador. / Messages include the relevant logical identity and physical path; they do not depend
on linker wording.

## Pruebas / Tests

La prueba dirigida cubre SDK ausente, biblioteca ausente y el mismo proyecto después de materializar
la biblioteca requerida. / The focused test covers a missing SDK, a missing library, and the same
project after the required library is materialized.
