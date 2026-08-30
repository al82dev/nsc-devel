# Part 5 Conformance — Stage 51 (cierre)

## Enum como tipo de miembro OOP de primera clase

**PASS** — `enum Mode { First, Second, Third }` a nivel namespace/top-level registra sus
miembros vía la misma infraestructura de Stage 37 ("static const field"): acceso calificado,
igualdad, paso por parámetro y cast explícito `(int)` compilan, lowerean a LLVM (`constant i32`
global sin cctor en runtime) y emiten objeto COFF real.

**Límite conocido, no corregido esta ronda**: `enum` anidado dentro de una `class` no parsea
(`"error: expected an instance field or method declaration in class body"`). Todos los enums de
este stage (`Mode` de prueba, `StringComparison`, `StringSplitOptions`) se declaran a nivel
namespace/top-level, que es donde ya vivían en el paquete original.

## `StringComparison` real como parámetro de API pública

**PASS** — `Equals`, `IndexOf`, `Contains`, `StartsWith`, `EndsWith` tienen ahora un overload
real `(..., StringComparison comparison)` que delega al overload `bool ignoreCase` existente
comparando `comparison == StringComparison.OrdinalIgnoreCase`. Ya no es sólo el rodeo `bool`
descrito como limitación en la ronda anterior.

## Superficie de `NativeSharp.Core.String`

**PASS** — 90 overloads públicos / 69 familias, verificado programáticamente. Incluye
`Split`/`RSplit`/`SplitLines`/`Partition`/`RPartition` vía el patrón de buffer de destino
provisto por el caller (`string[]` como parámetro, no como retorno owned).

**Fuera de alcance, documentado explícitamente en el capítulo conceptual sección 7** (lista
mucho más corta que la de la ronda anterior): `Encode`/`Decode` de verdad (requiere tabla de
codificación, no sólo ASCII), `ToString` de `float`/`double` (requiere algoritmo de punto
flotante-a-decimal tipo Grisu/Ryu, no trivial), especificadores de formato dentro de `Format`
(`{0:D2}` etc.), soporte Unicode real más allá de ASCII (`char` en NativeSharp es de un byte).

## Ownership discipline sobre arrays

**PASS** — confirmado (no asumido) que `owned string[]` como retorno sigue siendo imposible:
`raii string[]` rechazado, `move()` sobre `owned string[]` no-raii rechazado, no existe
sintaxis de array-literal. El patrón nuevo verificado y usado en la superficie de Split es
asignar un valor `owned` de retorno directamente dentro de un elemento de un array recibido
como **parámetro** (referencia prestada) — eso sí es válido porque la propiedad se mueve hacia
una ranura prestada del caller, no hacia un array nuevo propio de la función.

**PASS** — confirmado que `delete[] arrayVar;` no destruye los elementos `owned` contenidos
(`Deallocation` con `dynamicDestruction = false`); el patrón correcto y documentado es
`delete arrayVar[i];` por cada elemento válido antes de `delete[] arrayVar;`.

## Regresión de la suite completa

**PASS con un gap honesto documentado** — 202/203. La única falla
(`nsc.stage50_10.ownership-examples-documentation::testPackagedGuidance`) es un gap de
contenido (README histórico que no existe en este árbol), no un bug de lógica del compilador;
no se fabricó contenido para forzarla a pasar.

## Host ABI / superficie previa

**PASS** — esta ronda no modifica la frontera C ABI cerrada en Stage 48 ni el mecanismo RAII/
move de Stage 49/50; el fix de enums extiende el pipeline OOP (member model → method body model
→ HIR → MIR → LLVM lowering) sin tocar la frontera de host.

## Scope boundary

Stage 51 congela: soporte de `enum` a nivel namespace/top-level con acceso calificado,
igualdad, parámetro y cast explícito; superficie de `NativeSharp.Core.String` con 69 familias/
90 overloads. `enum` anidado en clase y las cuatro áreas de la sección "fuera de alcance"
quedan explícitamente para un stage futuro.
