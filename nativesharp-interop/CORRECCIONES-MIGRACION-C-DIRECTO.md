# Correcciones de interoperabilidad C halladas en la migración a bibliotecas C directas

Origen: pruebas de la migración de un framework de bindings a headers C directos, hechas en otra máquina. Aquí se
reprodujo cada caso con pruebas mínimas propias, sin el código del framework. **Estas pruebas no validan el
framework**: eso se repite en la máquina original con esta versión de NSC y NSLink.

Toolchain local: Clang 23.1.0 (`C:\clang-llvm`), Windows SDK 10.0.26100.0, NSLink de `A:\nslink-devel`.

## Resumen

| # | Problema | Estado | Causa demostrada | Fixture |
|---|----------|--------|------------------|---------|
| 1 | Alias de tipo NativeSharp + header C | Corregido | `cppScopedTypeNames` abortaba ante un alias cuyo destino no es namespace ni tipo C/C++ | `pruebas-interop-alias-tipo-ns` |
| 2 | Consumidor de biblioteca con campo privado `struct` C | Corregido | El inventario léxico no reconocía `using A = Modulo::Ns.Tipo;` de la API pública reconstruida | `pruebas-interop-consumidor-campo-c` |
| 3 | `--api-diff`: 19 delegados | Corregido | Lista de calificadores vacía o con ceros según la biblioteca; el diff comparaba la lista cruda | `pruebas-interop-api-diff-delegados` |
| 3 | `--api-diff`: 43 genéricos | Sin cambio de código | Cambio real de entrada: la migración expandió alias en esos archivos | — |
| 4 | `no visible Stage 44 generic method template` | Corregido | Llamada de instancia con receptor valor de dueño no genérico resuelta solo por nombre y número de argumentos | `pruebas-interop-genericos-receptor` |
| 5 | `GetProcAddress` / `FARPROC` | Corregido | Solo se proyectaban punteros a funciones con prototipo | `pruebas-interop-farproc` |
| 6 | Macros de headers de sistema (`SM_MAXIMUMTOUCHES`, COM de XAudio2) | Pendiente, requiere diseño | Las macros de headers de sistema se excluyen a propósito (T14-001, T19-003) | — |
| 7 | NSLink: import por ordinal (NSL1047) | Corregido en NSLink | `ArchiveResolver` descartaba los miembros con `NameType = Ordinal` | `nslink.nsl22.import-ordinal` |

## 1. Alias de tipo NativeSharp con un header C

- Reproducción: enum NativeSharp + `using BufferUsageType = Demo.Saru.Framework.BufferUsage;` + header C con un
  `struct`. Falla con `cannot prepare scoped C/C++ type names ... unsupported resolution`; sin header compila.
- Causa: `include/nativesharp/Frontend/CppScopedTypeNames.h`. Para un alias sin sintaxis explícita de tipo se
  probaba namespace y luego tipo C/C++; si no era ninguno, `return std::nullopt` y todo el archivo fallaba.
  Un tipo NativeSharp nunca está en ese inventario (solo contiene grafías C/C++).
- Corrección: ese alias se omite del conjunto con ámbito. El conjunto solo **agrega** grafías C/C++ al índice
  NativeSharp del parser (`Parser::isKnownTypeName` hace OR), así que el alias queda a cargo del índice y de la
  búsqueda ordinaria, exactamente como sin headers.
- Conservado: un alias inválido se rechaza con y sin header con el mismo motivo
  (`using alias target '...' does not name a visible namespace or type`).

## 2. Consumidor de una biblioteca con un campo privado de tipo `struct` C

- Reproducción: productor con `private state s;` (struct C) en una clase; consumidor separado que solo referencia la
  biblioteca y Core. Falla igual que el caso 1; con `int` en el campo compila.
