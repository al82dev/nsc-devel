# 132 — 1.0-E1: objetos precompilados completos

## Resultado de la entrega

E1 quedó implementado. Una biblioteca NativeSharp ya no publica una clase como una referencia
opaca que sólo puede recibirse desde una fábrica. El consumidor puede construirla con `new`, usar
su jerarquía, llamar por clase base, despachar por interfaz y dejar que `raii` la destruya sin
volver a compilar la implementación del productor.

La matriz lo demuestra con biblioteca estática y DLL, en O0 y O2:

```csharp
raii Precompiled.Counter value = new Precompiled.Counter(40);
raii Precompiled.BonusCounter polymorphic = new Precompiled.BonusCounter(40);
borrowed Precompiled.CounterBase baseView = polymorphic;
borrowed Precompiled.IScore interfaceView = polymorphic;

int direct = value.Add(2);
int virtualResult = baseView.Score();
int interfaceResult = interfaceView.Score();
```

También hay una prueba de destructor observable: `Tracked` incrementa un contador del productor y
el consumidor comprueba que el incremento ocurre exactamente una vez al salir del scope.

## De dónde partíamos

NativeSharp 0.9 ya conservaba en `.nsmeta` la base, interfaces, miembros, descriptor, vtable,
entrada de destrucción, firmas virtuales y tablas de interfaz. Sin embargo,
`buildPrecompiledPublicSource` omitía la jerarquía y los constructores, y
`canMaterializeNativeMethod` rechazaba virtuales y overrides. Era una frontera deliberadamente
segura: el compilador prefería no ofrecer una operación antes que convertir por accidente un
dispatch dinámico en una llamada directa.

El trabajo de E1 no consistió en inventar otro modelo de objetos. Consistió en conectar la
metadata publicada con los modelos semánticos, HIR, MIR y LLVM que ya usa una compilación normal,
marcando con precisión qué declaraciones vienen de otra biblioteca y qué código pertenece al
productor.

## Arquitectura que quedó

El flujo final es éste:

```text
productor
  clases + jerarquía + constructores + dispatch + destrucción
       │
       ├── .lib o DLL: cuerpos, constructores, slots y destructor reales
       └── .nsmeta 1.7: contrato nominal y símbolos publicados
                              │
                              ▼
consumidor
  declaración sintética precompilada
       │
       ├── name lookup / tipos / herencia / interfaces / overloads
       ├── HIR externo: firma y símbolo, nunca cuerpo del productor
       ├── MIR importado: tabla semántica, nunca globals duplicados
       └── LLVM: new + descriptor del productor + ctor externo
                    dispatch por slots + destrucción externa
```

La asignación del bloque de memoria sigue ocurriendo en el ejecutable consumidor usando el tamaño
exacto publicado. Lo que no se replica es la identidad dinámica: el descriptor se obtiene del
productor, el constructor ejecutado es el del productor y la liberación semántica cruza por su
entrada de destrucción.

## El problema inesperado de PE y su solución

El primer prototipo compiló y enlazó, pero la construcción desde DLL se colgó en ejecución. La
causa no estaba en la jerarquía: en PE/COFF una función y un dato importado no tienen la misma
indirección. El consumidor estaba tratando la celda IAT de un dato como si fuera el descriptor de
clase real.

La corrección fue publicar, para cada clase pública, una función de acceso:

```text
__nsc_get_classdesc_<identidad>() -> puntero al descriptor real
```

Por eso `LibraryMetadataDispatch` adquirió `descriptorAccessorName` y el formato avanzó primero a
NSMETA 1.6. El productor define y exporta la función; el consumidor la declara y la llama antes de
inicializar el encabezado del objeto. Una biblioteca 1.5 todavía se puede leer, pero no ofrece
`new` precompilado porque carece de una forma segura de obtener el descriptor.

## Dos fronteras semánticas corregidas

### Constructores importados

La declaración sintética necesita un cuerpo vacío para que el parser la acepte, pero ese cuerpo no
es una implementación. Al principio el frontend intentó revalidar la cadena `base(...)` de ese
cuerpo artificial y diagnosticó un error inexistente. `ProjectConstructionModel` ahora omite esa
revalidación únicamente para constructores marcados como precompilados; la cadena real ya fue
certificada al compilar el productor.

### Destructores importados

La prueba de destrucción exacta encontró un segundo límite: conservar el destructor como si tuviera
cuerpo local hacía que el consumidor intentara publicarlo o reconstruir el plan privado. Ahora se
conserva la declaración para la semántica del tipo, pero no se crea HIR de implementación ni se
regenera el plan de campos/destructores. `ProjectOopMirModel` deja la operación detrás de
`destructionEntryName`, que pertenece al productor.

Esta separación es importante: una declaración importada debe existir para resolver el lenguaje,
pero existir semánticamente no implica poseer su implementación.

