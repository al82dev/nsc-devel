# 133 — 1.0-E2: genéricos entre bibliotecas

## Resultado de la entrega

E2 quedó implementado. Un consumidor separado puede usar una plantilla genérica pública publicada
por una biblioteca, reutilizar una especialización que el productor ya generó y crear una
combinación nueva con el mismo monomorfizador del compilador.

La prueba central hace las dos cosas en la misma compilación:

```csharp
Precompiled.Box<int> warm;       // ya especializada por el productor
warm.Value = Precompiled.GenericAlgorithms.Identity<int>(42);

Precompiled.Box<long> fresh;     // especialización nueva en el consumidor
fresh.Value = Precompiled.GenericAlgorithms.Identity<long>((long)0);
```

El contrato también transporta constraints. Un `ScoreOf<T> where T : IScore` acepta la clase
precompilada que implementa `IScore` y rechaza otra que no la implementa, nombrando la interfaz
canónica en el diagnóstico.

## Decisión de representación

Al comenzar E2 todavía estaba abierta la pregunta de cómo transportar una implementación genérica.
Se evaluó la idea de serializar un AST o HIR especial. Eso habría creado un segundo formato de
lenguaje, un segundo clonador y otra ruta de compatibilidad además del parser y el monomorfizador
existentes.

La implementación final publica una **rebanada canónica de declaración** en NSMETA 1.7. No es texto
incidental ni el archivo completo del productor: es solamente la declaración pública que el
productor decidió convertir en plantilla materializable. Sus reglas son:

- sólo se emiten plantillas públicas;
- la rebanada forma parte del contrato versionado y de su huella;
- el consumidor la parsea como una unidad sintáctica separada;
- el monomorfizador ordinario sigue siendo el único que genera especializaciones;
- los miembros y dependencias privados no se publican por arrastre;
- metadata 1.7 con una plantilla pública incompleta se rechaza antes de compilar al consumidor.

Esta elección mantiene una sola semántica ejecutable. Si cambia el lenguaje aceptado por una
plantilla pública, cambia su contrato; no hay un bytecode interno paralelo que pueda divergir del
frontend.

## Arquitectura que quedó

```text
NSMETA 1.7
  TEMPLATE: identidad + visibilidad + parámetros + declaración pública
  SPECIALIZATION: clave canónica + nombre especializado + argumentos
                    │
                    ▼
ProjectCompilation
  unidad A: API precompilada          isPrecompiled = true
  unidad B: plantillas públicas       isPrecompiled = false
                    │
                    ▼
ProjectGenericModel
  selecciona identidad visible
  valida constraints
  consulta cache importada
     ├── encontrada: reutiliza símbolo del productor
     └── nueva: clona y monomorfiza en el consumidor
```

La separación entre unidad A y B evita el error conceptual más peligroso del bloque: si toda la
clase parcial se marcara como precompilada, el cuerpo genérico público se trataría como externo y
nunca se materializaría; si toda se marcara como local, el consumidor intentaría regenerar los
miembros ordinarios del productor. Por eso `isPrecompiled` ahora vive también en cada miembro.

## Reutilización frente a materialización

`ProjectGenericModel::build` recibe las especializaciones cerradas publicadas por las referencias y
precarga `specializationByKey_`. La clave incluye plantilla, identidad canónica de argumentos y
aridad. Si el consumidor pide exactamente esa clave, recupera `specializedName` y enlaza la
implementación existente. Si pide otra clave, se ejecuta `ensureTypeSpecialization` o
`ensureMethodSpecialization` sobre la plantilla pública hidratada.

Esto evita dos fallos opuestos:

- duplicar en el consumidor una especialización que ya vive en el productor;
- fingir que una combinación nunca producida existe dentro de la biblioteca.

## Identidad nominal y ambigüedad

Durante las pruebas apareció un bug independiente del transporte: al reescribir
`Precompiled.Box<int>`, el nombre especializado perdía el calificador y podía buscarse en el módulo
del consumidor. `processTypeSyntax` conserva ahora el prefijo escrito cuando sustituye el nombre de
la plantilla.

También se eliminó la selección por “primer módulo que coincida”. Dos bibliotecas pueden publicar
un `Precompiled.Box<T>` con la misma escritura visible y distinta identidad de módulo. En ese caso
`resolveTypeTemplate` devuelve ambigüedad y el compilador diagnostica antes de HIR y enlace. El
agrupamiento de métodos genéricos incluye igualmente la identidad del módulo; dos dueños que se
escriben igual no se mezclan como si fueran overloads de una sola clase.

El cambio actualizó un fixture de Stage 44: el contrato del diagnóstico pasó de “no es una
plantilla visible” a “no nombra una plantilla visible única”, texto que cubre de forma honesta tanto
ausencia como colisión.

## Constraints transportados

La rebanada publicada conserva el `where` original. Al hidratarse, la plantilla entra por las mismas
fases de tipos y constraints que una declaración local. La interfaz importada ya tiene identidad
nominal y tabla semántica gracias a E1, así que `where T : IScore` no se reduce a comparar textos:
se valida contra la relación de tipos reconstruida.

La prueba positiva usa `BonusCounter`; la negativa usa `Counter`, que deliberadamente no implementa
el contrato. Esto reproduce y cierra el problema real de constraints con tipos definidos en otro
proyecto.

