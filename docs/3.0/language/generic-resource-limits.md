# Generic resource limits / Límites de recursos genéricos

## Español

NativeSharp 3.0 limita la monomorfización para que una expansión genérica defectuosa termine con
un diagnóstico en vez de agotar la pila o la memoria del compilador. El perfil normativo usa:

- profundidad activa máxima: `64`;
- instancias nuevas máximas por compilación: `16384`;
- marcos mostrados en el backtrace causal: los `12` más recientes.

Una instancia ya presente en la caché no vuelve a consumir el presupuesto. Las especializaciones
precompiladas importadas tampoco consumen el presupuesto local. Sólo un cache miss aceptado aumenta
el contador; una entrada rechazada no queda publicada parcialmente.

El proyecto raíz puede declarar presupuestos explícitos:

```xml
<PropertyGroup>
  <GenericInstantiationDepth>96</GenericInstantiationDepth>
  <GenericInstantiationLimit>32768</GenericInstantiationLimit>
</PropertyGroup>
```

La CLI puede sobrescribirlos con `--generic-depth-limit=N` y
`--generic-instantiation-limit=N`. Los rangos admitidos son `1..4096` y `1..1048576`. Omitir las
opciones conserva 64/16384; por ello un programa que está dentro del perfil normativo no cambia por
esta característica. Reducir un límite es una decisión explícita del proyecto o de la invocación.

El diagnóstico distingue recursión de una identidad activa, exceso de profundidad y exceso de
cantidad. La cadena causal es estable y acotada. Para reducir trabajo, reutilice la misma identidad,
evite producir un tipo más anidado en cada paso y reemplace expansiones ramificadas por un algoritmo
iterativo o una representación no genérica interna.

## English

NativeSharp 3.0 bounds monomorphization so a defective generic expansion ends in a diagnostic
instead of exhausting compiler stack or memory. The normative profile permits an active depth of
`64`, `16384` new instances per compilation, and reports the newest `12` causal frames.

A cache hit does not consume the budget again. Imported precompiled specializations do not consume
the local budget either. Only an accepted cache miss increments the count, and a rejected entry is
never left partially published.

The root project may use `GenericInstantiationDepth` and `GenericInstantiationLimit`. Command-line
overrides are `--generic-depth-limit=N` (`1..4096`) and
`--generic-instantiation-limit=N` (`1..1048576`). Omitting them preserves 64/16384, so programs
within the normative profile keep their existing meaning. A lower limit is always explicit.

Diagnostics distinguish an active-identity cycle, depth exhaustion, and count exhaustion. The
causal chain is deterministic and compact. Prefer canonical instance reuse, stop creating a more
nested type at every step, and replace branching expansion with an iterative or non-generic
internal representation when a program reaches a budget.
