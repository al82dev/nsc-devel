# NS3-T13-F05 — Auditable unsafe operations

## Español

NativeSharp 3 hace visible cada frontera que puede romper las garantías del lenguaje. El
frontend mantiene un contexto `unsafe` léxico y rechaza las operaciones del catálogo fuera de
él:

- desreferenciar un puntero raw (`*pointer`, incluido el acceso `pointer->member`);
- convertir entre entero y puntero, porque el entero no conserva provenance;
- leer un miembro inactivo de una unión C;
- invocar una función importada por FFI;
- ejecutar ensamblador inline;
- llamar un método declarado `unsafe` o un valor `fn* ... unsafe`.

Una declaración de método coloca `unsafe` inmediatamente antes del tipo de retorno:

```nativesharp
public static unsafe int Read(int* pointer) { ... }
```

La marca expresa un requisito del caller; no vuelve implícitamente inseguro todo el cuerpo. Las
operaciones peligrosas del cuerpo siguen necesitando un bloque local, lo que mantiene pequeña la
superficie auditada:

```nativesharp
unsafe("pointer validated by the caller") { return *pointer; }
```

La razón es opcional. Un bloque sin razón que cubre más de ocho sentencias produce el warning
`NS3030MissingEffectContext`; se puede corregir reduciendo el bloque o documentando la invariante
con `unsafe("reason")`.

Los traits pueden declarar un contrato de implementación peligroso mediante `unsafe trait`.
Implementarlo exige `unsafe impl`; a la inversa, un trait seguro rechaza `unsafe impl`. Esta regla
hace explícito quién revisó las invariantes que el trait no puede comprobar.

El efecto unsafe se conserva en clones genéricos, tipos función estructurales y el conjunto de
efectos semántico. No cambia el ABI físico, pero sí la asignabilidad y la legalidad de cada call
site.

## English

NativeSharp 3 makes every boundary that can violate language guarantees visible. The frontend
tracks a lexical unsafe context and rejects catalogued operations outside it:

- dereferencing a raw pointer (`*pointer`, including `pointer->member`);
- converting between integers and pointers, because an integer does not retain provenance;
- reading an inactive C-union member;
- invoking an imported FFI function;
- executing inline assembly;
- calling an `unsafe` method or an `fn* ... unsafe` value.

A method declaration places `unsafe` immediately before the result type:

```nativesharp
public static unsafe int Read(int* pointer) { ... }
```

The marker is a caller requirement; it does not implicitly make the whole body unsafe. Dangerous
operations in the body still need a local block, keeping the audited surface small:

```nativesharp
unsafe("pointer validated by the caller") { return *pointer; }
```

The reason is optional. An unexplained block spanning more than eight statements emits
`NS3030MissingEffectContext`; narrow the block or document its invariant with
`unsafe("reason")`.

Traits may declare a dangerous implementation contract with `unsafe trait`. Implementing it
requires `unsafe impl`; conversely, a safe trait rejects `unsafe impl`. The rule makes explicit
who accepted invariants the trait cannot verify.

The unsafe effect survives generic cloning, structural function types, and the semantic effect
set. It does not change the physical ABI, but it does affect assignability and call-site legality.
