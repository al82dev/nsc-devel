# Tanda NSC305-G04b — `valueTypes` persistente (elimina la copia O(N²))

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G04 (scopes persistentes; primer contenedor: `valueTypes`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Sustituir `GenericLookupScope::valueTypes` (mapa copiado en profundidad por nodo) por una cadena persistente parent-linked, de modo que copiar el scope sea O(1) y añadir un binding sea O(1). |
| Naturaleza | Refactor no funcional (mismo sombreado; cambia solo la estructura interna). |
| Comportamiento idéntico | Sombreado de locals (hijo pisa padre), inferencia local, tipos de parámetros/campos/locals. |
| Revisión de referencia | `development` en `36deb95d`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `GenericLookupScope` (+`ValueBinding`, `addValueType`, `findValueType`); sitios de lectura/escritura de `valueTypes`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | Copia profunda de `valueTypes` eliminada; 544/544; expediente + commit local aislado. |

## 2. Cambio

- `valueTypes` pasa de `unordered_map<string, TypeSyntax*>` a `shared_ptr<const ValueBinding>`
  (lista persistente `{name, type, parent}`). `addValueType` añade al frente (O(1)); `findValueType`
  recorre de más nuevo a más viejo, preservando el sombreado.
- Lecturas (`inferExpressionType`) y escrituras (parámetros/campos/locals/lambda) usan los helpers.
- El contador `noteScopeCopy` ya no cuenta `valueTypes` (dejó de copiarse).

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
- **RESULTADO MEDIDO** (fixture 400 structs + 400 locals, `--time`, mediana 3 ejecuciones):
  - `root-demand-discovery`: **351.177 µs → ~30.700 µs** (≈11,4×), estable en 3 corridas.

## 4. Pendiente (misma tanda G04)

- `valuePassKinds` sigue copiándose por valor (pequeño: un binding por parámetro, no por local); el
  resto del scope (`aliases`, `typeAliases`, `typeParameters`, `importedNamespaces`, `genericLambdas`)
  son léxicos y pequeños, por lo que se dejan por valor. Convertirlos a frames delta es el siguiente
  paso de G04 si la medición lo justifica.
