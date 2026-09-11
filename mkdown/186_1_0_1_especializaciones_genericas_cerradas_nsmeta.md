# 1.0.1 — Especializaciones genéricas cerradas en superficies NSMETA

## Resultado

Una biblioteca precompilada ya puede publicar una API cuyo tipo cerrado usa una
plantilla declarada por otra biblioteca y un argumento perteneciente al propio
productor. El caso que motivó el cambio es equivalente a:

```text
// Biblioteca A
public class Bucket<T> { }

// Biblioteca B
public struct ProducerValue { public int Value; }
public class ProducerBox
{
    public owned Bucket<ProducerValue> Values;
}
```

Antes, la biblioteca B escribía en sus campos y miembros el nombre nominal
generado de `Bucket<ProducerValue>`, pero omitía de NSMETA la especialización que
explicaba cómo reconstruirlo. `--verify-library-artifacts` aceptaba esos
artefactos incompletos y un consumidor sin fuentes fallaba con `unknown type
name`.

Ahora NSMETA conserva esa demanda cerrada, utiliza identidades canónicas para
sus argumentos, materializa el tipo desde la plantilla abierta al importar la
biblioteca y rechaza metadata que referencia un tipo cerrado sin declararlo.

## Causa raíz

`buildMetadataModel` sólo conservaba una especialización cuando la plantilla
abierta pertenecía al proyecto raíz. Esa condición era insuficiente para una
superficie pública como `List<ProducerValue>` o `Bucket<ProducerValue>`: la
plantilla pertenece a una dependencia, pero la especialización concreta forma
parte del contrato del productor.

Además, los argumentos se registraban usando spellings legibles. Para cruzar
módulos sin confundir tipos homónimos se necesitan identidades canónicas con el
módulo propietario.

## Archivos, funciones e implementación

- `include/nativesharp/Frontend/ProjectGenericModel.h`,
  `PrecompiledGenericSpecialization`:
  agrega `hasConcreteDeclaration` para distinguir una especialización ya
  declarada en la fuente pública reconstruida de una demanda externa que debe
  materializarse desde su plantilla.
- `src/Frontend/ProjectGenericModel.cpp`, `ProjectGenericModel::build`,
  `ensureTypeSpecialization` y `ensureMethodSpecialization`:
  no precarga como resuelta una especialización importada sin declaración
  concreta y persiste las identidades canónicas de sus argumentos en lugar de
  sus spellings.
- `src/Interop/LibraryMetadata.cpp`, `buildMetadataModel`:
  recorre bases, interfaces, campos, delegates, miembros y parámetros públicos;
  publica las especializaciones cerradas que esos contratos referencian aunque
  la plantilla sea externa; también conserva transitivamente especializaciones
  cerradas usadas como argumentos de otras especializaciones.
- `src/Interop/LibraryMetadata.cpp`,
  `LibraryMetadataCodec::verifyCurrentCompiler`:
  valida que las declaraciones de especialización estén completas y no sean
  duplicadas, y rechaza referencias a nombres cerrados generados por el
  compilador cuando falta tanto un tipo concreto como la especialización que
  permite materializarlo.
- `src/Driver/Driver.cpp`, `buildPrecompiledPublicSource` y
  `configurePrecompiledModules`:
  reconstruye una firma fuente como `Bucket<ProducerValue>` a partir del
  registro NSMETA, resuelve recursivamente argumentos genéricos anidados y
  comunica al modelo genérico si la metadata ya contiene una declaración
  concreta.
- `tests/unit/Stage48_2LibraryMetadataTests.cpp`,
  `testPrecompiledClosedGenericSurface`:
  crea una biblioteca dueña de la plantilla, un productor que la cierra con un
  tipo propio y un consumidor que sólo recibe `LibraryReference`; también
  elimina deliberadamente el registro de especialización para comprobar que el
  verificador falle.

## Decisiones de compatibilidad

No se cambió el formato binario de NSMETA: se corrigió el significado de campos
ya existentes. El lector conserva compatibilidad con metadata 1.7–1.11 cuyos
argumentos antiguos pueden estar escritos como nombres legibles. Los builtins
siguen aceptándose en esa forma.

No se incorporaron fuentes del productor al consumidor, no se añadió
`ProjectReference` y no se modificó ningún binding. El consumidor conserva el
modelo de distribución `.lib + .nsmeta`.

## Evidencia dirigida

- `nsc_stage48_2_library_metadata_tests`: 185 comprobaciones correctas.
- Reproductor original reconstruido fuera del repositorio de bindings:
  productor O0, lectura y verificación de artefactos, HIR del consumidor,
  emisión del EXE O0 y ejecución con código 0.
- La metadata del productor pasó de 0 a 1 especialización genérica publicada.
- Compilador reconstruido e instalado en `C:\native-sharp`:
  `nsc 0.9.0+build.20260911T003412Z.35e302e6de184ba5`.

Por instrucción expresa, no se ejecutó baseline. Sólo se ejecutaron la prueba
unitaria dirigida y el reproductor exacto del defecto.

## Commit de referencia

### Español

**Título:** Corrige especializaciones genéricas cerradas en NSMETA

**Descripción:** Publica especializaciones externas usadas por contratos
públicos, conserva argumentos con identidad canónica, rematerializa tipos
cerrados desde su plantilla al consumir una biblioteca y hace que el verificador
rechace NSMETA incompleto. Añade una regresión productor/consumidor sin fuentes.

### English

**Title:** Fix closed generic specializations in NSMETA

**Description:** Publish external specializations referenced by public
contracts, preserve canonical argument identities, rematerialize closed types
from their template when consuming a library, and reject incomplete NSMETA.
Add a source-free producer/consumer regression.

## English summary

Precompiled libraries now retain external closed generic specializations that
are part of their public surface. Consumers reconstruct those types from the
open template using canonical cross-module argument identities, while artifact
verification rejects dangling compiler-generated closed type references. The
targeted regression covers production, verification, source-free consumption,
linking, and execution; no baseline was run.
