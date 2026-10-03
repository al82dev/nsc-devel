# NativeSharp 3.0 — Lifetimes nombrados e inferidos / Named and inferred lifetimes

## Contrato normativo / Normative contract

Los lifetimes de NativeSharp 3.0 describen, en tiempo de compilación, qué storage puede respaldar
una referencia retornada o almacenada en una vista prestada. No añaden palabras, campos, registros
de runtime ni parámetros ocultos al ABI Win64. La sintaxis está disponible sólo con
`<Language>3</Language>`.

NativeSharp 3.0 lifetimes describe, at compile time, which storage may back a returned reference or
a borrowed view. They add no words, fields, runtime records, or hidden parameters to the Win64 ABI.
The syntax is available only under `<Language>3</Language>`.

## Sintaxis / Syntax

```nativesharp
public ref[a] int Identity<lifetime a>(ref[a] int value)
{
    return ref value;
}

public ref[shorter] int Narrow<lifetime longer, lifetime shorter>(ref[longer] int value)
    where longer : shorter
{
    return ref value;
}

public borrowed struct View<lifetime a>
{
    public borrowed[a] string Source;
}
```

- `<lifetime a>` declara un nombre borrado en un tipo, delegate, método o función de namespace.
- `ref[a]` e `in[a]` ligan un parámetro prestado al nombre; `out[a]` es inválido porque `out`
  inicializa storage y no recibe un préstamo previo.
- `ref[a] T` liga el resultado por referencia. `borrowed[a] T` liga un tipo/campo prestado.
- `where longer : shorter` declara que `longer` sobrevive al menos tanto como `shorter`.
- `this` y `static` son lifetimes reservados para el receiver de instancia y storage estático.
- Los nombres deben estar declarados, no pueden repetirse y los ciclos outlives se rechazan.

- `<lifetime a>` declares an erased name on a type, delegate, method, or namespace function.
- `ref[a]` and `in[a]` bind a borrowed parameter; `out[a]` is invalid because `out` initializes
  storage rather than receiving an existing borrow.
- `ref[a] T` binds a by-reference result. `borrowed[a] T` binds a borrowed type or field.
- `where longer : shorter` states that `longer` outlives `shorter`.
- `this` and `static` are reserved lifetimes for the instance receiver and static storage.
- Names must be declared and unique; cyclic outlives graphs are rejected.

## Elisión / Elision

Cada parámetro `ref` o `in` sin anotación recibe internamente un lifetime independiente. Un retorno
`ref` sin anotación se resuelve en este orden:

1. en un miembro de instancia, se liga a `this`;
2. en un callable estático con exactamente un lifetime de entrada, se liga a esa entrada;
3. sin entradas prestadas, se liga a `static`;
4. con varias entradas distintas, la declaración es ambigua y exige `ref[a]` explícito.

Each unannotated `ref` or `in` parameter receives an independent internal lifetime. An unannotated
`ref` result is resolved in this order: an instance member binds to `this`; a static callable with
one input lifetime binds to that input; a callable with no borrowed inputs binds to `static`; and
multiple distinct input lifetimes require an explicit `ref[a]` result.

La inferencia nunca convierte dos entradas independientes en el mismo lifetime y nunca usa la
última lectura como final implícito del préstamo. La duración de los borrows locales sigue siendo
léxica conforme a F03.

Inference never merges independent inputs and never treats the last read as an implicit borrow
end. Local borrows remain lexical under the F03 contract.

## Subtyping outlives / Outlives subtyping

Un resultado ligado a `shorter` puede proceder de un parámetro ligado a `longer` únicamente cuando
existe una ruta transitiva `longer : shorter`. El solver normaliza el grafo, admite transitividad y
falla si la fuente no existe o el grafo contiene un ciclo. La relación sólo restringe procedencia;
no cambia mangling, representación ni calling convention.

A result bound to `shorter` may originate from a parameter bound to `longer` only when a transitive
`longer : shorter` path exists. The solver follows transitive edges and fails when no source exists
or the graph is cyclic. The relation constrains provenance only; it changes neither mangling,
representation, nor calling convention.

## Traits, delegates y closures / Traits, delegates, and closures

Los contratos de traits comparan binders por posición, no por ortografía: una implementación
`<lifetime b>` satisface una exigencia equivalente `<lifetime a>`. Éste es el HRTB limitado de F04:
el binder de cada método es universal y admite renombrado alfa; tipos función higher-ranked de
primera clase pertenecen a T08.

Trait contracts compare binders by position rather than spelling: an implementation using
`<lifetime b>` satisfies an equivalent `<lifetime a>` requirement. This is F04's limited HRTB: each
method binder is universal and alpha-renamable; first-class higher-ranked function types belong to
T08.

Los delegates conservan binders, pass kinds y bindings de parámetros/resultado. Una closure con
entorno almacenable no puede capturar un local ref ni un parámetro `ref`/`in`, aunque tenga nombre
de lifetime: el delegate podría sobrevivir al storage. Las políticas de captura explícita y
callables prestados se completan en T08, sin debilitar esta prohibición.

Delegates preserve binders, pass kinds, and parameter/result bindings. A storable closure cannot
capture a reference local or a `ref`/`in` parameter even when it carries a named lifetime: the
delegate may outlive the storage. Explicit capture policies and borrowed callable types are
completed by T08 without weakening this rejection.

## Módulos y NSMETA / Modules and NSMETA

NSMETA 1.34 publica parámetros de lifetime, aristas outlives, bindings de campos, parámetros y
retornos, además de contratos de delegates. El materializador reconstruye la declaración pública
antes del análisis del consumidor. Un modelo que contiene estos datos no puede etiquetarse como
1.33 o anterior. Los lifetimes forman parte de compatibilidad de fuente, no del ABI físico.

NSMETA 1.34 publishes lifetime parameters, outlives edges, field/parameter/result bindings, and
delegate contracts. The source materializer reconstructs the public declaration before consumer
analysis. A model carrying these data cannot be labeled 1.33 or older. Lifetimes affect source
compatibility, not the physical ABI.

## Diagnósticos mínimos / Required diagnostics

- lifetime no declarado o duplicado;
- `out[a]` o anotación sobre parámetro por valor;
- retorno ambiguo con varias entradas elididas;
- resultado sin parámetro fuente que lo sobreviva;
- ciclo en constraints outlives;
- retorno de storage local como lifetime del caller;
- captura de referencia local o parámetro `ref`/`in` por una closure almacenable;
- sintaxis de lifetime usada fuera de edición 3.