- Causa (instrumentación temporal, retirada): la unidad que fallaba es la fuente virtual
  `*.nsmeta.public-api.ns` del productor, que declara la base como
  `using __NscMetadataBase0 = NativeSharpCore::NativeSharp.Core.Object;` (`PublicSurfaceBuilder::collectBaseAlias`).
  `lexicalNamespaceScopes` no reconocía el destino calificado por módulo y lo marcaba como directiva no soportada.
  El campo privado C importa porque aporta un tipo C al consumidor: sin tipos C la preparación de nombres con
  ámbito no se ejecuta (`prepareCppScopedParserNames`), y por eso el control con `int` no falla.
- No comparte causa con el caso 1 (la corrección 1 sola no lo resuelve).
- Corrección: `include/nativesharp/Parser/LexicalNamespaceScopes.h` reconoce `using A = Modulo::Ns.Tipo;` como
  sintaxis explícita de tipo. El parser ya lee todo destino de alias con la gramática de tipos (`parseType`) y ya
  registra todo alias como nombre de tipo, así que no cambia la resolución ni el formato de metadata.
- Las sustituciones de Direct3D/XAudio que fallaban en la otra máquina no se pudieron reproducir aquí; no está
  demostrado que todas compartan esta causa.

## 3. `--api-diff`

### Delegados (19)

- Causa: `LibraryMetadataPayload.cpp` escribe la sección de calificadores de puntero solo si **algún** valor de la
  biblioteca es distinto de cero. La construcción guarda un cero por parámetro; al leer, una biblioteca sin ningún
  `T const*` deja la lista vacía y otra con alguno la reconstruye con ceros. `LibraryApiDiff.cpp` comparaba la
  lista cruda: misma declaración, contrato distinto. Es representación distinta con el mismo significado.
- Contrafactual (mismos `.nsmeta`, binario sin la corrección): agregar un método `ReadConst(int const* p)` marca
  `Reader` **y** `Combine` (sin punteros) como `source-breaking`; con la corrección, `source-compatible: yes`.
- Corrección: el contrato registra un valor por parámetro declarado, cero si falta (como lo lee
  `PublicSurfaceBuilder`). Un cambio real (`Reader(int* p)` → `Reader(int const* p)`) sigue siendo
  `source-breaking`, y ya no arrastra a `Combine`.
- Consecuencia: cambian las huellas `source-fingerprint` de bibliotecas con delegados (no se persisten en la
  metadata; solo se imprimen). El formato de metadata no cambia.

### Genéricos (43)

- Agregar solo un header C no cambia ningún contrato genérico (caso `v2-header` del fixture: `identical`). Las 43
  diferencias aparecieron en la otra máquina al expandir alias en los archivos fuente: los imports serializados de
  cada plantilla (y un cuerpo con `BufferUsageType.Dynamic`) cambian de verdad. Se espera que desaparezcan al
  restaurar los alias, lo que permite la corrección 1. Debe confirmarse en la máquina original.
- Hallazgo no corregido: `appendSourceGenericTemplates` inserta por `canonicalKey`, que para métodos es
  `Modulo::Ns.Tipo.Nombre``aridad` sin la firma; entre sobrecargas genéricas del mismo nombre y aridad solo se
  compara una. Un cambio en otra sobrecarga podría no detectarse. No se tocó (fuera del alcance pedido).

## 4. Plantillas genéricas "no visibles"

- Reproducción (también en un solo proyecto, no solo con bibliotecas precompiladas):
  `device.Draw<int>(v, ushortIdx)` con tres sobrecargas de igual aridad; y `d.Set<int>(data)` cuando **otra** clase
  declara `Set<T>(T[])`. Las llamadas estáticas `Device.Pick<int>(...)` sí funcionaban.
- Causa: `GenericCallInference.cpp`. Con receptor valor (`x.M<T>()`), el dueño pedido es la grafía de la variable;
  el tipo del receptor solo se consultaba si algún candidato tiene dueño genérico. Se caía a una búsqueda por
  nombre y número de argumentos en todas las plantillas visibles, ambigua en esos dos casos. Encaja con lo
  observado: `SetData<T>`/`GetData<T>` existen en varias texturas y búferes; `DrawUserIndexedPrimitives<T>` tiene
  sobrecargas de igual aridad; `SetConstantData<T>`, `Create<T>` y la `SetData<T>` de cuatro parámetros son únicos.
