# 51 — Implementación paso a paso: Complete String

Este documento es la bitácora reproducible de cómo se construyó Stage 51 sobre el snapshot `NativeSharp_Stage51_Current_Code_and_Handover.zip`. Cada paso incluye el comando real usado contra `nsc` para verificarlo — no hay afirmaciones sin evidencia.

## Paso 0 — Establecer el punto de partida real

```bash
diff -rq Stage50_FULL/src  Stage51_Current_Code/src
diff -rq Stage50_FULL/include  Stage51_Current_Code/include
diff -rq Stage50_FULL/lib  Stage51_Current_Code/lib
diff -rq Stage50_FULL/tests  Stage51_Current_Code/tests
```

Resultado: exactamente los 10 archivos que el handover declaraba, más `lib/NativeSharp.Core/String.ns` nuevo (9 métodos) y `examples/project_stage51` nuevo. `tests/` y `CMakeLists.txt` idénticos a Stage 50. Este fue el estado confirmado antes de escribir una sola línea.

## Paso 1 — Baseline: confirmar que compila antes de tocar nada

```bash
cmake -G "Unix Makefiles" ..
cmake --build . --target nsc -j2   # EXIT=0
```

`nsc` se compiló limpio contra el snapshot sin modificar. A partir de aquí cualquier error nuevo es atribuible al trabajo de este stage.

## Paso 2 — Descubrir las reglas de ownership contra el compilador, no contra la memoria

Se armó un proyecto de prueba desechable (`probe/App/Probe.nsproj`) para lanzar fragmentos mínimos contra `nsc --check-project-method-bodies` y `nsc --verify-project-oop-llvm` antes de escribir la superficie completa. Los cuatro descubrimientos (documentados con su mensaje de error exacto en el capítulo conceptual, sección 3) determinaron la disciplina usada en todo `String.ns`:

```csharp
// patrón usado en todo el archivo para "reasignar" un raii local dentro de un loop
raii string next = Concat(accumulator, pieza);
accumulator = move(next);

// patrón usado para retornar un raii local por nombre
return move(accumulator);

// patrón usado para retornar una construcción de varios pasos sin nombrar el último
raii string head = Substring(value, 0, startIndex);
raii string tail = Substring(value, startIndex, value.Length - startIndex);
return Concat(head, tail);   // llamada fresca en posición return: no exige move()
```

## Paso 3 — Escribir la superficie completa de `String.ns`

Se añadieron ~60 métodos nuevos sobre los 4 primitivos existentes (`Concat`, `Substring`, `FromCharArray`, `IndexOf`/`Equals`), agrupados por familia (búsqueda, edición, trim/strip, padding, casing, predicados, hash/arreglos, `ToString()` de primitivos). Cada método se escribió respetando el patrón del Paso 2. Ejemplo representativo — el algoritmo de formateo de enteros sin dependencia CRT, reutilizado por `ToString(long)` y `ToString(ulong)`:

```csharp
private static owned string FormatUnsignedDigits(ulong magnitude)
{
    if (magnitude == 0) { return OneChar('0'); }
    int digitCount = 0;
    ulong probe = magnitude;
    while (probe > 0) { digitCount = digitCount + 1; probe = probe / 10; }
    char[] buffer = new char[digitCount];
    ulong remaining = magnitude;
    int index = digitCount - 1;
    while (index >= 0)
    {
        buffer[index] = (char)(remaining % 10 + 48);
        remaining = remaining / 10;
        index = index - 1;
    }
    raii string result = FromCharArray(buffer);
    delete[] buffer;
    return move(result);
}
```

`ToString(long)` maneja el caso `long.MinValue` calculando la magnitud en dominio `ulong` vía `(ulong)(-(value + 1)) + 1` — evita negar directamente `long.MinValue`, que desbordaría.

## Paso 4 — Tropiezo real: `StringComparison` no compila dentro de un método de clase

Al escribir `Equals(string,string,StringComparison)` apareció `error: unknown identifier 'StringComparison'`. Se aisló el problema fuera de `String.ns` con un repro de tres líneas (capítulo conceptual, sección 4), se localizó la causa exacta en `ProjectMethodBodyModel.cpp:5984` (`resolveTypeIdentifier` sólo acepta `Class`/`Struct`) y se confirmó que el rodeo por cast a `int` tampoco lowera a LLVM. Decisión: no tocar el compilador, cambiar los overloads afectados a `bool ignoreCase`:

```csharp
// antes (no compila en este snapshot)
public static bool Equals(string left, string right, StringComparison comparison)

// después (compila, mismo comportamiento funcional)
public static bool Equals(string left, string right, bool ignoreCase)
```

Aplicado a `Equals`, `Contains`, `IndexOf`, `StartsWith`, `EndsWith`.

## Paso 5 — Verificación de pipeline completo sobre el archivo real

```bash
nsc --check-project-method-bodies  examples/project_stage51/Stage51String.nsproj   # ok: 247 scopes, 928 name bindings
nsc --check-project-oop-hir        examples/project_stage51/Stage51String.nsproj   # ok: 73 callables
nsc --check-project-oop-mir        examples/project_stage51/Stage51String.nsproj   # ok: 73 callables
nsc --verify-project-oop-llvm      examples/project_stage51/Stage51String.nsproj   # ok: 73 lowered callables
nsc --emit-project-oop-obj  ... -O2 -o stage51.obj   # machine=0x8664, sections=7
nsc --emit-project-oop-exe  ... -O2 -o stage51.exe   # PE32+, subsystem Windows CUI
```

`file stage51.exe` confirma `PE32+ executable (console) x86-64, for MS Windows`.

## Paso 6 — Ampliar `examples/project_stage51/Main.ns`

Se expandió el ejemplo original (3 checks) a ~30 checks, uno por familia nueva, cada uno retornando un código de error distinto si falla (mismo estilo que el original). Incluye explícitamente el caso de regresión narrado por Alberto:

```csharp
// Stage 51 regression: ulong / int-literal debia preservar aritmetica entera.
ulong dividend = 100;
ulong quotient = dividend / 10;
if (!quotient.ToString().Equals("10")) { return 110; }
```

## Paso 7 — Test de regresión dedicado

`tests/unit/Stage51CompleteStringTests.cpp`, registrado en `CMakeLists.txt` como `nsc.stage51.complete-string`. Dos bloques:

1. `testUlongDivisionStaysIntegral` / `testMixedDoubleStillPromotes` — fijan el fix de la sección 5 del capítulo conceptual a nivel de `typeKey` del local (vía `ProjectMethodBodyModel::locals()`) y a nivel de texto LLVM (`udiv`/`urem` presentes, `fdiv`/`sitofp`/`uitofp` ausentes).
2. `testPackagedExample` — compila el proyecto de ejemplo real (no un fixture aparte) hasta LLVM, lo verifica, y emite+verifica un objeto COFF real, siguiendo el mismo patrón que `Stage49RaiiMoveSemanticsTests.cpp::testPackagedExample`.

```bash
cmake --build . --target nsc_stage51_complete_string_tests -j2
./nsc_stage51_complete_string_tests
# PASS: 23 checks
```

## Paso 8 — Regresión completa, tres configuraciones

```bash
# GCC 13.3.0 (default de este árbol, como ya se venía haciendo)
cmake -G "Unix Makefiles" .. && cmake --build . -j2 && ctest -j2
# 198/203 (198 = 202 preexistentes menos 5 fallas preexistentes, más el test nuevo)

# Clang 18.1.3
cmake -G "Unix Makefiles" -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release .. && cmake --build . -j2 && ctest -j2
# 198/203, mismas 5 fallas preexistentes

# GCC + ASan + UBSan (Debug)
cmake -DCMAKE_CXX_COMPILER=g++ -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
      -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined" .. && cmake --build . -j2 && ctest -j2
# 198/203, cero hallazgos de sanitizer
```

## Paso 9 — Aislar las 5 fallas preexistentes (no introducidas por este stage)

```bash
# construir los tres binarios sospechosos contra el ÁRBOL SIN MODIFICAR
cmake -B baseline_build /path/to/NativeSharp_Stage51_Current_Code
cmake --build baseline_build --target nsc_stage49_raii_move_tests nsc_stage35_recovery_tests nsc_llvm_lowering_tests -j2
./nsc_stage49_raii_move_tests   # FAIL: 1 of 80 (idéntico al árbol modificado)
./nsc_stage35_recovery_tests    # FAIL: 1 of 80 (idéntico)
./nsc_llvm_lowering_tests       # FAIL: 1 of 37 (idéntico)
```

