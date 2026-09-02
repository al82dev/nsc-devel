# Stage 56 / C2–C3–C4 — Implementación paso a paso

Registro mecánico: qué archivo se tocó, en qué orden y por qué. El diseño y el razonamiento están
en `64_stage56_c2c4_language_completion.md`; esto es el cómo.

Punto de partida: el árbol tal como quedó tras C1 (`ctest` 233/233, build limpio con
`-DNSC_WARNINGS_AS_ERRORS=ON`).

---

## Paso 0 — Medir los huecos antes de arreglarlos

| Programa | Resultado medido |
|---|---|
| `public float rgba[4];` (grafía C) | `error: expected ';', found '['` — no es la sintaxis |
| `public float[4] rgba;` + `c.rgba[0]` | `error: index expression requires an indexer reference or a pointer with one integral index` |
| `int* p; p[0]` | pasa el chequeo anterior y muere en HIR: *"Stage 38 OOP lowering requires class/interface index expressions to resolve to an indexer accessor"* |
| `public union Val { ... }` | `union` no existe |
| `using DWORD = uint;` | `error: expected declaration name, found ';'` |

Y el hallazgo que fijó el diseño de C2.1: la clave de tipo de `float[4]` era
`builtin:float[38:4:3:4]` — **una huella de la sintaxis del contador**, no el contador. Nada aguas
abajo podía leer de ahí ni el tipo del elemento ni cuántos hay, y por eso el tipo parseaba y
maquetaba bien pero no había forma de bajar un acceso.

## Paso 1 — C2.1a: la clave de tipo lleva el contador

`ProjectMemberModel` — `evaluateConstantArrayCount` (movido desde `ProjectObjectLayoutModel`, que
ahora delega), más `fixedArrayElementTypeKey` y `fixedArrayCount`. `canonicalTypeKey` emite
`builtin:float[4]`; la huella de sintaxis queda sólo como fallback para un contador **no**
constante, que es un error que el layout model ya reporta con su propio diagnóstico.

La delegación importa: la clave canónica y el layout tienen que coincidir sobre "qué cuenta como
contador constante", o la clave de un campo y su tamaño describirían arrays distintos.

`ProjectObjectLayoutModel` — `checkedMultiply` quedó sin uso al mover el evaluador y se eliminó
(`-Werror=unused-function` lo habría roto de todas formas).

## Paso 2 — C2.1b: acceso a elementos, para las dos formas crudas

`ProjectMethodBodyModel` — `MethodCoreOperationKind::RawElement`, reconocido en `IndexExpression`
para **una** base fija (`T[N]`) o **una** base puntero (`T*`), con un índice integral. Una sola
operación cubre las dos: la aritmética es idéntica y ninguna tiene cabecera de longitud.

`ProjectOopHirModel` / `ProjectOopMirModel` — nodo `RawElementAddress`, hijos `[base, índice]`. La
única diferencia entre las dos bases es cómo se obtiene la base, y es real: un array embebido **es**
su almacenamiento, así que se baja en contexto de dirección; un puntero **contiene** una dirección,
así que se baja como valor ordinario. En contexto de valor el nodo se envuelve en `PointerLoad`; en
contexto de dirección se devuelve tal cual, y el `Store` escribe directo.

El tamaño/alineación del elemento sale de `elementLayout`, extraído de la lambda que ya vivía en el
caso del array manejado, para que las dos rutas pregunten lo mismo de la misma forma — dos
respuestas que pudieran discrepar sobre el tamaño de un struct darían un stride de GEP que no
coincide con el layout del campo.

`ProjectOopLlvmLowerer` — el caso nuevo es un GEP byte a byte: `base + índice * elementSize`. Sin
bounds check y sin null check, a diferencia de `ArrayElementAddress`: es el tier crudo. Y `llvmType`
mapea `T[N]` a `[N x T]`, que es lo que hace que un **local** de array embebido sea un solo alloca.

## Paso 3 — C2.1c: lo que un array embebido no puede hacer

`ProjectMemberModel` — un array embebido como tipo de parámetro o de retorno se rechaza, con el
arreglo en el mensaje. C dice lo mismo degradando a puntero y negándose a retornarlo; la
alternativa era copiar 16 bytes en silencio.

## Paso 4 — C2.2a: la regla de colocación, en la capa que corresponde

`TargetDataLayout` — `StructLayoutOptions` (`isUnion`, `pack`, `minimumAlignment`) y
`StructFieldRequest` (layout del campo + offset explícito opcional), más un `layoutStruct` general.
Una sola pasada cubre las cuatro formas porque difieren nada más en cómo se elige el offset y cómo
avanza el tamaño:

```text
struct plano  -> siguiente offset alineado; el tamaño crece pasando cada campo
union         -> offset 0 para todos; el tamaño es el del campo MAYOR
pack(N)       -> la alineación del campo se acota, así que el offset alineado queda más apretado
at(N)         -> el campo nombra su offset; solaparse está permitido, deliberadamente
```

