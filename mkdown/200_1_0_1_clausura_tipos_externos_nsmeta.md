# 1.0.1 — Clausura nominal externa de NSMETA

## Resultado

NSMETA distingue ahora entre una declaración que pertenece al productor y una
identidad nominal que pertenece a otra `LibraryReference`. El consumidor carga
primero el conjunto completo de metadatos, valida esas dependencias contra el
módulo propietario y sólo entonces reconstruye las APIs públicas.

Esta entrega cierra el grupo reportado por SaruGL como una sola categoría:

- 30 usos de siete especializaciones cerradas de `EventHandler<T>` propiedad de
  `SaruNativeSharp`.
- Cinco tipos matemáticos propiedad de `NativeSharpCglm`: `Mat4`, `Quat`,
  `Vec2`, `Vec3` y `Vec4`.

SaruGL no debe copiar ni volver a declarar ninguno de esos tipos. Su metadata
conserva la referencia exacta y el consumidor exige que el módulo propietario
esté presente y realmente publique esa identidad.

## Defecto

El modelo anterior tenía sólo dos estados para una referencia nominal:

1. el tipo aparecía en `metadata.types`; o
2. una entrada de `genericSpecializations` explicaba cómo materializarlo.

Eso era suficiente dentro de un solo productor, pero no describía una API que
usa tipos de otra biblioteca. Las especializaciones precompiladas se podían
republicar como si el nuevo productor las poseyera, mientras que tipos externos
ordinarios dependían implícitamente de que otro módulo casualmente estuviera
visible. El verificador aislado tampoco podía distinguir una dependencia
externa válida de un tipo realmente ausente.

## Regla implementada

NSMETA 1.12 agrega `externalTypeKeys`. Cada entrada es una clave canónica
completa `type:Modulo::Nombre` que cumple estas propiedades:

- el módulo actual la utiliza en su contrato;
- el módulo actual no la declara ni materializa;
- la identidad conserva arreglos y punteros fuera de la clave base;
- el propietario nominal es un módulo distinto;
- el conjunto queda ordenado y sin duplicados para mantener emisión
  determinista.

Una especialización genérica importada cuyo `specializedSyntax` es nulo ya está
materializada por su proveedor. El nuevo productor la registra como dependencia
externa y no vuelve a publicar su `genericSpecialization`. Una especialización
creada durante la compilación actual conserva su entrada local, incluso si la
plantilla abierta procede de otra biblioteca; ése es el caso necesario de
`List<TipoDelProductor>` y permanece intacto.

## Validación del consumidor

`configurePrecompiledModules()` realiza ahora dos pasadas:

1. lee y verifica todos los archivos NSMETA del conjunto de
   `LibraryReference`;
2. valida cada `externalTypeKey` contra el módulo indicado por su propia clave.

El proveedor satisface la dependencia cuando declara exactamente el tipo en su
tabla nominal o publica la especialización cerrada correspondiente. Si falta el
módulo o la identidad, el compilador rechaza el conjunto antes de generar
fuente sintética. El diagnóstico acumula todas las faltas, por lo que no obliga
a corregir una referencia y recompilar para descubrir la siguiente.

## Archivos y funciones

### `include/nativesharp/Interop/LibraryMetadata.h`

- Incrementa el formato actual de NSMETA de 1.11 a 1.12.
- Agrega `LibraryMetadata::externalTypeKeys`.
- Documenta la nueva garantía de clausura nominal entre bibliotecas.

### `src/Interop/LibraryMetadata.cpp`

- `metadataTypeModule()` extrae de forma canónica el propietario de una clave
  nominal.
- `buildMetadataModel()` separa especializaciones importadas de
  especializaciones materializadas por el productor.
- La emisión recolecta referencias externas desde tipos, miembros, argumentos
  genéricos, contratos de dispatch y exports C.
- `encodePayload()` y `decodePayload()` conservan la nueva tabla únicamente a
  partir de NSMETA 1.12, manteniendo lectura de 1.0–1.11.
- `verifyCurrentCompiler()` comprueba forma, propiedad, duplicados y coherencia
  entre tipos locales, especializaciones locales y dependencias externas.

### `src/Driver/Driver.cpp`

- `LoadedPrecompiledMetadata` conserva el NSMETA junto con su proyecto y ruta.
- `metadataTypeModule()` y `metadataProvidesType()` resuelven la propiedad
  nominal sin reducirla a un nombre fuente.
- `validatePrecompiledExternalTypeClosure()` valida todo el conjunto y agrega
  faltas por módulo y tipo.
- `configurePrecompiledModules()` carga primero todas las bibliotecas y sólo
  materializa sus fuentes después de aprobar la clausura.

### `tests/unit/Stage48_2LibraryMetadataTests.cpp`

- Agrega un proveedor con los cinco tipos matemáticos reportados y siete
  especializaciones de un delegate genérico `EventHandler<T>`.
- Agrega una fachada que los expone sin apropiarse de sus declaraciones.
- Comprueba que los doce tipos quedan como dependencias externas, que la fachada
  no republica las siete especializaciones y que el conjunto proveedor/fachada
  llega a HIR.
- Agrega el caso rojo sin proveedor y exige un único diagnóstico acumulativo.

## Compatibilidad

El lector continúa aceptando NSMETA 1.0–1.11. Esos formatos no prometían una
tabla explícita de dependencias nominales y conservan su comportamiento
histórico. Los productores nuevos escriben 1.12 y reciben la validación fuerte.

No se modificaron SaruGL, bindings, tutoriales ni repositorios externos. No se
ejecutó baseline ni se compiló el producto durante esta entrega; la compilación
e instalación se reservan para el cierre del barrido solicitado.

## Commit de referencia

### Español

**Título:** Declara dependencias nominales externas en NSMETA

**Descripción:** Agrega a NSMETA 1.12 una tabla determinista de tipos propiedad
de otras LibraryReference, evita republicar especializaciones precompiladas y
valida la clausura completa contra el conjunto de módulos antes de reconstruir
APIs. Cubre los siete EventHandler cerrados y Mat4, Quat, Vec2, Vec3 y Vec4 con
un caso positivo y un diagnóstico acumulativo cuando falta el proveedor.

### English

**Title:** Declare external nominal dependencies in NSMETA

**Description:** Add a deterministic table of types owned by other
LibraryReferences to NSMETA 1.12, avoid republishing precompiled
specializations, and validate the complete module set before rebuilding APIs.
Cover the seven closed EventHandler types plus Mat4, Quat, Vec2, Vec3, and Vec4
with a positive case and an accumulated missing-provider diagnostic.

## English summary

NSMETA 1.12 explicitly records nominal types owned by other precompiled
modules. Consumers validate the full `LibraryReference` type closure before
source reconstruction, while already materialized generic specializations stay
owned by their original provider.