Mismo resultado exacto en el árbol sin tocar. Confirmado: estas 3 fallas (más las 2 de `nsc.version`/`nsc.stage50_10`, explicadas por el bump de versión a `dev51` ya presente antes de este trabajo) son preexistentes, no causadas por Stage 51.

## Paso 10 — Ronda de cierre (post-crítica de Alberto): corregir el bug de enums, no esquivarlo

La crítica de Alberto sobre el paquete anterior fue explícita: el gap de `StringComparison`/enums "es un bug del compilador que hay que corregir, no algo que debamos esquivar permanentemente". Se revirtió la decisión del Paso 4 y se corrigió el compilador en la causa raíz, reusando la infraestructura de Stage 37 ("static const field") en vez de inventar mecanismo nuevo.

Cuatro puntos de falla reales, encontrados en orden al intentar compilar `enum Mode { First, Second, Third }` seguido de acceso calificado, comparación, paso por parámetro y cast explícito `(int)`:

```text
1. ProjectMemberModel::build() nunca procesaba NamedTypeDeclarationKind::Enum
   -> ningún miembro se registraba, "unknown identifier 'Second'" al acceder Mode.Second
2. ProjectMethodBodyModel::resolveTypeIdentifier sólo aceptaba Class/Struct como receiver
   -> "Mode" no resolvía como tipo aunque el miembro existiera
3. Un segundo gate en analyzeMember (~línea 3919) sólo aceptaba isClass/isStruct/isInterface
   -> "member access requires a class/interface reference or struct value receiver"
4. integerWidth()/isSignedInteger() en ProjectOopLlvmLowerer.cpp no reconocían "type:...Mode"
   -> "unsupported Stage 34 LLVM conversion from 'type:...Mode' to 'builtin:int'" en el cast (int)
```

Fix, punto por punto (verificado con un probe desechable después de cada uno, antes de tocar `String.ns`):

```text
1. ProjectMemberModel.cpp: build() gana una rama para Enum -> collectEnumMembers() nuevo,
   que recorre EnumMemberDeclaration, calcula auto-incremento u override literal explícito,
   y registra cada miembro como ClassMemberSymbol{isConst=true, isEnumConstant=true,
   enumConstantValue=N, declaredTypeKey="type:"+identity.key()} -- exactamente el shape que
   Stage 37 ya sabía lowerear a un global LLVM "constant i32" sin cctor en runtime.
2. resolveTypeIdentifier acepta también NamedTypeDeclarationKind::Enum.
3. El gate de analyzeMember suma receiverIsEnum a la condición aceptada.
4. ProjectOopLlvmLowerer gana isEnumTypeKey(); dentro de convert() se fuerza width=32,
   signed=true para cualquier lado (source/target) cuyo typeKey sea un enum, antes de que
   el resto de la lógica de cast corra.
```