La alineación del agregado es la mayor alineación EFECTIVA (post-pack), elevada a
`minimumAlignment` si se pidió una, y el tamaño final se redondea a ella — la misma regla de
padding final de C, que es lo que hace que `sizeof()` coincida con un compilador C.

El overload de un argumento se conserva porque casi ningún llamador tiene algo que decir sobre el
layout.

## Paso 5 — C2.2b: la sintaxis

`Parser` — tres adiciones, todas con palabras clave **contextuales** (siguen siendo identificadores
en cualquier otro lugar, igual que `partial`):

- `union Nombre { ... }` se enruta a `parseStructDeclaration(StructLayoutForm::Union)`, que marca
  el nodo. **No** hay un `NamedTypeDeclarationKind` nuevo: una union es un struct en todo lo que
  este compilador modela salvo dónde caen sus campos, y un kind paralelo rippleaba por cada
  `switch` del compilador para nada.
- `pack(N)` / `align(N)` como modificadores de declaración, validados como potencia de dos y
  rechazados sobre class/interface/enum (ahí no hay colocación de campos que negociar).
- `at(N)` como modificador de campo, aceptado **en cualquier posición** entre los modificadores —
  ningún orden de `public`, `static` y `at(4)` es más correcto que otro, y aceptar sólo uno sería
  una regla arbitraria de recordar.

`ProjectMemberModel` — `explicitFieldOffset` en el símbolo, con el diagnóstico para los casos donde
un offset no significa nada (campo estático, miembro de una clase).

`ProjectObjectLayoutModel` — lee los marcadores de la declaración (`union`, `pack=N`, `align=N`) y
los pasa al layout. **Éste es el único lugar del compilador que sabe que una union no es un struct
ordinario.** Los marcadores no reconocidos se ignoran en vez de ser fatales: un marcador que este
build no conozca no debe convertir una declaración válida en un error duro.

`StructValueLayout` — se expone lo que la declaración pidió (`isUnion`, `pack`, `minimumAlignment`,
`hasExplicitFieldOffsets`) junto al resultado calculado, para que quien deba **reproducir** el
layout en otro lenguaje —el generador de headers C, sobre todo— emita `union` / `#pragma pack(N)` /
un atributo de alineación en vez de adivinar a partir de offsets que casualmente coinciden.

## Paso 6 — C2.2c: el borde C

`CHeaderGenerator`:

- un array embebido se emite con la extensión **después del nombre** (`float rgba[4];`), que es
  como C lo escribe;
- una union se emite como `union`, y el `typedef` adelantado usa el mismo tag (C rechaza
  `typedef struct X X;` seguido de `union X { ... }`);
- `pack(N)` se emite como el par `#pragma pack(push, N)` / `pop`;
- `align(N)` se emite como `NS_ALIGNAS(N)` **sobre el primer miembro**, que es donde C11 acepta un
  especificador de alineación y donde eleva la alineación del agregado entero (después del tag no
  es C válido — se descubrió compilando el header con clang, no razonando sobre el estándar);
- offsets explícitos se **rechazan** con la razón y la alternativa: un struct C plano no puede
  expresar campos solapados, y emitir uno produciría un header cuyas propias aserciones fallan en
  el host.

## Paso 7 — C3: aliases FFI

`TypeSyntax` — `setBaseTokenKind` y `prependModifiers`. Son lo que necesita la expansión: sustituir
la base, y meter los modificadores del alias **por debajo** de los escritos en el sitio de uso, para
que `using PDWORD = uint*;` usado como `PDWORD*` dé `uint**` y no `uint*` dos veces.

`Parser` — el destino de `using X = ...;` pasa a ser un **tipo completo**, parseado por `parseType`.
Un nombre punteado sin modificadores sigue conservando también su texto, que es lo que mantiene
intactos los alias de **namespace**. El nombre del alias se registra en `knownTypeNames_`, sin lo
cual `DWORD d = 42u;` se parsea como expresión y falla (lo atrapó la primera prueba de humo).

`ProjectNameLookup`:

- `LookupEnvironment` gana `typeAliases` (nombre → sintaxis de tipo); `aliases` sigue siendo la
  mitad de namespaces. Un alias está en exactamente uno de los dos mapas, y quién decide es
  `resolveNamespaceTarget`, no la sintaxis.
- Un destino que es **otro alias en scope** se acepta (encadenamiento); uno que no resuelve a nada
  sigue dando el diagnóstico de siempre.
- **La expansión ocurre en `bindTypeSyntax`, dentro del propio nodo**, con un tope de profundidad
  que reporta un ciclo en vez de colgarse. Después de eso el AST dice lo que el alias decía, y
  nada aguas abajo se entera de que los aliases existen. Eso es exactamente lo que significa un
  typedef, y es la razón por la que esta fase no tocó claves de tipo, layout, ABI, HIR ni backend.
