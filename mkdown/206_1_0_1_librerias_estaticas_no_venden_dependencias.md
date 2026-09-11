# 1.0.1 — Las bibliotecas estáticas conservan externa su clausura de dependencias

## Resumen

Una biblioteca estática producida por NSC incluía dentro de su propio archivo
COFF cada `.lib` indicada mediante `LibraryReference`. Al consumir después esa
biblioteca, la clausura registrada en NSMETA volvía a proporcionar las mismas
dependencias al enlace final. NSLink recibía dos copias de los mismos miembros y
diagnosticaba `NSL1021`.

El símbolo que expuso el defecto fue:

```text
NativeSharp.Core.Object::Object()
```

## Evidencia del artefacto real

La inspección de `Saru.NativeSharp.lib`, regenerada con el compilador instalado,
mostró que el archivo no contenía una definición local del constructor. Contenía
miembros de import library para `NativeSharp.Foundation.dll`, incluidos:

```text
__imp___nsc_ctor_NativeSharpFoundation$3A$3ANativeSharp$2ECore$2EObject__Object
__nsc_ctor_NativeSharpFoundation$3A$3ANativeSharp$2ECore$2EObject__Object
```

El enlace recibía además la import library de Foundation directamente, porque
NSMETA conserva correctamente esa dependencia. El constructor era, por tanto,
el primer duplicado visible de una dependencia vendida dos veces; no eran dos
cuerpos de constructor generados por el frontend.

## Causa

`WindowsLibraryBuilder::buildStatic()` construía `archiveInputs` con:

- `program.obj` del productor;
- `runtime.obj` del productor;
- todas las bibliotecas de `extraLinkLibraries_`.

`extraLinkLibraries_` contiene las `LibraryReference` validadas por el driver.
Copiarlas dentro del producto contradecía el contrato de NSMETA: una dependencia
seguía declarada como externa y, simultáneamente, quedaba físicamente aplanada en
el archivo del productor.

LLD podía ocultar parcialmente el problema por su selección perezosa de miembros
de archivo. NSLink, al resolver la composición explícitamente, detectaba las dos
definiciones y emitía el diagnóstico correcto.

## Corrección

### `src/Linker/WindowsLibraryBuilder.cpp`

`buildStatic()` archiva ahora exclusivamente los objetos que pertenecen al
producto que está construyendo:

- `program.obj`;
- `runtime.obj`.

Las `LibraryReference` no se copian dentro de la nueva `.lib`. Su identidad,
metadata y runtime permanecen en la clausura NSMETA, y el enlace final de la DLL
o ejecutable las aporta una sola vez.

La ruta de DLL no cambia: `buildDynamic()` continúa enlazando sus dependencias
porque una imagen PE final sí debe resolverlas. Tampoco se modificó NSLink.

## Prueba de regresión

### `tests/unit/Stage46NativeLibrariesTests.cpp`

Se añadió `testStaticLibraryKeepsReferencesExternal()`:

1. produce una biblioteca estática de dependencia;
2. produce otra biblioteca estática que la consume mediante `LibraryReference`;
3. valida el archivo COFF resultante;
4. confirma que el símbolo propio del productor está presente;
5. confirma que el símbolo físico de la dependencia no fue copiado al productor.

La prueba observa el contenido del archivo, no sólo el éxito de LLD, para evitar
el falso verde que daba la extracción perezosa de archives.

## Relación con la corrección anterior

El commit `75a4d9e8` conserva como ODR los constructores implícitos realmente
sintetizados en más de un módulo. Esa regla es válida, pero no explicaba por sí
sola el `Object()` de Saru: el artefacto real confirmó que la segunda identidad
provenía de una import library aplanada. Esta entrega corrige la propiedad del
archive y elimina la duplicación en su origen.

## Archivos modificados

- `src/Linker/WindowsLibraryBuilder.cpp`
  - deja de incorporar `extraLinkLibraries_` al archivo estático producido;
  - documenta la separación entre propiedad del producto y clausura NSMETA.
- `tests/unit/Stage46NativeLibrariesTests.cpp`
  - agrega `testStaticLibraryKeepsReferencesExternal()`.
- `mkdown/206_1_0_1_librerias_estaticas_no_venden_dependencias.md`
  - registra evidencia, causa, implementación y alcance.

## Referencia de commit

### Título / Title

```text
Mantiene externas las dependencias de bibliotecas estáticas / Keep static-library dependencies external
```

### Descripción / Description

```text
ES: Evita aplanar las LibraryReference dentro de una biblioteca estática, dejando que NSMETA entregue cada dependencia una sola vez al enlace final. Elimina duplicados NSL1021 de import libraries como NativeSharp.Foundation y agrega una prueba que inspecciona los símbolos del archive producido.

EN: Stop flattening LibraryReference archives into a static library, allowing NSMETA to supply each dependency exactly once at the final link. Eliminate NSL1021 duplicates from import libraries such as NativeSharp.Foundation and add a regression test that inspects the produced archive symbols.
```

## English summary

NSC flattened every `LibraryReference` archive into a newly produced static
library while NSMETA continued to publish those references as external
dependencies. Final consumers therefore linked two copies of the same import or
NativeSharp symbols. Static-library creation now archives only the producer's own
program and runtime objects. Dependencies remain external and are supplied once
by the final EXE/DLL link. A regression test verifies archive ownership directly.