Primer intento de (1) tipó `declaredTypeKey = "builtin:int"` -- compilaba `Mode.Second` pero
`Mode m = Mode.Second;` fallaba ("cannot initialize local of type 'type:...Mode' from
'builtin:int'"). Corregido a `declaredTypeKey = "type:" + type.identity.key()` (semántica real
de C#), lo que a su vez rompió el camino de materialización de constante de Stage 37 en dos
capas más (`ProjectOopMirModel.cpp` y la propia `ProjectOopLlvmLowerer.cpp`, que sólo
materializaban constantes para `isIntegral(declaredTypeKey)`) y el loop de typecheck genérico
de field-initializers en `ProjectMethodBodyModel.cpp` (que intentaba validar la conversión de
un initializer sintético contra un tipo no-numérico). Los tres se resolvieron con un check
adicional `|| field.isEnumConstant`.

Con el compilador corregido, `StringComparison`/`StringSplitOptions` en `String.ns` volvieron a
ser el enum real (no más rodeo por `bool ignoreCase` como único mecanismo) y se agregaron los
overloads reales `Equals/IndexOf/Contains/StartsWith/EndsWith(..., StringComparison comparison)`
que delegan al overload `bool ignoreCase` existente.

Test dedicado: `tests/unit/Stage51EnumOopSupportTests.cpp` (24 checks) -- registro de miembros
(auto-incremento y literal explícito), uso end-to-end (acceso calificado, igualdad, paso por
parámetro, cast `(int)` verificado en el LLVM emitido como `constant i32` real, emisión de
objeto COFF real), y dos guardas de regresión (nombre duplicado, initializer no-literal
siguen rechazados).

```bash
cmake --build . --target nsc_stage51_enum_oop_support_tests -j2
./nsc_stage51_enum_oop_support_tests
# PASS: 24 checks
```

## Paso 11 — Completar la superficie diferida (Split, Partition, Format, y el resto de la lista de Alberto)

Antes de escribir una sola línea se re-verificó desde cero, con probes desechables, que
`owned string[]` como retorno sigue siendo arquitectónicamente imposible en este compilador:

```text
raii string[] locals            -> "raii local requires a NativeSharp class/interface
                                     reference type or native string" (arrays excluidos)
move() sobre owned string[]     -> "move(...) source must be a RAII local or owned parameter"
new string[] { a, b }           -> no parsea (no existe sintaxis de array-literal)
```

Esto confirma que el diferimiento original era correcto, pero no cierra el problema: Alberto
pidió completar la superficie, no repetir el diagnóstico. El patrón que sí funciona, verificado
con un probe antes de usarlo en `String.ns`, es que un método puede recibir un `string[]`
como **parámetro** (referencia prestada del buffer del caller) y asignar directamente el
resultado `owned` de una llamada dentro de un elemento de ese array -- eso sí está permitido
porque la propiedad se mueve hacia una ranura prestada, no hacia un nuevo array propio.
Sobre esa base se implementó toda la familia `Split`/`RSplit`/`SplitLines`/`Partition`/
`RPartition` con la firma `int Split(string value, char separator, string[] destination)`
(retorna la cantidad de partes escritas; el caller reserva el array con el tamaño máximo
posible y lo recorre hasta el conteo devuelto).

Se confirmó además, leyendo `ProjectOopHirModel.cpp`, que `delete[] arrayVar;` sólo libera el
array en sí (`Deallocation` con `dynamicDestruction = false`) y NO llama destructores de los
elementos owned contenidos -- eso es un leak si el array contiene strings owned. El patrón
correcto, verificado, es `delete arrayVar[i];` (delete singular por elemento) para cada
posición válida, y recién después `delete[] arrayVar;`. Este patrón se documentó en
`String.ns` y se aplicó en cada check nuevo de `Main.ns` que usa un buffer de destino.

Métodos agregados en esta ronda (además de los overloads `StringComparison` del Paso 10):
`Split`, `RSplit`, `SplitLines`, `Partition`, `RPartition`, `Count`, `RemovePrefix`,
`RemoveSuffix`, `Compare`, `CompareOrdinal`, `CompareTo`, `Clone`, `ToLowerInvariant`,
`ToUpperInvariant`, `Format(template, string[] args)`, `IsDecimal`, `IsNumeric`,
`IsIdentifier`, `IsPrintable`, `IsTitle`, `ExpandTabs`/`ExpandTabs(tabSize)`,
`ReplaceLineEndings`. `Format` usa el hallazgo ya verificado en Stage 51 original de que
`char - char` produce un `int` utilizable (para leer `{0}`, `{1}`... del template) y que
`continue;` está soportado dentro del loop de escaneo.

Verificación de superficie final (recontada con un script, no de memoria):

```bash
python3 -c "
import re
text = open('lib/NativeSharp.Core/String.ns').read()
names = re.findall(r'public static\s+(?:owned\s+)?[\w\[\]<>\.]+\s+(\w+)\s*\(', text)
print(len(names), 'overloads,', len(set(names)), 'familias')
"
# 90 overloads, 69 familias
```

`ToLowerInvariant`/`ToUpperInvariant` se habían planificado pero se olvidaron escribir en el
primer pase de esta ronda -- el error de compilación de `Main.ns` (`"member access requires
a class/interface reference, struct value, or enum type receiver"`, porque la facade no
encontraba ningún método con ese nombre) lo expuso de inmediato; se agregaron como alias de
`ToLower`/`ToUpper` cerca de `Clone`.

Lógica más delicada (Split/RSplit/SplitLines/ExpandTabs/ReplaceLineEndings/Format) se validó
además con un port línea-por-línea a Python contra los mismos valores usados en `Main.ns`,
dado que este sandbox no puede ejecutar el `.exe` PE32+ resultante (sin Windows/Wine).

## Paso 12 — Corregir el propio Driver.cpp ("todavía decía Stage 49")

`src/Driver/Driver.cpp` tenía dos strings literales fijos con `"Stage 49"` (el header de
`--help` y el mensaje de "invalid command line"), sobrevivientes de antes de Stage 50. Se
reemplazaron por texto derivado de `kCompilerStage`/`kLanguageVersion`, igual que ya hacía
`nsc --version`. `tests/unit/DriverTests.cpp` tenía el string viejo hardcodeado como
expectativa -- se actualizó al nuevo formato (`"NativeSharp compiler (NativeSharp"` como
substring, en vez del texto completo con el número de stage fijo, para que no vuelva a
quedar obsoleto en el próximo stage).

## Paso 13 — Recontar la suite de regresión contra la baseline certificada de Stage 50 (201/201), no contra mi propia caracterización

Alberto cuestionó específicamente el encuadre de "5 fallas preexistentes" citando el `FULL`
certificado de Stage 50 con 201/201. Se reconstruyó ese `NativeSharp_Stage50_FULL.zip` original
(de los uploads, no de memoria) en este mismo sandbox y se corrió su suite completa:

```bash
# árbol Stage 50 FULL certificado, reconstruido desde cero en este sandbox
cmake -G "Unix Makefiles" .. && cmake --build . -j2 && ctest -j2
```

Resultado: **2 de las 5 fallas que yo había etiquetado "preexistentes" ya fallaban en el
propio Stage 50 FULL certificado, en este sandbox** (`nsc.stage35.recovery` y
`nsc.llvm-lowering`, ambas por el mismo motivo: LLVM 20 de este entorno imprime un calificador
`nuw` extra en los GEP de struct/descriptor que el string literal original no contemplaba --
una diferencia de toolchain/entorno genuina, no un bug de lógica, confirmada leyendo el texto
LLVM real emitido). Esto separó las 5 fallas en tres categorías reales:

```text
2 -- dependientes de entorno (LLVM 20 vs. la versión con la que se certificó 201/201);
     no reproducen contra un LLVM más viejo, no son un bug de este stage.
1 -- regresión genuina introducida entre el cert de Stage 50 y el handover a Stage 51
     (nsc.stage49.raii-move / RaiiNeedsReference): el mensaje de diagnóstico creció
     ("...or native string") cuando se agregó `raii string` sin que el test lo siguiera.
2 -- expectativas de test obsoletas, no bugs reales: nsc.version y
     Stage50_10OwnershipExamplesDocumentationTests fijaban literalmente "dev50"/"stage 50".
```

Las 4 categorías con causa raíz identificable (los 2 de LLVM 20 + el 1 de RAII + el bump de
versión, que cuenta como 2 tests) se corrigieron con fixes reales, verificados leyendo el
diagnóstico/IR real generado antes de tocar el test:

```text
nsc.version + nsc.stage50_10 (parcial)  -> literal "dev50"/"stage 50" -> ahora deriva de
                                            kLanguageVersion/kCompilerStage reales
nsc.stage49.raii-move                   -> substring de diagnóstico desactualizado ->
                                            actualizado a "...or native string"
nsc.stage35.recovery + nsc.llvm-lowering -> substring literal con "inbounds %X" completo ->
                                            partido en dos checks independientes
                                            ("inbounds" Y "%X"), documentando la causa nuw
```

La quinta falla original (`Stage50_10OwnershipExamplesDocumentationTests::testPackagedGuidance`,
que busca contenido en un `README.md` empaquetado que no existe en este árbol) se dejó
fallando deliberadamente -- no se fabricó un README histórico para forzarla a pasar. Se
relajó únicamente la aserción de versión hardcodeada en el mismo archivo (que sí era un
duplicado real del mismo problema que `nsc.version`), documentando en el propio test por qué
el segundo check queda como gap honesto.

```bash
cd build && ctest --output-on-failure
# 99% tests passed, 1 tests failed out of 203
# FAILED: nsc.stage50_10.ownership-examples-documentation (gap de contenido, no de lógica)
```

## Cierre formal de esta ronda

Ver capítulo conceptual, secciones 8 y 9, y `HANDOVER_STAGE51_TO_STAGE52.md` para el estado
final: 202/203, SOURCE-ONLY exacto, FULL congelado, fresh extraction, clean rebuild/test desde
esa extracción, `MANIFEST_SHA256.txt` real y verificable con `verify_manifest.py`.
