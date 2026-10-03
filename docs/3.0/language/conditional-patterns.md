# Conditional patterns / Patrones condicionales

## Español

### Formas admitidas

NativeSharp 3.0 permite usar el mismo lenguaje de patterns de `match` en condiciones y en una
inicialización que debe continuar con éxito:

```nativesharp
if (let Some(var value) = result)
    Use(value);

while (let Some(var item) = Next())
{
    Consume(item);
}

let Some(var required) = result else { return -1; };
Use(required);
```

Las tres formas aceptan wildcard, literales, rangos enteros, tuples, structs, variants y OR
patterns sin bindings. Los modos `var`, `ref` y `move` conservan las mismas reglas que en
`match`: un `ref` no puede atravesar una copia de tuple o payload, y un OR pattern todavía no
puede fusionar bindings entre alternativas.

### Evaluación y alcance

La expresión a la derecha de `=` se evalúa exactamente una vez. El compilador la conserva en un
temporal interno y ejecuta el mismo árbol de decisiones usado por `match`; la extracción del
payload sólo ocurre después de comprobar el tag correspondiente.

En `if let`, los nombres existen únicamente en la rama verdadera. En `while let`, se crean de
nuevo en cada iteración y dejan de existir al salir del cuerpo. La rama `else` de un `if let` no
puede observarlos. Los bloques explícitos y las sentencias embebidas sin llaves respetan las
mismas fronteras.

En `let ... else`, los bindings pertenecen al bloque que contiene la sentencia y quedan
inicializados para todo el camino que continúa. Por esa razón el bloque `else` es obligatorio y
debe terminar el flujo mediante `return`, `throw`, `break`, `continue` o un `if` cuyas dos ramas
terminen. Un `else` que pueda regresar a la siguiente sentencia es un error.

### Lowering

Las formas son azúcar de frontend, no objetos runtime. El HIR y el MIR reciben declaraciones,
condiciones y llamadas de extracción ordinarias. `if let` y `match` comparten literalmente el
parser y el lowering de patterns, así que sus pruebas de tag, rangos y payload no forman dos
implementaciones independientes.

La divergencia basada en una expresión de tipo `never` se incorpora con T07; F04 reconoce las
formas estructurales de control disponibles antes de ese ítem.

## English

### Accepted forms

NativeSharp 3.0 accepts the `match` pattern language in `if let`, `while let`, and `let ... else`.
The right-hand expression is evaluated exactly once and variant payloads are extracted only after
their tag test succeeds.

`if let` bindings exist only in the success branch. `while let` creates fresh bindings for each
iteration. `let ... else` publishes its bindings to the containing block after the failure branch
has been proven divergent.

The `else` block is mandatory for `let ... else` and must end control flow with `return`, `throw`,
`break`, `continue`, or an `if` whose two branches both diverge. A fall-through failure branch is
rejected because it would expose uninitialized bindings.

### Lowering contract

Conditional patterns are frontend sugar with no runtime object, allocation, boxing, or linker
protocol. They share the actual pattern parser and decision-tree lowerer with statement `match`.
O0 MIR therefore uses the same case-test and payload-extraction primitives for both spellings.

OR-pattern binding joins and `never`-typed expression divergence remain owned by their dedicated
T03-F05 and T07 work; the currently accepted forms do not claim those later contracts.