## Cambios por archivo y responsabilidad

### Contratos de frontend

- `include/nativesharp/Frontend/ProjectMemberModel.h`
  - `ClassMemberSymbol::isPrecompiled` conserva el origen por miembro. Es por miembro, no sólo por
    tipo, porque un tipo parcial puede combinar contrato importado y plantillas materializables.
- `src/Frontend/ProjectMemberModel.cpp`
  - Propaga la marca desde cada parte sintáctica a métodos, constructores, destructores y enums.
- `include/nativesharp/Frontend/ProjectOopMirModel.h`
  - `ProjectOopMirDispatchTable::isImported` separa tabla semántica de tabla emitida.
  - `descriptorAccessorName` identifica el acceso seguro al descriptor del productor.

### Reconstrucción del contrato publicado

- `src/Driver/Driver.cpp`
  - `canMaterializeConstructor` admite sólo constructores públicos de clases con descriptor
    accesible y parámetros reconstruibles.
  - `canMaterializePrecompiledDeclaration` decide métodos concretos, virtuales, overrides,
    abstractos, constructores y destructor sin abrir miembros que no forman parte del contrato.
  - `buildPrecompiledPublicSource` reconstruye interfaces, base, interfaces implementadas,
    modificadores de clase y dispatch, campos necesarios y miembros invocables.
  - `publishedNativeSymbols` incluye constructores y accessors de descriptor.
  - `runProjectOopDynamicLibraryGeneration` exporta constructores públicos, destinos virtuales y
    accessors requeridos por consumidores NativeSharp.

### Semántica, HIR y MIR

- `src/Frontend/ProjectVirtualDispatchModel.cpp`
  - Las clases importadas ya participan en el mismo cálculo semántico de slots.
- `src/Frontend/ProjectConstructionModel.cpp`
  - Evita reinterpretar cuerpos sintéticos como cadenas reales de constructores.
- `src/Frontend/ProjectMethodBodyModel.cpp`
  - Decide el carácter precompilado desde el miembro exacto.
- `src/Frontend/ProjectOopHirModel.cpp`
  - Crea declaraciones HIR externas para métodos y constructores importados.
  - No crea cuerpo HIR para el destructor importado.
- `src/Frontend/ProjectOopMirModel.cpp`
  - Construye el mapa semántico importado y sus destinos de dispatch.
  - No recopila cuerpos de destructor ni pasos de destrucción privados para tablas importadas.

### Metadata y LLVM

- `include/nativesharp/Interop/LibraryMetadata.h`
  - Introduce el accessor de descriptor en el contrato versionado.
- `src/Interop/LibraryMetadata.cpp`
  - Publica, serializa, lee y valida ese símbolo desde NSMETA 1.6.
- `src/LLVM/ProjectOopLlvmLowerer.cpp`
  - Declara el accessor para tablas importadas y no emite sus globals locales.
  - Define el accessor para tablas del productor.
  - Durante `new`, llama el accessor y escribe el descriptor real en el encabezado.
  - Omite definiciones locales de entradas de destrucción importadas.

## Pruebas añadidas o ampliadas

- `tests/unit/Stage46NativeLibrariesTests.cpp`
  - `new` directo de clase precompilada;
  - constructor base y constructor derivado;
  - override por referencia base;
  - implementación y dispatch de interfaz;
  - destrucción observable exactamente una vez;
  - biblioteca estática y DLL;
  - O0 y O2;
  - exportación del accessor de descriptor.
- `tests/unit/Stage48_2LibraryMetadataTests.cpp`
  - round-trip del accessor junto con slots virtuales.
- `tests/unit/Stage50_9MetadataHostCompatibilityTests.cpp`
  - un archivo NSMETA 1.5 omite el campo 1.6 y el lector conserva alineados vtable y slots.

## Resultado verificable

- Compilación C++ de `nsc` y suites afectadas: correcta.
- `nsc.stage46.native-libraries`: **PASS, 126 checks**.
- `nsc.stage48_2.library-metadata`: **PASS**.
- `nsc.stage50_9.metadata-host-compatibility`: **PASS**.
- Ejecución estática y DLL: correcta en O0 y O2.
- Suite completa del árbol: **297/297 PASS** en una sola corrida (`396.27 s`).

El build usado para la revalidación focal fue
`0.9.0+build.20260907T083449Z.987ab50278b400ff`. El número público no se cambió en esta entrega:
E1 es trabajo rumbo a 1.0, pero el cierre de versión se hará cuando todos sus gates estén unidos y
certificados.

## Estado de cierre

E1 está cerrado en implementación, pruebas focales y regresión completa. Antes de declarar el release completo sólo
corresponde integrar su resultado con los demás bloques y volver a certificar el baseline del árbol
final; no queda una segunda arquitectura de objetos pendiente dentro de este bloque.