- El **nombre escrito** se captura antes de expandir y se conserva en la ligadura. Sin eso, la
  suite `nsc.project-name-lookup` fallaba: la ligadura decía `Point` donde la persona había escrito
  `PointAlias`, y los diagnósticos habrían hablado de código que nadie escribió. Lo atrapó la suite
  existente en el primer build, no una revisión.

## Paso 8 — C4: `ref` / `out` / `in`

La decisión de fondo: las tres bajan a **un puntero**, que es maquinaria que C1 ya certificó de
punta a punta. Nada nuevo llega a MIR ni al backend; lo nuevo son reglas y un dereferenciado
implícito.

`Parser` — `ref`/`out`/`in` antes del tipo de un parámetro **añaden un modificador puntero al
tipo** y dejan la forma en el nodo. En el sitio de la llamada, `f(ref x)` se construye como la
expresión de dirección que el parámetro puntero espera, con la palabra clave anotada.

`ProjectMemberModel` — `ParameterPassKind` y `pointeeTypeKey` en el símbolo. La forma entra en la
**clave de firma**: `F(int* p)` y `F(ref int p)` bajan a la misma firma de máquina pero son
miembros distintos del lenguaje, y sin ese prefijo chocarían como duplicados.

`ProjectOopHirModel` — la forma entra también en el **nombre mangleado**, por la misma razón, o los
dos se volverían una sola función LLVM. Y el uso de un parámetro por referencia baja a
`PointerLoad(ParameterValue)` en contexto de valor y a `ParameterValue` (el puntero) en contexto de
dirección — exactamente como ya se comporta `*p` sobre un parámetro puntero explícito.

`ProjectMethodBodyModel`:

- un uso del parámetro evalúa al **pointee**, no al puntero: ése es el dereferenciado implícito, y
  es la diferencia ergonómica entera con un `int*` pelado;
- `in` no es asignable, por definición;
- `checkArgumentPassKinds` exige que el sitio de la llamada y la firma coincidan en la palabra
  clave, en **las dos direcciones**;
- cuando exactamente una sobrecarga tiene la aridad correcta y lo único mal es la palabra clave,
  se emite ese mensaje en vez del genérico "no viable overload" — el desajuste `int` vs `int*` que
  vería la persona es un artefacto de la representación, no algo que escribió. Hay dos rutas de
  resolución de llamadas en este archivo y las dos recibieron la misma precisión;
- `verifyOutParameterAssignments` cierra la obligación de `out`. Cuenta como asignación cualquier
  escritura **a través** del parámetro (`rect.left = 0;`, `valores[0] = 1;`, `*p = v;`), porque la
  forma GetMessage/GetClientRect es justo para lo que existe `out` sobre un struct — el fixture
  contractual `c4_ref_out_in` fue lo que expuso ese caso, que la primera versión (sólo asignación
  directa) rechazaba mal. El alcance exacto del chequeo, y lo que deliberadamente **no** prueba,
  está dicho en el comentario de la propia función y en el documento conceptual.

## Paso 9 — Tests, fixtures, ejemplo

`tests/unit/Stage56C2C4LanguageCompletionTests.cpp` — 16 escenarios, **81 checks**: layout y acceso
de arrays embebidos; array local y `p[i]`; el negativo de pasarlo por valor; union/pack/align/at con
sus números de layout; los modificadores rechazados donde no significan nada; **la certificación
contra clang** del header generado; el rechazo de offsets explícitos en el borde C; **la matriz de
tamaños 1/2/3/4/8/12/16/24/64/80 contra clang**; las cuatro clases de alias más encadenamiento; el
alias de namespace como regresión; `ref`/`out`/`in` de ida y vuelta; la palabra clave obligatoria y
exacta; `in` de sólo lectura y `out` como obligación; y la sobrecarga por ref-idad.

`tests/interop_contract/` — tres fixtures nuevos (`c2_11_fixed_field_array`, `c2_12_union`,
`c4_ref_out_in`) y el README actualizado: **la matriz de 15 casos del roadmap queda completa**.

`examples/project_stage56_c2c4/` — ejemplo empaquetado que recorre C2, C3 y C4 en orden de roadmap,
con un código de salida propio por chequeo fallido.

`CMakeLists.txt` — registro del ejecutable con `NSC_SOURCE_DIR` (lo necesita para las fixtures, el
ejemplo y para invocar clang sobre el header generado).

## Paso 10 — Verificación final

1. Reconfiguración + build completo con `-DNSC_WARNINGS_AS_ERRORS=ON`: limpio.
2. `ctest` completo: **234/234**.
3. Manifest regenerado y verificado.
4. Extracción aislada del ZIP completo: configure + build + `ctest` desde cero.
