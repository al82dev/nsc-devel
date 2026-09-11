# 1.0.1 — Tipos calificados y diagnóstico completo de NSMETA

## Resultado

El parser reconoce tipos conocidos escritos con su nombre calificado completo
en todas las posiciones declarativas que inmediatamente llaman a `parseType`.
La materialización pública de NSMETA puede conservar nombres como
`Saru.Framework.EventHandler__...` dentro de contratos de interfaz sin perder
sincronización sintáctica.

Cuando la fuente pública reconstruida es inválida, el verificador devuelve
todos los diagnósticos disponibles con archivo sintético, línea, columna,
fragmento de fuente y marcador. Ya no oculta el resto detrás del primer error.

## Causa raíz

`canStartType(current())` sólo reconoce un token aislado. Ante
`Contracts.Types.Payload`, comprobaba `Contracts` contra el catálogo de tipos y
rechazaba la declaración antes de que `parseType()` pudiera consumir la
secuencia calificada. `canStartTypeAt(index_)` ya implementaba la búsqueda de la
identidad completa, pero no se usaba de forma uniforme.

Por separado, `verifyPrecompiledPublicSource()` conservaba únicamente
`diagnostics().front().message()`. Una pérdida de sincronización producía
decenas de cascadas, pero el usuario sólo veía la primera.

## Archivos e implementación

- `src/Parser/Parser.cpp` usa `canStartTypeAt(index_)` en:
  - retornos de delegates;
  - bases de clases;
  - herencia de interfaces;
  - métodos, propiedades e indexadores de interfaces;
  - tipos de eventos;
  - destinos de operadores de conversión;
  - tipos de `catch`;
  - recursos de `using`.
- `src/Driver/Driver.cpp` renderiza el conjunto completo de diagnósticos de la
  fuente NSMETA mediante `DiagnosticRenderer`.
- `verifyLibraryArtifacts` valida primero el archivo COFF y la DLL opcional; un
  fallo sintáctico posterior informa que esas capas físicas ya fueron
  comprobadas.
- `tests/unit/Stage45ExceptionsSyntaxTests.cpp` contiene una sola muestra
  compuesta que recorre las ocho posiciones con tipos calificados.
- `tests/unit/Stage48_2LibraryMetadataTests.cpp` amplía la regresión NSMETA con
  herencia de interfaces y tipos de otro namespace en método, propiedad e
  indexador, manteniendo además la independencia respecto a `kAbstract`.

No se modificaron SaruGL, SaruDX, bindings ni scripts externos.

## Verificación prevista

Las pruebas quedan guardadas para la certificación posterior. En este carril se
compilará e instalará solamente NSC; no se ejecutarán SaruGL, bindings,
tutoriales ni baseline.

## Commit de referencia

### Español

**Título:** Reconoce tipos calificados al reconstruir NSMETA

**Descripción:** Usa detección de tipos calificados en todas las posiciones
declarativas equivalentes y presenta todos los diagnósticos de la fuente
pública generada. Valida los artefactos físicos antes de reportar errores de
materialización.

### English

**Title:** Recognize qualified types while rebuilding NSMETA

**Description:** Use qualified-type detection at every equivalent declaration
site and report all diagnostics from generated public source. Validate physical
artifacts before reporting materialization errors.

## English summary

Qualified metadata type names are now accepted consistently across declaration
grammar, and NSMETA verification exposes complete source diagnostics without
hiding archive or runtime validation.
