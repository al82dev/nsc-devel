# Operadores mediante traits / Trait-based operators

## Contrato / Contract

ES: En NativeSharp 3.0 cada operador sobrecargable tiene una identidad `lang:*` estable. La
escritura fuente selecciona esa identidad por operador y aridad; la identidad selecciona un
requisito estático de trait y su witness concreto. El nombre ABI `op_*` continúa siendo un detalle
compatible de símbolo, no la identidad semántica.

EN: In NativeSharp 3.0 every overloadable operator has a stable `lang:*` identity. Source syntax
selects that identity by spelling and arity; the identity selects a static trait requirement and
its concrete witness. The ABI `op_*` name remains a compatible symbol detail, not the semantic
identity.

```nativesharp
public trait IAdd<T>
{
    public static T operator +(in T left, in T right);
}

public struct Number : IAdd<Number>
{
    public int Value;

    public static forceinline Number operator +(in Number left, in Number right)
    {
        return new Number(left.Value + right.Value);
    }
}
```

## Tabla normativa / Normative table

| Fuente / Source | Aridad | Lang item | Nombre ABI / ABI name |
|---|---:|---|---|
| `+` | 1 | `lang:UnaryPlus` | `op_UnaryPlus` |
| `-` | 1 | `lang:Negate` | `op_UnaryNegation` |
| `!` | 1 | `lang:LogicalNot` | `op_LogicalNot` |
| `~` | 1 | `lang:OnesComplement` | `op_OnesComplement` |
| `+` | 2 | `lang:Add` | `op_Addition` |
| `-` | 2 | `lang:Subtract` | `op_Subtraction` |
| `*` | 2 | `lang:Multiply` | `op_Multiply` |
| `/` | 2 | `lang:Divide` | `op_Division` |
| `%` | 2 | `lang:Remainder` | `op_Modulus` |
| `&` | 2 | `lang:BitwiseAnd` | `op_BitwiseAnd` |
| `\|` | 2 | `lang:BitwiseOr` | `op_BitwiseOr` |
| `^` | 2 | `lang:ExclusiveOr` | `op_ExclusiveOr` |
| `<<` | 2 | `lang:LeftShift` | `op_LeftShift` |
| `>>` | 2 | `lang:RightShift` | `op_RightShift` |
| `==` | 2 | `lang:Equality` | `op_Equality` |
| `!=` | 2 | `lang:Inequality` | `op_Inequality` |
| `<` | 2 | `lang:LessThan` | `op_LessThan` |
| `<=` | 2 | `lang:LessThanOrEqual` | `op_LessThanOrEqual` |
| `>` | 2 | `lang:GreaterThan` | `op_GreaterThan` |
| `>=` | 2 | `lang:GreaterThanOrEqual` | `op_GreaterThanOrEqual` |

## Selección y built-ins / Selection and built-ins

- Las reglas intrínsecas de strings, referencias, punteros, enums y promoción numérica conservan
  su semántica. La tabla no convierte built-ins en llamadas ni cambia consteval.
- Para un tipo usuario, coherence registra el requisito estático y el miembro que lo satisface. La
  resolución reúne witnesses de ambos operandos, elimina duplicados y aplica el overload resolver
  ordinario. Dos candidatos igualmente buenos producen ambigüedad explícita.
- Los parámetros por valor aceptan conversiones implícitas normales. `in` y `ref` son referencias
  implícitas en la sintaxis del operador: no se escribe la palabra en la expresión; requieren
  conversión de identidad y almacenamiento direccionable. `ref` exige además mutabilidad. `out`
  no es válido para un operador.
- El resultado de un operador por valor es un valor, no una ubicación direccionable oculta.
- `inline`, `forceinline` y `noinline` conservan la misma política de una función estática. Un
  witness `forceinline` se integra incluso en O0 mediante la fase obligatoria de inlining.

Builtin string, reference, pointer, enum, and numeric-promotion rules remain intrinsic and retain
their constant-evaluation behavior. User operands select coherent static witnesses from both
sides; ordinary overload ranking decides, and an equal rank is an error. Value parameters use
normal implicit conversions. Operator syntax passes `in`/`ref` implicitly, but only from identity-
typed addressable storage; `ref` also requires writable storage and `out` is forbidden.

## Cortocircuito / Short circuit

`&&`, `||` y `??` son control de flujo intrínseco: el operando derecho puede no evaluarse. No
pueden declararse como operadores porque una llamada estática ordinaria volvería eager ambos
operandos. Un protocolo futuro de branching tendría que diseñar y versionar esa semántica antes de
habilitar cualquier sobrecarga.

`&&`, `||`, and `??` are intrinsic control flow: the right operand may not execute. They cannot be
declared as ordinary operators because a static call would eagerly evaluate both operands. Any
future branching protocol must define and version that behavior first.

## Módulos y metadata / Modules and metadata

F07 no incrementa NSMETA. El formato 1.32 ya transporta declaraciones públicas de trait,
requisitos estáticos y operadores con su nombre/forma ABI. El consumidor deriva la identidad
`lang:*` de la tabla versionada por nombre y aridad; serializar una cuarta copia habría permitido
divergencia. Cambiar esta tabla es un cambio de lenguaje/ABI y exige versionado deliberado.

F07 does not bump NSMETA. Format 1.32 already transports public traits, static requirements, and
operator declarations with their ABI shape. Consumers derive the `lang:*` identity from the
versioned name-and-arity table; serializing a fourth copy would allow divergence. Changing this
table is a language/ABI change and requires deliberate versioning.
