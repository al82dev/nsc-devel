# NativeSharp 3.0 — Tanda operativa 05: calls, typed params e initializers

Fecha de cierre: 29 de septiembre de 2026

Rama: `development`

Plataforma normativa: Windows x64

Configuración de construcción: Debug/O0

Estado al cerrar el commit: `TARGETED-GREEN`; pendiente del baseline integral que ejecutará Alberto

## 1. Objetivo

Esta tanda conecta tres partes de T01 con el compilador productivo:

- `NS3-T01-F01`: conservar el mapeo de argumentos nombrados sin alterar el orden textual de
  evaluación;
- `NS3-T01-F02`: hacer ejecutable `params ReadOnlySpan<T>` en métodos, funciones libres y
  constructores, sin arreglo administrado ni heap oculto;
- `NS3-T01-F05`: resolver inicializadores nombrados y spreads de agregados con evaluación única,
  inicialización completa y construcción directa en el almacenamiento final del local.

La sintaxis ya existía. El trabajo de esta tanda está en binding, overload resolution, HIR, MIR,
LLVM, metadata y pruebas de extremo a extremo.

## 2. Resultado funcional

### 2.1 Argumentos nombrados

La ruta existente sigue siendo la autoridad. La fixture ejecutable comprueba que:

- cada argumento se evalúa en el orden en que aparece en la fuente;
- después de evaluarse, los valores se ordenan por ordinal de parámetro para la ABI;
- parámetros predeterminados y argumentos nombrados comparten el mismo mapa;
- el nombre público del parámetro continúa siendo parte de NSMETA y del source materializado.

Ejemplo comprobado: `Encode(third: Mark(3), first: Mark(1), second: Mark(2))` registra efectos en
orden `3,1,2`, pero el callee recibe `first=1`, `second=2`, `third=3`.

### 2.2 `params ReadOnlySpan<T>`

El símbolo de parámetro conserva dos datos distintos:

- el tipo físico completo del parámetro, `ReadOnlySpan<T>`;
- la clave canónica de `T`, usada para convertir cada elemento expandido.

El overload resolver distingue tres formas:

1. pack vacío: sintetiza `{ data=null, length=0 }`;
2. uno o más argumentos elemento: los convierte a `T` y forma un span temporal;
3. un `ReadOnlySpan<T>` ya construido: lo pasa directamente, sin volver a empaquetarlo.

La preferencia de overload elige una firma fija antes que una expansión `params` cuando ambas son
viables. Un elemento ownership-qualified, fresh owner o `move(...)` no puede entrar en el span
temporal: éste no es dueño de sus elementos y no puede prolongar su lifetime.

La bajada publica nodos `ParameterPack` en HIR y MIR. LLVM reserva en el entry block:

- un `[N x T]` para los elementos, sólo si `N > 0`;
- el carrier `ReadOnlySpan<T>`;
- stores izquierda→derecha;
- puntero nulo y longitud cero para el pack vacío.

No se llama al allocator. El alloca se coloca en el entry block para que un call dentro de un loop
no haga crecer el stack en cada iteración. MIR valida además que el carrier comience con `T*` e
`int`, evitando escribir un supuesto span sobre un layout incompatible.

La misma semántica se aplica a:

- llamadas de instancia, estáticas y virtuales;
- funciones libres de namespace;
- construcción de clases y structs;
- inicializadores `this(...)` y `base(...)`;
- construcción por valor y placement.

### 2.3 NSMETA 1.24

NSMETA sube de 1.23 a 1.24. Cada parámetro de miembro o función libre conserva
`isParameterPack`. El consumidor `LibraryReference` reconstruye la palabra `params` y vuelve a
ejecutar la misma resolución de calls.

El read-back del productor usa ahora la edición almacenada en metadata al parsear tanto la API
pública como las plantillas públicas. Esto corrige un defecto encontrado por la prueba dirigida:
la API generada sí contenía `params`, pero el verificador la analizaba como edición 2 y se
rechazaba a sí mismo con `NS3002`.

