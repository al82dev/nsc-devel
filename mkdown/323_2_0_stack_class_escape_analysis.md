# 323 — Escape analysis de clases hacia stack / Stack allocation of nonescaping classes

## Resultado / Outcome

NativeSharp puede bajar al frame una construcción de clase ordinaria cuando el objeto pertenece a
un local `raii` y el HIR completo demuestra que su identidad no sale del local:

```nativesharp
public class Cell
{
    public int Left;
    public int Right;
}

public static noheap int Work()
{
    raii Cell cell = new Cell();
    cell.Left = 40;
    cell.Right += 2;
    return cell.Left + cell.Right;
}
```

Este ejemplo no llama a `__nativesharp_alloc`. LLVM reserva una vez el layout completo en el bloque
de entrada, instala el descriptor y conserva las lecturas/escrituras ordinarias de fields. El slot
propietario `raii` sigue garantizando cleanup exactamente una vez, pero entra por la destrucción
object-only del descriptor: termina el lifetime del objeto sin intentar liberar el stack.

NativeSharp can lower an ordinary class construction into the current frame when a `raii` local's
complete HIR proves that its object identity cannot leave the local. LLVM allocates the canonical
layout once in the entry block, initializes the descriptor and fields, and performs object-only
cleanup without calling or freeing through the heap allocator.

## Prueba conservadora / Conservative proof

La superficie inicial queda cerrada intencionalmente. Se exige:

- clase local al proyecto, sin base ni metadata precompilada;
- construcción directa, sin argumentos, mediante default implícito sin entry de constructor;
- ningún inicializador de instancia sintetizado;
- ningún destructor explícito ni field de instancia `owned`;
- layout canónico no vacío y de hasta 1 MiB;
- local `raii` con lifetime y storage originalmente `owned/owned`;
- usos limitados a copiar el valor de un field o escribir directamente la dirección de un field.

Comparar la referencia —incluido contra `null`—, invocar un método sobre ella, pasarla, prestarla,
tomar su dirección, moverla, capturarla, retornarla o reasignarla cancela la optimización. El
programa ordinario sigue siendo válido y conserva su heap; dentro de `noheap`, esa construcción no
promovida recibe el mismo diagnóstico directo fijado en el punto 321.

The initial proof accepts only a project-local root class with trivial implicit construction,
known layout of at most one mebibyte, no initializer/destructor/owned-field work, and field-only
loads or direct stores. Any identity-observing or address-propagating use declines promotion. A
normal program remains heap-backed; a `noheap` program retains the established allocation error.

## Contratos congelados / Frozen contracts

- HIR marca declaración y `ObjectConstruction` con `stack-promoted=true`.
- El lifetime permanece `object-ownership=owned`; el storage pasa a
  `storage-ownership=none`, porque pertenece al frame y no al objeto.
- MIR sube de schema 1.5 a 1.6 y transporta la prueba y el ordinal del local destino.
- El backend crea un buffer fijo por local en el bloque de entrada, con tamaño/alineación del
  modelo de layout; por tanto un local dentro de un loop no acumula `alloca` dinámicos.
- La construcción promovida es no nula y no conserva una rama de fallo del asignador. Los programas
  que pueden observar nulidad o identidad quedan fuera del subconjunto y siguen en heap.
- RAII usa la entrada 7 object-only, no la entrada 5 que destruye y libera.
- No cambian parser, sintaxis, NSMETA 1.16, layout de clase, Object ABI 2, Runtime ABI, runtime,
  bindings ni tutoriales.

## Evidencia / Evidence

`Item323StackClassEscapeAnalysisTests.cpp` verifica en O0:

- aceptación del caso promovido dentro de `noheap`;
- marcas y ownership en HIR;
- schema y transporte en MIR 1.6;
- buffer `[16 x i8]` en el frame y ausencia de llamada a `__nativesharp_alloc`;
- selección de cleanup object-only;
- enlace y ejecución con resultado 42;
- conservación de heap al observar identidad, invocar un método o declarar constructor explícito;
- rechazo `noheap` cuando una llamada como receptor impide demostrar el no escape.

Las regresiones adyacentes cubren placement, destrucción explícita, los dos ejes de ownership,
contextos/propagación `noheap`, lowering OOP y consumidores del contrato y baseline MIR.

## Mensaje de commit / Commit message

`Promueve clases no escapantes al stack / Promote nonescaping classes to stack`
