# 1.0.1 — Deduplicación de runtimes por contenido

## Resultado

La clausura de runtimes acepta dos copias byte-idénticas de una DLL aunque
provengan de rutas distintas. Un archivo homónimo con contenido diferente
continúa siendo un conflicto y el diagnóstico conserva ambas rutas.

## Causa raíz

El empaquetador identificaba una DLL por nombre, pero resolvía duplicados por
ruta absoluta. Esto convertía una copia transitiva válida —por ejemplo,
`NativeSharp.Foundation.dll` junto al productor— en una DLL supuestamente
distinta de la copia directa instalada en `C:\native-sharp`.

## Archivos e implementación

- `src/Driver/Driver.cpp` reutiliza `filesHaveEqualContents` al registrar
  runtimes directos con el mismo nombre.
- `packageRuntimeClosure` aplica la misma comparación cuando descubre una
  dependencia transitiva junto a otra DLL.
- La ruta sigue siendo la comparación rápida. La lectura binaria sólo ocurre
  cuando los nombres coinciden y las rutas difieren.
- Tamaños distintos fallan antes de comparar bloques; tamaños iguales se
  comparan en bloques de 64 KiB.

No se modificaron bindings ni tutoriales.

## Verificación diferida

De acuerdo con el flujo de integración actual, este cambio se guardó sin una
compilación intermedia. Se comprobará junto con el resto del barrido en una
única reconstrucción final de NSC. No se ejecutó baseline.

## Commit de referencia

### Español

**Título:** Deduplica runtimes byte-idénticos entre rutas

**Descripción:** Compara el contenido cuando dos runtimes homónimos provienen
de rutas diferentes, tanto en referencias directas como en la clausura
transitiva. Acepta copias idénticas y mantiene el rechazo de DLLs realmente
distintas.

### English

**Title:** Deduplicate byte-identical runtimes across paths

**Description:** Compare contents when same-named runtimes come from different
paths, both for direct references and transitive closure. Accept identical
copies while continuing to reject genuinely different DLLs.

## English summary

Runtime identity now combines the PE name with byte equality instead of path
equality. This permits direct and transitive copies of the same DLL without
weakening conflict detection.
