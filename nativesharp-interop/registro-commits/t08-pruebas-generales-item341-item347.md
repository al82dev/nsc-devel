# Pruebas generales fuera de la interoperabilidad C: `item341` e `item347`

Fecha: 2026-11-09. Indicación del usuario:

- `item341`: documentar el conflicto entre la prueba y la regla vigente, con el resultado esperado que se propone.
  La regla del lenguaje no se cambia.
- `item347`: registrar el desbordamiento de pila con una reproducción y su alcance.

No se modificó ni el compilador ni ninguna de las dos pruebas.

## `nsc.item341.exception-safe-com-thunk`

### Qué prueba

El fixture convierte un método que lanza en un `extern C delegate`, lo guarda en una vtable COM y lo llama desde
allí. Comprueba:

1. el IR del thunk de frontera, con `boundary.catchswitch` y las catchpads de excepción y de panic;
2. que una excepción que escapa termina con el trap 13 (`CallbackExceptionEscape`), código de salida 141, en O0 y O2.

### Conflicto con las reglas vigentes

| Regla | Origen | Efecto en el fixture |
|---|---|---|
| `throw` chequeado: un `throw` debe atraparse o declararse en el `throws` del callable | congelamiento 3.0 C-13 (`8d46bf5cc`, `73b03a394`, 2026-10-25) | `73b03a394` agregó `throws Boom` a `Throws(void*, int)` |
| Un grupo de métodos se convierte en delegate solo si su tipo de error coincide con el del delegate | `45258d046` (2026-10-19), `compatibleDelegateMethods(…, delegateErrorTypeKey(…))` | `ComMethod` no declara error, así que `Throws` deja de ser compatible: «no method in the method group exactly matches delegate signature» |
| `panic()` usa el perfil abort | NativeSharp 3.0 T07 | un panic termina el proceso con el trap 14 (142) y no llega a la frontera |

Sondas en `claude\sonda-t09\com341`:

- **`a`:** el mismo método sin `throws` se convierte y compila. El filtro de calificadores de T08-002 no interviene.
- **`b`:** con `throws Boom`, se rechaza con el mensaje de arriba.
- **`panic`:** un callback que hace `panic("boom")` termina con 142 en O0 y O2, no con 141.

**Conclusión.** Con las reglas vigentes, ningún código fuente NativeSharp puede hacer llegar una excepción a la
frontera C:

- una excepción chequeada no se convierte en callback C;
- un panic aborta antes.

El segundo punto de la prueba (salida 141) no es alcanzable. La catchpad de excepción del thunk queda como defensa
ante un invariante roto, no como comportamiento observable.

### Resultado esperado que se propone (sin aplicarlo)

1. Convertir a un `extern C delegate` un método que declara `throws` se rechaza con el diagnóstico actual. Se
   comprueba el rechazo.
2. Un método sin `throws` convertido en callback sigue generando el thunk con `boundary.catchswitch` y las dos
   catchpads. Se conserva la comprobación de IR.
3. Un `panic()` dentro de ese callback termina con 142 en O0 y O2 (perfil abort).

Así la prueba documentaría las reglas vigentes sin cambiar ninguna. La otra salida es una regla nueva: permitir
que un `extern C delegate` acepte métodos con `throws` y convertir la excepción en el trap 13. Eso cambia el
lenguaje y decide el usuario.

## `nsc.item347.integer-vector-128`

### Reproducción

`nsc.exe` termina con `0xC00000FD` (desbordamiento de pila) sin mensaje. `claude\sonda-t09\v128`, proyecto con
`ProjectReference` a `lib\NativeSharp.Core`, O0, `--no-artifact-cache`:

```
using NativeSharp.Core.Numerics;
public class Program
{
    public static int Main()
    {
        Int32x4 left;
        left.X = 10; left.Y = 2; left.Z = 3; left.W = 4;
        Int32x4 sum = Int32x4.Add(left, left);
        return sum.X;
    }
}
```

### Alcance medido

| Llamada | Resultado |
|---|---|
| `Int32x4.Add`, `Int32x4.Subtract` | desbordamiento de pila |
| `Int64x2.Add`, `Int64x2.Subtract` | desbordamiento de pila |
| `Int32x4.Multiply`, `Int64x2.Multiply` | compila |
| copia de un `Int32x4` sin llamadas | compila |
| `copy struct` propio con la misma forma y un `Add` no intrínseco | compila |
| proyecto sin vectores con la misma referencia a Core | compila |
| `item348` y `item349` (vectores de 256 bits) | pasan |

El fallo está en la sustitución intrínseca de `Add`/`Subtract` de los vectores enteros de 128 bits. No está en la
compilación de NativeSharp.Core, ni en la copia de estos structs, ni en `Multiply`.

### No es una regresión de la tanda 8

El compilador construido en `97911ad25` (`interop-t07-close`, el commit anterior al primero de la tanda 8) falla
igual con la misma reproducción (`claude\scripts\bisect-nsc.cmd`). La reproducción compila las fuentes de
`lib\NativeSharp.Core` del árbol actual. Lo que cambia es el compilador; las fuentes de Core no las tocó esta tanda. Pertenece a la ruta SIMD, fuera de la
interoperabilidad C, y queda sin corregir.
