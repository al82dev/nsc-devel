# Tanda Universal 55: cierre físico del resultado de indexador genérico

## SCOPE LOCK

| Dimensión | Alcance autorizado |
|---|---|
| Objetivo | Corregir únicamente la ausencia de `Select` y `Main` en el `--dump-symbols` de nsc2 para la prueba 182. |
| Evidencia nueva | El objeto O0 de nsc2 muestra que `DumpSymbol` compara directamente el puntero devuelto por `List<bool>.get_Item` y no carga el `bool`. |
| Archivo modificable | `selfhost/nsc/CodeGen/LlvmBackend.ns`. |
| Símbolos modificables | La frontera de retorno de `IndexExpression` y un helper privado exclusivo para cerrar su resultado escalar genérico. |
| Protegido | Todo lo fijado en Tanda 54, incluida la prueba 181 y los clasificadores globales. |
| Terminación | El objeto generado carga el escalar cerrado, los dumps de nsc0/nsc2 coinciden y 182 pasa. |

## Resultado de la primera hipótesis

El commit `a8e199fda` preservó SH41/181, que pasó en O0 con NSLink, pero no modificó la salida de 182. La comparación siguió siendo:

```text
nsc0: exit 0, 2 líneas (Select y Main)
nsc2: exit 0, 0 líneas
```

Los dumps adicionales demostraron que nsc2 sí construye cinco símbolos y el mismo árbol semántico que nsc0. Por tanto, la pérdida sucede exclusivamente dentro de `DumpSymbols`.

El desensamblado de `DumpSymbol` aportó la prueba decisiva:

```text
call List<bool>.get_Item
cmp rax, 0
setne al
```

No aparece una carga de `i1`. El `SemanticFact` del indexador conserva el placeholder declarado, de modo que `ConvertScalar` retorna antes de observar un destino cerrado. Normalizar el texto del destino dentro de `ConvertScalar` no puede resolver un tipo que todavía no fue cerrado.

## Corrección causal

`ClosedGenericScalarResult` opera sólo en la rama de `IndexExpression` que ya resolvió un accesor:

1. obtiene la posición genérica desde el retorno declarado (`T`, `TElement`, `TKey`, `TValue` o `TPriority`);
2. obtiene el argumento cerrado desde el tipo del receptor, por ejemplo `List<bool>`;
3. sólo sustituye el resultado cuando ese argumento cerrado es un escalar físico;
4. entrega a `ConvertScalar` tipos distintos (`owned T -> bool`), habilitando la carga ya existente de la caja genérica.

El intento de normalización textual de Tanda 54 se retira en este commit hacia adelante porque la evidencia demostró que no participa en la reproducción. No se elimina ni reescribe ningún commit.

## Radio de impacto

- Indexadores no genéricos: sin cambio.
- Resultados genéricos de referencia: sin cambio.
- Llamadas ordinarias, argumentos, condiciones, ABI y conversiones numéricas: sin cambio.
- Sólo indexadores genéricos cerrados sobre un escalar cambian de puntero opaco a su valor físico.
- Complejidad: `O(g)` por la lectura del argumento genérico textual; memoria temporal `O(g)`.

## Validación dirigida

Después del commit se regenerará sólo SH41 O0, se repetirá la comparación de `--dump-symbols` y se ejecutará únicamente 182. No se ejecutará batería completa ni baseline.
