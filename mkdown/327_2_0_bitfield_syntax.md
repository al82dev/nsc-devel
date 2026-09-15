# 327 — Sintaxis de bitfields / Bit-field syntax

## Resultado / Outcome

NativeSharp reconoce bitfields nombrados, sin nombre y de ancho cero:

```nativesharp
public struct Control
{
    public uint Enabled : 1;
    public volatile uint Mode : (1 + 2);
    public uint : 4;
    public uint : 0;
}
```

La expresión de ancho se conserva completa en una arista AST distinta del inicializador de field.
Las formas sin nombre reciben una identidad interna determinista que no es accesible desde fuente.

NativeSharp recognizes named, unnamed and zero-width bit-field declarations. The complete width
expression survives on a dedicated AST edge; unnamed declarations receive an unreachable,
deterministic internal identity.

## Frontera deliberada / Deliberate boundary

El punto 327 no decide aún unidad de almacenamiento, offset de bit, máscara, signo ni interacción
con `pack`/unions. `ProjectMemberModel` conserva `isBitField`, `isUnnamedBitField` y el nodo de ancho;
`ProjectObjectLayoutModel` rechaza explícitamente el layout hasta el punto 328. Así una compilación
no puede tratar accidentalmente `uint Flag : 1` como cuatro bytes ordinarios.

Se aceptan accesibilidad y `volatile` en la sintaxis. La semántica física `volatile` pertenece al
punto 330. Inicializadores y modificadores de storage, callable o layout de bytes se diagnostican.
No cambian MIR 1.6, NSMETA 1.17, Object ABI 2, Runtime ABI, headers, runtime, bindings ni tutoriales.

## Evidencia / Evidence

`Item327BitFieldSyntaxTests.cpp` verifica 17 condiciones:

- AST de bitfields nombrados y anchos expresados con operaciones;
- formas sin nombre y de ancho cero con identidad determinista;
- conservación en el member model sin afectar al field ordinario siguiente;
- diagnóstico físico exacto de la frontera del punto 328;
- rechazo de `static`, `readonly`, alineación explícita e inicializador.

## Mensaje de commit / Commit message

`Añade sintaxis de bitfields / Add bit-field syntax`
