# 225 — Movimiento uniforme hacia agregados poseídos / Uniform moves into owned aggregates

**Línea / Line:** `1.0.1-dev`

**Alcance / Scope:** semántica de ownership, OOP HIR y evidencia dirigida O0

## Resumen

Esta entrega elimina una decisión duplicada entre análisis semántico y OOP HIR. Una asignación a
almacenamiento que posee su valor ahora lleva esa propiedad explícitamente en
`MethodExpressionTypeBinding`. La misma señal cubre campos `owned` y elementos referencia de un
arreglo alcanzado mediante un binding `owned` o `raii`.

Con ello, reemplazar un elemento de un arreglo poseído:

1. exige `move(owner)`, `new T(...)`, `null` o un retorno `owned`;
2. rechaza un alias común que convertiría silenciosamente una referencia prestada en dueño;
3. libera el valor desplazado antes de instalar el nuevo;
4. reutiliza el lowering address-based existente, sin una segunda implementación de destrucción.

No cambia el contrato de `delete[]`: el arreglo nativo sigue siendo almacenamiento explícito y la
liberación del buffer no recorre sus elementos. La regla sólo gobierna la sustitución de una ranura
referencia que el programa declaró como parte de almacenamiento poseído.

## Problema medido

Los campos, propiedades, indexadores, locales y parámetros poseídos ya tenían reglas de
transferencia, pero OOP HIR reconocía un destino poseído volviendo a inspeccionar únicamente la
forma sintáctica de un campo. Un elemento `items[i]` no tiene esa forma aunque `items` sea un campo
`owned T[]`. El frontend aceptaba el movimiento, almacenaba el nuevo puntero y perdía el valor que
la ranura contenía.

El error tenía dos mitades:

- un alias no poseído podía escribirse en la ranura;
- una transferencia válida podía sobrescribir un owner vivo sin liberarlo.

Resolver solamente una mitad habría cambiado una fuga por doble ownership. Por eso ambas dependen
de una sola decisión semántica.

## Diseño e implementación

### `MethodExpressionTypeBinding::ownsStorageDestination`

`ExpressionResult` calcula si una asignación `=` termina en almacenamiento poseído y
`recordExpressionType()` conserva esa decisión para las etapas posteriores. El binding evita que
OOP HIR tenga que repetir lookup de miembros o inferir ownership a partir del AST.

### Detección del arreglo poseído

`ProjectMethodBodyModel::analyzeAssignment()` analiza la base del indexador nativo y combina:

- que el destino sea un elemento de arreglo nativo o fijo;
- que la base lleve ownership `Owned`, o sea un campo de instancia declarado `owned`;
- que el elemento sea una referencia de clase o `string` nativo.

Los elementos por valor no adquieren cleanup ficticio. Los arreglos fijos sin un binding poseído
conservan su contrato crudo anterior.

### Regla única de transferencia

Campos y elementos poseídos comparten el mismo conjunto de RHS permitido. La diferencia del
diagnóstico sólo identifica el destino: un campo muestra su nombre y un elemento informa que
pertenece a un arreglo poseído.

### Lowering único

`ProjectOopHirModel` consulta el binding de la expresión de asignación. Cuando
`ownsStorageDestination` está activo marca el `Store` para la ruta ya existente de
release-before-store. `ProjectOopLlvmLowerer` opera sobre la dirección calculada, por lo cual sirve
igual para un campo y para una ranura de arreglo y no necesitó cambios.

## Archivos modificados

| Archivo | Cambio |
|---|---|
| `include/nativesharp/Frontend/ProjectMethodBodyModel.h` | Conserva la propiedad semántica del destino en `ExpressionResult` y `MethodExpressionTypeBinding` |
| `src/Frontend/ProjectMethodBodyModel.cpp` | Reconoce elementos de arreglos poseídos, unifica la validación de transferencia y registra la decisión |
| `src/Frontend/ProjectOopHirModel.cpp` | Sustituye la redetección exclusiva de campos por la señal semántica única |
| `tests/unit/Stage50_11OwnershipExecutionTests.cpp` | Agrega conteo O0 de liberaciones y una negativa contra alias común |
| `mkdown/225_1_0_1_movimiento_agregados_poseidos.md` | Registra alcance, diseño, decisiones y evidencia de la entrega |

## Evidencia dirigida

La prueba positiva crea dos `Resource`, mueve ambos sucesivamente a `items[0]` y finalmente limpia
la ranura con `null`. El resultado esperado es exactamente dos destrucciones: una al desplazar el
primer valor y otra al limpiar el segundo. La prueba negativa intenta `items[0] = value` con una
referencia sin ownership y exige el diagnóstico de transferencia.

Durante este lote no se ejecuta ninguna prueba ni baseline intermedio. Después del commit se
reconstruye desde cero únicamente el objetivo `nsc`; la evidencia ejecutable se consume en el
baseline acumulativo posterior a la entrega 234.

---

## English

This delivery removes a duplicated decision between semantic analysis and OOP HIR. An assignment
to storage that owns its current value now carries that fact explicitly through
`MethodExpressionTypeBinding`. The same signal covers `owned` fields and reference elements of an
array reached through an `owned` or `raii` binding.

Replacing an owned-array element therefore requires an ownership-transfer expression, rejects a
plain alias, and releases the displaced value before storing the replacement. The implementation
reuses the existing address-based release path; LLVM lowering needs no array-specific destruction
branch.

Native-array lifetime remains explicit: `delete[]` releases the buffer and does not walk its
elements. This change only defines replacement semantics for a reference slot belonging to
declared owning storage. Value elements and raw fixed arrays keep their previous behavior.

Semantic analysis is the single authority. `ExpressionResult` computes
`ownsStorageDestination`, `recordExpressionType()` preserves it, and OOP HIR consumes it instead
of rediscovering an owned field from syntax. Directed O0 evidence counts exactly two destructions
for two successive replacements and rejects storing a non-owning alias. Tests and the cumulative
baseline remain deferred until delivery 234; only a clean `nsc` rebuild follows this commit.
