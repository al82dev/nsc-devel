# Tanda universal 42: índices de línea sin ABI genérica intermedia

## Causa confirmada

El diagnóstico agregado en la tanda 41 mostró que el compilador de primera generación pasaba la
auto-prueba, pero el compilador producido por él obtenía:

```text
SH04 line-column mismatch: line=4 column=220199317
```

`SourceBuffer.BuildLineStarts` construía primero un `List<int>` genérico y después copiaba sus
elementos a un `int[]`. Esa ruta atraviesa el ABI genérico bootstrap de punteros/valores antes de
escribir el arreglo físico. La investigación posterior de la tanda 43 precisó que el arreglo no era
la causa primaria: el backend emitía `Span<T>.CopyTo` con stride de puntero aun cuando el tipo cerrado
era `Span<byte>` o `Span<char>`. El arreglo directo eliminó una intermediación innecesaria y expuso
de forma reproducible ese defecto más profundo, que se corrige por separado.

## Cambio

`BuildLineStarts` usa ahora dos recorridos iterativos sobre el texto normalizado:

1. cuenta los saltos de línea y reserva exactamente un `int[]`;
2. escribe directamente en ese arreglo el offset inicial de cada línea.

Se elimina la colección genérica temporal, el boxing/unboxing bootstrap y una asignación dinámica.
La complejidad permanece O(n), el almacenamiento auxiliar baja de una lista creciente más arreglo a
un solo arreglo exacto y se conserva la línea vacía final cuando el texto termina en `\n`.

## Archivos

- Modificado: `selfhost/nsc/Source/SourceBuffer.ns`.
- Nuevo: este documento.

## Validación dirigida

1. Regenerar sólo SH41 en O0.
2. Comprobar que `nsc1.exe` y `nsc2-o0-nslink.exe` devuelvan `42` en
   `--self-test-basic`.
3. Continuar con `--check-project` y la comparación nsc2/nsc3 del mismo verificador.
