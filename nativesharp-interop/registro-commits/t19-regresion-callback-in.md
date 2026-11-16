# Tanda 19: argumentos `in` y `ref` en callbacks estructurales (T19-037)

La [línea base](t19-linea-base-nsc.md) encontró que `nsc.stage52-1.generic-collections-additions` pasa en `152654e5a` y falla
ahora. Se corrige en el compilador; la prueba no se modificó.

**Defecto.** `SortedList.ns`, de la biblioteca Core, deja de compilar:

```
SortedList.ns:135:33: error: indirect call argument would discard pointer pointee const qualification
    return comparer(in a, in b) == 0;
```

**Causa.** `bindDelegateInvocationArguments` (`CallAnalyzer.cpp`) rechaza un dato apuntado `const` pasado a un
parámetro puntero sin `const` de un callback estructural (T08-002). Un argumento `in` o `ref` es una referencia:
su parámetro (`InComparison<T>`: `int(in T, in T)`) es por referencia y se baja como puntero, pero no es un
puntero C. La comprobación lo trataba como uno.

**Cambio.** La comprobación se aplica solo a argumentos pasados por valor
(`argumentPassKind == ParameterPassKind::Value`). Las referencias conservan sus propias reglas de solo lectura.
Para un puntero `const T*` pasado por valor la comprobación no cambia; ninguna prueba existente la ejercita y no lo he verificado con un fixture (T19-036).

## Evidencia

- `ctest -R stage52-1`: pasa (fallaba desde la tanda 8).
- Fixtures con callbacks, CRT y STL (`lista-rg1.txt`: tandas 15, 16, 18, 19 y `09-llamadas`): 21 de 21 en
  estática (`logs/regresion-rg2`) y 17 de 17 en DLL (`logs/regresion-dll-rg2d`).

No se modificó Clang ni NSLink. El contrato NSMeta vigente es el único.
