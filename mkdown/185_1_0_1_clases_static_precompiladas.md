# 1.0.1 — Clases `static` precompiladas sin jerarquía de instancia

## Resultado

La API pública reconstruida desde NSMETA ya no genera declaraciones inválidas
como:

```text
public static class Utility : Object
```

El arreglo tiene dos lados:

- un productor actual no registra base ni interfaces para una clase `static`;
- el consumidor ignora esas cláusulas si aparecen en un NSMETA histórico 1.10.

Los métodos siguen siendo estáticos y la API `Utility.Answer()` no cambia. No se
degrada el contenedor a una clase instanciable como workaround.

## Archivos y funciones

- `src/Interop/LibraryMetadata.cpp`, `buildMetadataModel`:
  omite la jerarquía de instancia de tipos `static`;
- `src/Driver/Driver.cpp`, `buildPrecompiledPublicSource`:
  no emite `:` para clases `static`, incluso ante metadata histórica
  contaminada;
- `tests/unit/Stage46NativeLibrariesTests.cpp`:
  productor nuevo, verificador, fixture 1.10 con base e interfaz artificiales,
  consumidor, enlace y ejecución.

## Evidencia dirigida

Stage 46 pasó 425/425 después del arreglo aislado y 433/433 después de agregar la
clausura entre bibliotecas. La prueba verifica artefactos actuales, materializa
el fixture NSMETA 1.10 y ejecuta el consumidor con resultado 42.

No se modificó SaruGL ni ningún binding. No se instaló nada en
`C:\native-sharp`.

## Commit

- `b66724fa` — corrige jerarquía `static` en NSMETA precompilado.

## English summary

Current producers no longer persist an instance hierarchy for static classes,
and consumers ignore stale base/interface clauses found in legacy NSMETA 1.10.
The regression covers artifact verification, source materialization, linking,
and execution without changing the public static API or editing SaruGL.