## Cambios por archivo y responsabilidad

### Formato de biblioteca

- `include/nativesharp/Interop/LibraryMetadata.h`
  - Avanza el formato a NSMETA 1.7.
  - `LibraryMetadataGenericTemplate::accessibility` hace explícita la publicación.
  - `LibraryMetadataGenericTemplate::sourceText` contiene la declaración materializable.
- `src/Interop/LibraryMetadata.cpp`
  - `buildMetadataModel` omite plantillas privadas y extrae la rebanada pública.
  - `encodePayload` y `decodePayload` condicionan ambos campos al minor 7.
  - `verifyCurrentCompiler` rechaza una plantilla 1.7 no pública o sin declaración.

### Carga de referencias

- `include/nativesharp/Frontend/ProjectCompilation.h`
  - `PrecompiledSyntaxModule` transporta por separado `templateSourceText` y las
    `genericSpecializations` conocidas.
- `src/Driver/Driver.cpp`
  - `buildPrecompiledGenericTemplateSource` arma una unidad sólo con plantillas publicadas.
  - Para métodos genéricos, reconstruye una parte `public partial class` del dueño canónico.
  - `configurePrecompiledModules` convierte los registros NSMETA al contrato del frontend.
- `src/Frontend/ProjectCompilation.cpp`
  - Agrega la API importada como unidad precompilada.
  - Agrega la unidad de plantillas como fuente materializable ordinaria.
  - Junta la cache de especializaciones de todas las referencias antes de construir genéricos.

### Selección y monomorfización

- `include/nativesharp/Frontend/ProjectGenericModel.h`
  - `PrecompiledGenericSpecialization` representa una entrada de cache importada sin depender de
    las estructuras de metadata del driver.
  - `ProjectGenericModel::build` recibe esas entradas.
- `src/Frontend/ProjectGenericModel.cpp`
  - Precarga la cache y reutiliza nombres especializados del productor.
  - Conserva el calificador de tipos genéricos escritos explícitamente.
  - Rechaza identidades de tipo ambiguas en lugar de escoger la primera.
  - Incluye el módulo al agrupar candidatos de método genérico.
- `include/nativesharp/Frontend/ProjectMemberModel.h` y
  `src/Frontend/ProjectMemberModel.cpp`
  - Conservan procedencia por miembro para clases parciales híbridas.
- `src/Frontend/ProjectMethodBodyModel.cpp` y `src/Frontend/ProjectOopHirModel.cpp`
  - Analizan y bajan el miembro materializado local, pero mantienen externos los miembros
    ordinarios importados de la misma clase.

## Matriz de pruebas

`tests/unit/Stage46NativeLibrariesTests.cpp` cubre:

- `Box<T>` y `Identity<T>`;
- reutilización de `Box<int>` e `Identity<int>` precalentados por el productor;
- materialización nueva de `Box<long>` e `Identity<long>`;
- constraint de interfaz positivo y negativo;
- plantilla privada `Secret<T>` no visible;
- dos módulos con la misma escritura genérica: error de ambigüedad;
- productor compuesto que consume una plantilla de su dependencia;
- biblioteca estática y DLL;
- O0 y O2;
- ambos caminos producen y ejecutan el resultado 42.

`tests/unit/Stage48_2LibraryMetadataTests.cpp` verifica que las dos plantillas públicas tienen
visibilidad pública y declaración no vacía, además de las especializaciones cerradas.

`tests/unit/Stage50_9MetadataHostCompatibilityTests.cpp` escribe de verdad un archivo NSMETA 1.6,
lo lee con el lector 1.7 y comprueba que los campos ausentes toman su valor vacío sin desplazar la
especialización que viene después. También prueba que una plantilla incompleta del formato actual
se rechaza y que la misma plantilla pasa al proporcionar su declaración.

Los canarios existentes de constraints y sobrecargas también se revalidaron:

- `nsc.cc1.correctness.cc1-c44-constraints-genericos`: **PASS**.
- `nsc.cc1.correctness.cc1-c46-sobrecargas-genericas`: **PASS**.
- `nsc.cc1.correctness.cc1-c48-constraints-cross-project`: **PASS**.

## Resultado verificable

- `nsc.stage44.generics-monomorphization`: **PASS, 73 checks**.
- `nsc.stage46.native-libraries`: **PASS, 126 checks**.
- `nsc.stage48_2.library-metadata`: **PASS**.
- `nsc.stage50_9.metadata-host-compatibility`: **PASS**.
- Ejecución de consumidores estáticos y DLL: correcta en O0 y O2.
- Suite completa del árbol: **297/297 PASS** en una sola corrida (`396.27 s`).

El build focal fue `0.9.0+build.20260907T083449Z.987ab50278b400ff`. Igual que E1, este bloque no
cambia por sí solo el número público: aporta contratos de 1.0 sobre la rama 0.9 hasta que el conjunto
completo pase su gate de release.

## Estado de cierre

E2 está cerrado en implementación, pruebas focales y regresión completa. La representación materializable, la cache,
los constraints, la identidad, la ambigüedad y la composición transitiva tienen cobertura. Lo que
resta después de esta entrega pertenece a los siguientes bloques de 1.0 y a la certificación final
del árbol conjunto, no a una segunda fase oculta de genéricos entre bibliotecas.