Un intento de escribir una biblioteca con packs como NSMETA 1.23 o anterior falla de forma
explícita; el escritor no elimina silenciosamente la semántica.

### 2.4 Inicializadores generalizados

El binder construye un mapa estable de entrada de fuente a slot de layout. Soporta:

- miembros nombrados en cualquier orden;
- mezcla de entradas posicionales y nombradas sin que una posicional robe un slot reservado por
  un nombre posterior;
- un único spread `..source` del mismo tipo exacto;
- structs ordinarios, structs genéricos cerrados y fixed arrays;
- inicializadores anidados;
- diagnóstico de nombre desconocido, duplicado, exceso o miembro omitido.

Una forma generalizada debe cubrir todos los campos o aportar un spread. La forma posicional
histórica de C conserva su compatibilidad: los elementos no mencionados permanecen en cero.

El spread se evalúa exactamente una vez en su posición textual y sólo rellena slots todavía no
asignados. HIR publica `AggregateSpread`; MIR publica `AggregateSpreadStore` con offsets de origen
y destino. Los bitfields deben escribirse por nombre y no se copian de manera ambigua mediante
spread.

Cuando el initializer pertenece a una declaración local, LLVM escribe directamente en el slot
final. Ya no crea `aggregate.init`, carga el carrier y lo vuelve a almacenar. Los inicializadores
que son argumentos, retornos o subexpresiones todavía necesitan un temporal porque no poseen un
place final suministrado por el caller.

### 2.5 Seguridad ante fallo parcial

El modelo actual todavía no tiene drop flags por campo para value types; eso pertenece a T06.
Aceptar hoy un inicializador generalizado de un struct con destructor o campo `owned` permitiría
filtrar lo ya construido si una expresión posterior falla. La regla productiva es deliberadamente
segura:

- agregados trivialmente destructibles: aceptados y construidos in-place;
- agregados con destructor o campos `owned`: rechazados con diagnóstico causal y recomendación de
  constructor.

La ampliación a cleanup parcial real se reabre junto con `NS3-T06-F02/F05/F08`. Los protocolos de
colección estática se integrarán después de que T05 defina traits/interfaces estáticas; esta tanda
no inventa un protocolo ad hoc antes de su autoridad semántica.

## 3. Versiones de contrato

| Contrato | Antes | Después | Motivo |
|---|---:|---:|---|
| Project OOP MIR | 1.14 | 1.15 | `ParameterPack` y `AggregateSpreadStore` explícitos |
| NSMETA | 1.23 | 1.24 | persistencia de `params` en miembros y funciones libres |
| Native ABI | sin cambio | sin cambio | el callee continúa recibiendo un `ReadOnlySpan<T>` físico |

## 4. Archivos modificados

### Símbolos, binding y overload resolution

- `include/nativesharp/Frontend/ProjectConstructionModel.h`
- `include/nativesharp/Frontend/ProjectGenericModel.h`
- `include/nativesharp/Frontend/ProjectMemberModel.h`
- `include/nativesharp/Frontend/ProjectMethodBodyModel.h`
- `include/nativesharp/Frontend/ProjectNamespaceFunctionModel.h`
- `src/Frontend/ProjectCompilation.cpp`
- `src/Frontend/ProjectConstructionModel.cpp`
- `src/Frontend/ProjectGenericModel.cpp`
- `src/Frontend/ProjectMemberModel.cpp`
- `src/Frontend/ProjectMethodBodyModel.cpp`
- `src/Frontend/ProjectNamespaceFunctionModel.cpp`

### HIR, MIR y observabilidad

- `include/nativesharp/Frontend/ProjectOopHirModel.h`
- `include/nativesharp/Frontend/ProjectOopMirModel.h`
- `src/Frontend/ProjectOopHirModel.cpp`
- `src/Frontend/ProjectOopHirModelDumper.cpp`
- `src/Frontend/ProjectOopMirModel.cpp`
- `src/Frontend/ProjectOopMirModelDumper.cpp`

