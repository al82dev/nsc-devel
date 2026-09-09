# 159 — NativeSharp 1.0.1: inicialización `get;` y comparación enum con cero

## Casos medidos en SaruCSHARP

El port conservaba dos lowering temporales después de cerrar los inicializadores enum:

```csharp
// GamePadTriggers.cs
public float Left { get; }
public GamePadTriggers(float left) { Left = left; }

// GamePadDPad.cs
return (buttons & button) != 0;
```

El primero terminaba en `Stage 38 property/indexer has no accessible setter`; el segundo, en
`comparison operands have incompatible Stage 34 types`.

## Auto-propiedad sólo-lectura

El parser ya crea un backing field privado para toda auto-propiedad concreta. La corrección no
sintetiza un setter: cuando el lado izquierdo de una asignación simple es una auto-propiedad
`get;`, el binder lo liga directamente a ese backing únicamente si la escritura ocurre dentro del
constructor del tipo declarante y el receptor es el `this` actual, implícito o explícito.

Una escritura desde un método ordinario o mediante otra instancia continúa produciendo el
diagnóstico de setter ausente. La superficie pública sigue teniendo sólo getter y el valor queda
inmutable después de construir el objeto.

## Literal cero frente a enum

Para `==` y `!=`, el literal entero `0` se admite como el valor cero del enum opuesto. El HIR
materializa una conversión al enum antes de comparar; esto evita depender de que su tipo subyacente
sea `int` y está comprobado también con un enum basado en `byte`.

La regla es deliberadamente estrecha: `enum == 1` continúa rechazado y dos enums distintos no se
vuelven compatibles.

## Evidencia

`Stage38PropertiesIndexersOperatorsTests` cubre inicialización implícita y `this.Prop`, además de
los dos rechazos fuera del constructor. `Stage51EnumOopSupportTests` cubre la forma real
`(buttons & button) != 0`, el orden inverso `0 == buttons`, un enum basado en `byte` y el rechazo de
un literal distinto de cero.

La fixture ejecutable `c54_constructor_get_only_enum_zero` reproduce los nombres y formas de
`GamePadTriggers` y `GamePadDPad`; debe devolver 42 tanto en O0 como en O2.

`NSC-SARU-004`, los inicializadores nombrados `new T { campo = valor }`, permanece fuera de este
cambio: requiere gramática, AST, semántica, orden de evaluación y cleanup propios.
