# Controlled specialization / Especialización controlada

## Español

NativeSharp 3 permite que una familia de métodos genéricos declare una implementación más
específica mediante el modificador contextual `specialize`:

```nativesharp
class Selector
{
    public static int Choose<T>(T value) { return 1; }

    public specialize static int Choose<T>(T value)
        where T : unmanaged
    {
        return 42;
    }
}
```

`specialize` es un opt-in semántico, no una sugerencia de optimización. Cuando el argumento
satisface ambas implementaciones, gana la que lleva el conjunto de constraints estrictamente más
fuerte. Si la implementación específica no es viable, se usa el fallback.

### Reglas de coherencia

Dos métodos forman una familia únicamente cuando coinciden su propietario, nombre, aridad
genérica, forma static/instance y patrón alfa-normalizado de parámetros. El nombre elegido para los
parámetros genéricos y el tipo de retorno no separan implementaciones.

La versión inicial es conservadora y sólo demuestra orden por inclusión estricta de átomos
semánticos normalizados. Por ejemplo, `where T : unmanaged, copy` domina a
`where T : unmanaged`; dos conjuntos iguales son equivalentes y no forman una especialización.
El compilador no supone implicaciones no escritas como “unmanaged implica copy”. Esta regla evita
que una ampliación futura del solver cambie silenciosamente qué cuerpo se ejecuta.

El programa se rechaza antes de analizar llamadas cuando ocurre cualquiera de estos casos:

- una implementación más restringida no lleva `specialize`;
- un `specialize` no domina a ningún fallback;
- dos implementaciones tienen constraints equivalentes;
- dos ramas son incomparables, incluido un diamante de constraints;
- `specialize` modifica un campo, propiedad, operador, constructor u otra declaración no válida.

Las conversiones ordinarias, los argumentos predeterminados, variádicos y packs se comparan antes
que los constraints. La especialización sólo rompe un empate entre candidatos con la misma firma
productiva; no reemplaza la resolución normal de overloads.

### Módulos y estabilidad

Las plantillas públicas transportadas por NSMETA conservan el modificador y todas sus cláusulas.
Productor y consumidor reconstruyen la misma familia y aplican el mismo orden parcial. Agregar
código al proyecto importador no cambia la elección: una implementación especializada pertenece
al mismo tipo y módulo que su fallback, y cada cuerpo obtiene una identidad física distinta.

El perfil inicial se limita a métodos genéricos ordinarios no virtuales de clases y structs. Las
funciones libres genéricas todavía no admiten constraints productivos y por eso no son objetivos de
`specialize`; los métodos virtuales, abstractos, override y sealed también se rechazan. Esta
restricción evita prometer coherencia abierta o interacción con dispatch que la versión 3.0 aún no
modela.

## English

NativeSharp 3 supports controlled specialization of ordinary generic methods through the
contextual `specialize` modifier. It is a semantic opt-in, not an optimization hint. When both a
fallback and a specialized implementation are viable, the implementation with the strictly
stronger constraint set wins; otherwise the fallback remains available.

Methods share a specialization family only when owner, name, generic arity, static/instance form,
and alpha-normalized parameter pattern agree. The initial conservative proof is strict inclusion
of normalized semantic constraint atoms. Equivalent sets and incomparable branches are rejected,
and the compiler deliberately does not infer unstated implications between capabilities.

A stronger implementation must carry `specialize`, every specialized implementation must dominate
a fallback, and diamond overlaps are diagnosed before any call is required. Ordinary conversion,
default-argument, variadic, and pack ranking still precedes the constraint tie-break.

Public template source transported by NSMETA preserves the modifier and clauses, so a separate
consumer build reconstructs the same order and selection. Specialized bodies use distinct physical
symbols while the fallback retains its existing symbol identity.

The initial profile covers concrete, non-virtual generic methods on classes and structs. Free
generic functions do not yet have productive generic constraints and are therefore outside this
feature; virtual, abstract, override, and sealed methods are rejected as specialization targets.
