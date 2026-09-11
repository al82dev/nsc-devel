# 1.0.1 — Identidad nominal de callables precompilados

## Resultado

La reconstrucción de una API pública desde NSMETA deja de agregar imports de
namespace que pueden cambiar el significado de sus tipos. Un constructor o
método conserva ahora la identidad nominal registrada por el productor aunque
otra `LibraryReference` publique un tipo con el mismo nombre simple.

Además, la producción de NSMETA falla inmediatamente si un método o constructor
concreto, públicamente alcanzable y destinado al consumo no tiene una identidad
física emitida por MIR. El compilador ya no puede producir deliberadamente una
metadata que sólo falle más tarde en el consumidor por faltar su símbolo.

## Defecto observado

Saru NativeSharp publicó correctamente:

```text
InputKeyEventArgs(type:SaruNativeSharp::Keys)
```

y su `.lib` contenía el símbolo físico correspondiente. Sin embargo, al unir la
metadata de Saru con la de GLFW, la fuente pública sintética incluía:

```text
using NativeSharp.Glfw;
```

La declaración reconstruida conservaba sólo la escritura `Keys`. En
NativeSharp, un tipo introducido por `using` precede deliberadamente a un tipo
del namespace global. Por ello el modelo semántico reconstruía la firma como:

```text
InputKeyEventArgs(type:NativeSharpGlfw::NativeSharp.Glfw.Keys)
```

La búsqueda posterior de identidad física actuaba correctamente al no asociar
esa firma alterada con el símbolo de `SaruNativeSharp::Keys`.

## Causa raíz

`buildPrecompiledPublicSource()` ya convertía cada `type:Module::Namespace.Type`
en una escritura con su namespace completo. Aun así, agregaba también todos los
namespaces encontrados como directivas `using`. Esos imports eran redundantes
para parámetros, retornos, campos, propiedades, eventos y delegates, y podían
sombrear nombres globales del propio productor.

Las listas de bases son la única forma que requiere un identificador simple.
Ese caso ya tiene aliases privados `__NscMetadataBaseN`, de modo que no depende
de imports globales.

## Archivos e implementación

- `src/Driver/Driver.cpp`
  - `buildPrecompiledPublicSource()` deja de recolectar y emitir directivas
    `using` sintéticas.
  - Los tipos de contrato continúan saliendo de sus claves canónicas NSMETA con
    namespace completo.
  - Las clases base e interfaces conservan los aliases privados existentes, sin
    cambiar la gramática pública ni la resolución ordinaria del lenguaje.
- `src/Interop/LibraryMetadata.cpp`
  - `buildMetadataModel()` comprueba métodos y constructores concretos de tipos
    públicamente alcanzables.
  - La comprobación cubre accesibilidad `public` y `protected`, omite contratos
    abstractos, helpers del compilador e imports C, y respeta la regla existente
    para `extern C` no exportado.
  - Si MIR no proporciona nombre físico, la emisión se detiene con un diagnóstico
    que identifica el tipo y la firma afectados.
- `tests/unit/Stage48_2LibraryMetadataTests.cpp`
  - Añade un productor con su enum global `Keys` y un segundo enum
    `NativeSharp.Glfw.Keys` proveniente de otra biblioteca precompilada.
  - Comprueba que el constructor publica
    `InputKeyEventArgs(type:NominalProducer::Keys)` con símbolo físico.
  - Añade un consumidor que utiliza simultáneamente ambos enums y exige que el
    constructor conserve el tipo local del productor.

No se modificaron SaruGL, bindings, tutoriales ni repositorios externos.

## Alcance

La corrección no es específica de `Keys` ni de constructores. Elimina la fuente
de rebinding para todos los tipos nominales reconstruidos desde NSMETA:
métodos, constructores, campos, propiedades, indexadores, eventos, delegates,
arreglos y parámetros por referencia. Tampoco altera la precedencia normal de
`using` en código NativeSharp escrito por el usuario.

## Verificación prevista

La regresión queda guardada para la certificación posterior. En este carril se
reconstruye e instala únicamente NSC; no se ejecutan SaruGL, bindings,
tutoriales ni baseline.

## Commit de referencia

### Español

**Título:** Conserva tipos nominales en callables NSMETA

**Descripción:** Elimina imports sintéticos que podían volver a enlazar tipos
globales con homónimos de otras bibliotecas al reconstruir APIs precompiladas.
Rechaza además la emisión de callables públicos concretos sin identidad física
y añade una regresión productor/consumidor con dos enums `Keys` distintos.

### English

**Title:** Preserve nominal types in NSMETA callables

**Description:** Remove synthetic imports that could rebind global types to
same-named types from other libraries while rebuilding precompiled APIs. Also
reject concrete public callables without a physical identity and add a
producer/consumer regression with two distinct `Keys` enums.

## English summary

Precompiled public-source reconstruction now relies on the canonical qualified
type spellings already stored in NSMETA instead of introducing namespace imports
that can change name lookup. Producer metadata emission also refuses consumable
concrete callables that have no MIR physical symbol.
