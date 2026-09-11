# 1.0.1 — Superficie física completa de genéricos cerrados

## Resultado

Los productos de biblioteca materializan todos los cuerpos `public` y
`protected` de los tipos genéricos cerrados que publican. Un consumidor puede
invocar esos miembros aunque el productor nunca los haya usado internamente.

Los ejecutables conservan la política anterior de materialización por demanda:
un método ordinario de una especialización sólo se genera cuando el programa lo
llama o lo convierte a delegate. La ampliación ocurre exclusivamente cuando el
proyecto raíz produce una biblioteca estática o compartida.

La validación MIR → NSMETA también acumula todos los callables públicos sin
identidad física y los muestra juntos. Una compilación ya no obliga a corregir
un miembro para descubrir el siguiente.

## Defecto observado

La biblioteca de Saru especializa y publica:

```text
ReferenceList<IPhysicsObject>
```

La especialización exponía la firma pública:

```text
RemoveAt(builtin:int)
```

pero el productor no llamaba ese método. `ProjectMethodBodyModel` lo trataba
como un cuerpo genérico diferido, OOP HIR y MIR no recibían una función para su
ordinal, y el rechazo temprano añadido en el cambio 198 detenía correctamente
la creación del NSMETA inutilizable:

```text
has no physical identity; refusing to emit unusable NSMETA
```

## Causa raíz

La política de demanda distinguía constructores, destructores, virtuales e
implementaciones de interfaces, pero no distinguía el tipo de producto.

Para un ejecutable, la ausencia de una llamada significa que un método no
necesita código. Para una biblioteca, la llamada puede estar en un consumidor
que todavía no existe. Publicar la firma sin publicar el cuerpo físico rompe el
contrato `.nsmeta + .lib`.

## Cobertura de la corrección

La raíz de demanda se calcula por categoría, no por nombre de método. Cubre:

- métodos de instancia públicos;
- métodos estáticos públicos;
- accessors públicos de propiedades e indexadores;
- operadores y conversiones, que son miembros de método;
- métodos protegidos alcanzables por tipos derivados externos;
- métodos ordinarios de clases y structs genéricos cerrados;
- especializaciones de métodos genéricos presentes en una superficie pública;
- helpers privados o internos llamados transitivamente desde cualquiera de las
  raíces anteriores.

Constructores, destructores, métodos virtuales e implementaciones concretas de
interfaces ya tenían demanda propia y la conservan. Imports C siguen usando su
identidad externa y no se convierten artificialmente en símbolos del productor.

## Archivos e implementación

- `include/nativesharp/Frontend/ProjectMethodBodyModel.h`
  - Amplía `ProjectMethodBodyModel::build()` con el ordinal opcional del proyecto
    cuya superficie pública debe materializarse.
  - Documenta la diferencia entre demanda de ejecutable y demanda de biblioteca.
- `src/Frontend/ProjectCompilation.cpp`
  - Detecta `StaticLibrary` y `SharedLibrary` en el proyecto raíz.
  - Entrega su ordinal al modelo de cuerpos; para `Executable` conserva
    `std::nullopt`.
- `src/Frontend/ProjectMethodBodyModel.cpp`
  - Identifica métodos diferidos de especializaciones cerradas.
  - Exige accesibilidad `public` o `protected` y que toda la cadena de tipos
    contenedores sea pública y pertenezca al productor.
  - Siembra esos métodos antes de ejecutar la clausura existente de llamadas y
    delegates, por lo que sus helpers no públicos también reciben bindings.
- `src/Frontend/ProjectOopHirModel.cpp`
  - Mantiene una sola autoridad: sólo baja cuerpos confirmados por
    `callableBodyAnalyzed()`.
  - Actualiza el contrato para reconocer las raíces externas de bibliotecas.
- `src/Interop/LibraryMetadata.cpp`
  - Acumula todas las firmas públicas concretas que, después de MIR, sigan sin
    nombre físico.
  - Emite un diagnóstico único con la lista completa y rechaza NSMETA.
- `tests/unit/Stage48_2LibraryMetadataTests.cpp`
  - Añade una biblioteca con `ReferenceList<IPhysicsObject>` publicado mediante
    un campo de API.
  - El productor no llama `RemoveAt`, `Count`, `Kind` ni `Shift`.
  - La regresión exige identidad física para método de instancia, accessor,
    método estático y método protegido.
  - `RemoveAt` llama además un helper privado para cubrir la clausura transitiva.
  - Un consumidor precompilado instancia el tipo y usa sus miembros públicos.

No se modificaron SaruGL, bindings, tutoriales ni repositorios externos.

## Auditoría de faltantes

El punto de auditoría queda en `buildMetadataModel()`, después de construir OOP
MIR y antes de serializar NSMETA. Es el último sitio que conoce simultáneamente:

- la accesibilidad semántica de cada miembro;
- la alcanzabilidad pública de su tipo contenedor;
- el ordinal estable del miembro;
- la función MIR y su nombre físico.

Por eso puede informar en una sola compilación cualquier callable que aún rompa
el contrato, sin enlazar consumidores ni ejecutar una matriz externa.

## Verificación prevista

Las regresiones quedan guardadas para certificación posterior. En este carril
se reconstruye e instala únicamente NSC una vez al final; no se ejecutan SaruGL,
bindings, tutoriales ni baseline.

## Commit de referencia

### Español

**Título:** Materializa APIs completas de genéricos cerrados

**Descripción:** Genera todos los métodos públicos y protegidos de
especializaciones cerradas publicadas por bibliotecas, incluyendo accessors,
estáticos y dependencias privadas transitivas. Conserva la demanda por uso en
ejecutables, acumula todas las identidades físicas faltantes y añade una
regresión productor/consumidor basada en `ReferenceList<IPhysicsObject>`.

### English

**Title:** Materialize complete closed-generic APIs

**Description:** Generate every public and protected method of closed
specializations published by libraries, including accessors, statics, and
transitive private dependencies. Preserve use-driven emission for executables,
aggregate all missing physical identities, and add a producer/consumer
regression based on `ReferenceList<IPhysicsObject>`.

## English summary

Library products now treat their exported closed-generic methods as external
call roots. Their complete consumable surface receives semantic bindings, HIR,
MIR, and physical symbols even when producer source never invokes those members.