- Corrección: solo cuando esa búsqueda ya falló, el tipo estático del receptor elige el dueño (la ruta existente
  de dueños genéricos). Una llamada que hoy resuelve no cambia. Si el tipo no declara candidatos (método heredado)
  se conserva el resultado y el diagnóstico anteriores.
- Límites conocidos: métodos genéricos **heredados** con el mismo nombre en otro tipo siguen ambiguos; las llamadas
  sin argumentos genéricos explícitos (`d.Draw(v, i)`) siguen otra ruta y no se cambiaron.

## 5. `GetProcAddress` y `FARPROC`

- `FARPROC` es `INT_PTR (WINAPI *)()` en x64: puntero a función C **sin prototipo**. Cuatro comprobaciones
  rechazaban `!hasPrototype` (`CppSemanticFunction.cpp`, `CppAliasFunctionType.h`, dos en `QualifiedTypeNames.cpp`).
- Corrección: `interop::cppFunctionTypeCallable` (en `CppType.h`) acepta además una función C sin prototipo,
  `cdecl`, sin parámetros declarados ni variádica: la misma regla que ya seguía una declaración C directa sin
  prototipo. C hace compatibles `R (*)()` y `R (*)(void)`, que comparten tipo de función NativeSharp. En x64
  `WINAPI` se reduce a `cdecl`. Nulabilidad: el valor es un puntero a función comparable con `null`.
- Validado: resolver `GetCurrentProcessId`, convertir a `ProcessIdFunction` y llamarlo, llamar el `FARPROC` sin
  argumentos, y símbolo ausente → `null`. Sin adaptadores C.

## 6. Constantes y macros C (pendiente)

- Evidencia: con el mismo header (que incluye `windows.h` como sistema), `#define PROJECT_MAXIMUM_TOUCHES 95` del
  header del proyecto se importa (devuelve 95); `SM_MAXIMUMTOUCHES` da `unknown identifier`.
- Causa: `ClangSession.cpp` (`macroConstantsLocked`, `expandCFunctionMacroCall`) excluye a propósito las macros
  definidas en headers de sistema. Las macros COM de XAudio2 en C (`#define IXAudio2_Release(This)
  ((This)->lpVtbl->Release(This))`, `xaudio2.h`) son macros de función de un header de sistema: misma exclusión.
- Por qué no se corrigió aquí: importar todas las macros de `windows.h` agrega miles de nombres a cualquier proyecto
  que lo incluya (rompe la invariante de ausencia) y las evalúa en cada sesión. Se requiere un diseño bajo
  demanda: pasar a la sesión los identificadores que las fuentes NativeSharp usan (las sesiones se abren antes de
  analizarlas), evaluar solo esas macros de sistema, incluir ese conjunto en la identidad de caché de la sesión y
  definir la precedencia frente a declaraciones NativeSharp homónimas. Macros con argumentos y COM necesitan además
  decidir cuándo un nombre de sistema se trata como macro y no como función. Acceso a unions anónimas no evaluado.

## 7. NSLink: importaciones por ordinal

Ver el commit de `A:\nslink-devel`: `ImportDescription.ordinals`, `ArchiveResolver`, `LinkPipeline`,
`PeImportBuilder` (ILT/IAT con `IMAGE_ORDINAL_FLAG64 | ordinal`, sin Hint/Name). Con la `xinput.lib` del SDK,
`llvm-readobj` muestra `XINPUT1_4.dll` ordinal 2 en NSLink y en LLD; ambos ejecutables devuelven
`ERROR_DEVICE_NOT_CONNECTED` de la DLL real (sin mando físico conectado).

## Otros hallazgos (no corregidos)

- Los identificadores `i16`, `i32`, `u32` son nombres de tipo del lenguaje; usarlos como variables produce errores
  de sintaxis poco claros (`expected expression`).
- `nsc.cpp-interop.session` necesita `cpp-fixtures/project/CppProbe.nsproj`, que no existe en este checkout.
- `nsc.compiler.pass-kind-overloads` exige `nslink.exe` en `PATH`.