Los dumpers imprimen tipo/element count/storage de packs, número de targets del spread, evaluación
única y offsets de stores. La expansión deja de ser una decisión invisible del backend.

### Backend y metadata

- `src/LLVM/ProjectOopLlvmLowerer.cpp`
- `include/nativesharp/Interop/LibraryMetadata.h`
- `src/Interop/LibraryMetadata.cpp`
- `src/Driver/Driver.cpp`

### Pruebas

- `tests/unit/Cc1CorrectnessTests.cpp`
- `tests/unit/Stage48_2LibraryMetadataTests.cpp`

## 5. Archivos nuevos y razón

- `tests/cc1/c55_ns3_calls_initializers/c55_ns3_calls_initializers.nsproj`
  - fixture ejecutable edición 3, independiente de NativeSharp.Core, para probar la semántica sin
    arrastrar la compilación de las librerías.
- `tests/cc1/c55_ns3_calls_initializers/Main.ns`
  - cubre calls nombrados, packs 0/1/N, span directo, overload fijo, función libre, constructor de
    struct, orden de efectos, named/spread, struct genérico y fixed array.
- `docs/3.0/TANDA-05-CALLS-PARAMS-INITIALIZERS-T01.md`
  - expediente de diseño, archivos, límites, mejora y evidencia de este commit.

No se creó ningún worktree, directorio Codex, archivo fuente en C: ni cambio remoto.

## 6. Mejora obtenida

Antes de esta tanda, `params` terminaba en la firma pero una call ordinaria intentaba convertir el
primer argumento al span completo; los miembros nombrados/spreads de initializer existían sólo en
la superficie y el backend seguía tratando las entradas por posición. Una `LibraryReference`
perdía la marca `params`.

Después:

- las calls usan el mapa real de parámetros y preservan el orden de efectos;
- packs vacíos o expandidos funcionan en todos los callables productivos;
- el costo es visible: stack + stores, sin heap implícito;
- overload resolution distingue firma fija, pack expandido y span directo;
- NSMETA 1.24 conserva el contrato cross-module;
- named/spread inicializers llegan hasta LLVM con evaluación única;
- los locales se construyen directamente en su almacenamiento final;
- un lifetime no soportado falla temprano en vez de filtrar ownership.

## 7. Construcción y pruebas dirigidas

### Construcción

- `nsc` Debug/O0: correcta.
- `nsc_ns3_t01_expression_tests`: compilado.
- `nsc_stage48_2_library_metadata_tests`: compilado; no se ejecutó porque el ejecutable completo
  contiene escenarios O2 ajenos a esta tanda.
- `nsc_cc1_correctness_tests`: compilado; no se ejecutó completo porque su fixture runner incluye
  O2. La fixture c55 se ejecutó directamente sólo con O0.

### Resultados ejecutados

| Comprobación dirigida | Resultado |
|---|---:|
| helper `nsc_ns3_t01_expression_tests` | 27/27 |
| HIR c55 `parameter-pack` | 5 nodos |
| MIR c55 `parameter-pack` | 5 nodos |
| MIR c55 `aggregate-spread-store` | 2 nodos |
| calls al allocator en LLVM c55 | 0 |
| temporales `aggregate.init` para locales c55 | 0 |
| ejecutable c55 O0 | exit 42 |
| productor NSMETA | 1.24 válido |
| consumidor `LibraryReference` O0 | exit 42 |
| initializer no trivial | rechazado con el diagnóstico esperado |

No se ejecutó CTest, baseline integral, O1, O2 ni O3. La certificación acumulada queda reservada al
baseline final de Alberto.

## 8. Pendientes explícitos, no ocultos

- drop flags y cleanup parcial para aggregates no triviales: T06;
- spreads move-only/copy-by-trait: T06/T05;
- collection initializer por protocolo estático: T05 después de traits;
- explicación de overload/pack en LSP y visualización específica en debugger: tanda de tooling;
- baseline integral y promoción: Tanda 32, bajo control de Alberto.
