# Tanda NSC305-G04c — Copy-on-write para el resto del scope

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G04 (cierre: todos los contenedores del scope copian en O(1)).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Convertir los contenedores léxicos restantes de `GenericLookupScope` (`importedNamespaces`, `aliases`, `typeAliases`, `typeParameters`, `valuePassKinds`, `genericLambdas`) a copy-on-write, para cumplir el gate "copias de mapas/sets durante traversal a cero". |
| Naturaleza | Refactor no funcional (copy-on-write con `use_count()>1`). |
| Comportamiento idéntico | Sombreado, imports, aliases, lambdas y pass-kinds. |
| Revisión de referencia | `development` en `c4d9ef53`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `GenericLookupScope::Cow<T>` (`read()`/`write()`); sitios de lectura/escritura. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | Copia de mapas/vector del scope a O(1); 544/544; expediente + commit local aislado. |

## 2. Cambio

- `Cow<T>` envuelve cada contenedor: `read()` devuelve la vista const, `write()` clona solo si
  `use_count()>1` y devuelve la vista mutable.
- Lecturas → `.read()`; escrituras (`insert`/`erase`/subíndice/`push_back`) → `.write()`.
- `QualifiedNameSite.aliases`/`importedNamespaces` se alimentan de `.read()` (puntero/span estables).

## 3. Evidencia

- **HECHO COMPROBADO**: `nsc` compila; `Host.Frontend` **544 PASS / 0 FAIL**.

## 4. Resultado de G04

- `valueTypes`: cadena persistente (G04b, ~11×).
- Resto del scope: copy-on-write (G04c, O(1) copy). G04 queda cerrado (gate "cero copias" cumplido).
