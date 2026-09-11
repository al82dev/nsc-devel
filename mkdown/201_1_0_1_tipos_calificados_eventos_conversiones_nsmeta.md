# 1.0.1 — Tipos calificados en eventos y conversiones NSMETA

## Resultado

La reconstrucción de APIs públicas precompiladas conserva ahora el nombre
calificado completo de los tipos usados por eventos y operadores de conversión.
Esto permite que el consumidor asocie cada referencia con el módulo que la
declara en vez de intentar resolver un nombre corto dentro del namespace de la
biblioteca consumida.

La corrección cubre las 35 inconsistencias reportadas como un solo defecto de
materialización con dos manifestaciones:

- 30 usos de siete especializaciones externas de `EventHandler<T>` en eventos.
- Cinco tipos matemáticos externos usados como destino de conversiones:
  `Mat4`, `Quat`, `Vec2`, `Vec3` y `Vec4`.

## Defecto raíz

NSMETA 1.12 ya conservaba correctamente la identidad canónica y el módulo
propietario de los doce tipos. Sin embargo, `buildPrecompiledPublicSource()`
destruía esa información al convertir la metadata nuevamente en fuente:

1. los eventos respaldados por campo reducían
   `Saru.Framework.EventHandler__...` a `EventHandler__...`;
2. los eventos sin campo materializado aplicaban la misma reducción;
3. los operadores de conversión reducían `NativeSharp.Cglm.Mat4` a `Mat4`, y
   hacían lo mismo con los otros cuatro tipos matemáticos.

El consumidor recibía por tanto nombres sintácticos distintos de los
registrados en `externalTypeKeys`. El proveedor estaba presente y la clausura
nominal era válida, pero la fuente pública reconstruida ya no podía resolverlos.

## Implementación

### `src/Driver/Driver.cpp`

Se modificó `buildPrecompiledPublicSource()` en sus tres rutas afectadas:

- Los eventos reconstruidos desde su backing field emiten directamente el tipo
  producido por `sourceTypeForMetadataKey()`.
- Los eventos reconstruidos desde el miembro NSMETA hacen lo mismo.
- Los operadores `implicit` y `explicit` usan el `returnType` calificado como
  destino de la conversión.

No se agregó sintaxis nueva. El parser de NativeSharp ya acepta eventos y
conversiones con tipos calificados; el defecto era una normalización innecesaria
del generador de fuente precompilada.

### `tests/unit/Stage48_2LibraryMetadataTests.cpp`

Se amplió `testPrecompiledExternalTypeClosure()` para que el contrato de
regresión reproduzca las rutas reales y no sólo firmas de métodos:

- Declara siete eventos, uno por cada especialización cerrada de
  `EventHandler<T>` propiedad del proveedor.
- Declara cinco operadores de conversión cuyo destino es un tipo matemático
  propiedad del proveedor.
- Mantiene la comprobación de que la fachada registra exactamente siete más
  cinco dependencias externas y no republica especializaciones ajenas.
- Obliga al consumidor combinado a reconstruir esas declaraciones desde los
  dos archivos NSMETA.

El caso habría fallado con las tres reducciones de nombre anteriores aunque la
tabla `externalTypeKeys` fuera correcta.

## Alcance y accesibilidad

La corrección se aplica a los miembros que forman parte de la API pública
materializada. Los miembros `private` no se publican como contrato consumible y
no necesitan resolverse desde otro módulo. Los tipos privados que participan
únicamente en implementación permanecen fuera de esta ruta.

No se modificaron reglas de accesibilidad, ownership, ABI, resolución genérica
ni formato NSMETA. Tampoco se modificaron SaruGL, bindings, tutoriales o
repositorios externos.

## Verificación de esta entrega

La entrega requiere únicamente una reconstrucción completa del compilador y su
instalación en `C:\native-sharp`. No se ejecuta baseline ni se ejecutan bindings.
La comprobación funcional de SaruGL queda en manos del consumidor que reportó
las 35 inconsistencias.

## Commit de referencia

### Español

**Título:** Conserva tipos calificados al reconstruir NSMETA

**Descripción:** Evita recortar namespaces en eventos y operadores de
conversión reconstruidos desde NSMETA. Cubre las siete especializaciones de
EventHandler y los cinco tipos matemáticos externos con una regresión que usa
eventos y conversiones reales, sin cambiar el formato ni las reglas del
lenguaje.

### English

**Title:** Preserve qualified types while rebuilding NSMETA

**Description:** Stop stripping namespaces from events and conversion
operators rebuilt from NSMETA. Cover the seven EventHandler specializations and
five external math types with a regression using actual events and conversions,
without changing the format or language rules.

## English summary

The precompiled public-source generator no longer shortens external event types
or conversion targets. Fully qualified names survive NSMETA reconstruction, so
the consumer resolves all twelve reported provider-owned types instead of
looking for them inside the facade module.
