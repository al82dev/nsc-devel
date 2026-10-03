# Generic packs / Packs genéricos

## Español

NativeSharp 3 admite un pack final de tipos o valores const. El pack pertenece a la identidad de
la especialización y se materializa durante compilación; no añade arreglos, diccionarios ni estado
oculto al ABI de runtime.

```nativesharp
struct Types<pack Ts>
{
    public int Count() { return packcount(Ts); }
    public packtype<Ts, 0> First;
}

int Sum<pack const int Ns>() { return packsum(Ns); }
int Pick<pack const int Ns>() { return packat(Ns, 1); }
```

### Declaración y expansión

- `pack Ts` declara un pack de tipos.
- `pack const int Ns` declara un pack de valores const. Se admiten los mismos tipos const que en
  un parámetro const ordinario.
- El pack debe ser el último parámetro. Esta regla hace inequívoca la partición de argumentos y
  evita búsquedas combinatorias.
- `Ts...` o `Ns...` expande un pack únicamente en una lista de argumentos genéricos.
- Una forma compuesta como `Pair<Ts, Ts>...` se clona por posición. Todos los packs nombrados por
  una misma expansión deben tener la misma longitud.
- `F<>()` materializa explícitamente un pack vacío; `F()` continúa siendo una llamada con
  inferencia ordinaria.

### Índices y folds

- `packcount(P)` produce la longitud del pack.
- `packtype(Ts, I)` selecciona el tipo `I` y sólo es válido en contexto de tipo.
- `packat(Ns, I)` selecciona el valor const `I`.
- `packsum(Ns)` y `packproduct(Ns)` pliegan valores enteros con identidades `0` y `1`.
- `packall(Bs)` y `packany(Bs)` pliegan valores `bool` con identidades `true` y `false`.

Los índices deben ser enteros constantes no negativos y estar dentro del pack. Un fold exige un
pack de la categoría adecuada; no convierte tipos arbitrariamente.

### Terminación y errores

Cada expansión está limitada a 1024 elementos y comparte los límites generales de profundidad y
cantidad de especializaciones. Una expansión sin pack activo, una longitud zip incompatible, un
índice inválido o un fold de categoría incorrecta cancela la materialización y emite el error en
la instancia que lo causó. Una expansión residual nunca llega silenciosamente al backend.

## English

NativeSharp 3 supports one trailing type pack or const-value pack. Packs are compile-time parts of
specialization identity and add no runtime array, dictionary, or hidden ABI state. `pack Ts`
declares a type pack; `pack const int Ns` declares a const-value pack. The trailing-only rule makes
argument partitioning deterministic.

`Ts...` and `Ns...` expand only inside generic argument lists. A composite expansion such as
`Pair<Ts, Ts>...` is positionally zipped, and every pack named by one expansion must have the same
length. `packcount`, `packtype`, and `packat` provide length and checked indexing. `packsum`,
`packproduct`, `packall`, and `packany` are compile-time folds with the usual empty identities.

Indices must be non-negative compile-time integers. Category mismatches, unequal zip lengths,
residual expansions, and expansions beyond 1024 elements fail at the causal instantiation. Normal
specialization depth and count budgets also apply, so pack materialization is bounded.
