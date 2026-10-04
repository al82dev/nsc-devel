# Exhaustive match / Match exhaustivo

## Español

### Dominios cerrados

Un `variant` define su dominio con el catálogo nominal de casos. El chequeo no deduce ese dominio
de nombres de métodos: usa la declaración 3.0 o la declaración reconstruida desde NSMETA 1.27.
Por ello un consumidor precompilado recibe la misma decisión que el proyecto productor.

```nativesharp
variant Result { Ok(int), Error(int) }

match (result)
{
    Ok(var value) => { Use(value); },
    Error(var code) => { Report(code); }
}
```

No se necesita `_`: ambos casos están cubiertos por patterns irrefutables sobre su payload. En
cambio `Ok(42)` sólo cubre un subconjunto de `Ok`; no puede cerrar el caso completo.

### Utilidad y orden

Los brazos se examinan en orden textual. Un brazo es inalcanzable cuando un wildcard previo ya
cubrió el dominio, cuando un caso ya fue cubierto por un pattern irrefutable o cuando un wildcard
aparece después de cubrir todos los casos cerrados. Los diagnósticos usan el índice estable del
brazo y enumeran los casos faltantes en el orden del catálogo.

Los dominios abiertos —enteros, strings, structs y tipos sin catálogo variant— todavía exigen un
brazo `_` o binding total. Rangos y literales no pretenden enumerar un entero completo.

### Lowering

El statement `match` conserva evaluación única de la expresión gobernante y baja a decisiones
ordinarias, sin objeto runtime, heap, boxing ni dependencia de linker. Las condiciones short-circuit
protegen la extracción de payload; los brazos posteriores quedan guardados tras seleccionar uno.

La forma expresión produce un valor sin asignación implícita ni objeto runtime:

```nativesharp
long code = match (result)
{
    Ok(var value) => value,
    Error(var error) => (long)error.Code
};
```

El governing se evalúa exactamente una vez. Los brazos `never` no fuerzan un tipo; los demás
deben compartir tipo, tener un tipo numérico común o una conversión implícita común. Ownership se
conserva sólo si todos los brazos coinciden. `=> ref storage` crea un match por referencia: todos
los brazos deben usar `ref`, nombrar storage mutable/addressable y tener identidad de tipo exacta.

Cada brazo tiene scope y cleanup propios. Los bindings se crean sólo después del pattern; si el
guard falla se destruyen antes de continuar, y si el brazo gana se destruyen después de extraer o
mover su resultado. Un variant cerrado compuesto sólo por casos completos sin guard usa una carga
del tag y `switch`; cualquier forma parcial conserva branches ordenados. Ambos caminos son nativos,
sin heap, boxing ni dependencia del linker.

## English

### Closed domains

A `variant` defines its domain through the nominal case catalogue. The checker consumes either the
edition-3 declaration or the declaration reconstructed from NSMETA 1.27; it does not guess from
method names. A precompiled consumer therefore receives the same exhaustiveness decision as its
producer.

An irrefutable payload pattern such as `Ok(var value)` covers the whole `Ok` case. A restrictive
pattern such as `Ok(42)` does not. Closed variants may omit a wildcard after every case is covered;
open domains still require `_` or a total binding arm.

### Usefulness and order

Arms are considered in textual order. An arm is unreachable after a wildcard, after an
irrefutable pattern already covers the same case, or when a wildcard follows complete closed-domain
coverage. Diagnostics use stable arm indices and list missing cases in catalogue order.

### Lowering

Statement match evaluates its governing expression once and lowers to ordinary guarded decisions,
with no runtime object, heap allocation, boxing, or linker dependency. Short-circuit conditions
protect payload extraction.

Expression match has dedicated AST, HIR, and MIR 1.26 nodes. The governing expression is evaluated
once. Reachable arms must have one exact/common numeric/implicitly compatible result type; bottom
arms do not constrain the join. Ownership survives only when every arm agrees. `ref` results require
every arm to yield mutable addressable storage of the same type, and their escape origins are joined.

Arm bindings have lexical cleanup on both guard failure and selected-result exit. Complete,
unguarded cases over a closed variant load the tag once and use an LLVM switch; partial patterns,
guards, and open domains retain ordered branches. Neither path allocates a runtime match object.
