# Tanda 19, cambio 6: tipos C++ nombrados a través de un alias de namespace

## Defecto

En la [auditoría inicial](../TANDA-19-AUDITORIA-INICIAL.md), con `namespace A = B;`:

- `A.get(p)` funciona (T10-002);
- `A.P p;` es un error de sintaxis;
- `new A.P()`, `var` y un parámetro `A.P` dicen `unknown type name 'A.P'`.

En la tanda 18, `icu.UnicodeString` no se podía escribir.

Causa en el código: `addAlternateFunctionNames` (`ClangSession.cpp`) da a las funciones sus nombres alternativos.
Los tipos solo tienen el nombre de su namespace y el inventario de alias de tipo (`typeAliasesSnapshot`) solo
contiene `typedef` y `using`.

## Diseño

Un alias de namespace nombra cada tipo de su namespace destino como en la búsqueda calificada de C++:

- `A::T` para un tipo de B;
- `A::N::T` para uno de un namespace anidado `N`;
- los namespaces inline no agregan segmento.

`addNamespaceAliasTypeNames` agrega cada uno al inventario de alias de tipo con el tipo que nombra, así que
NativeSharp resuelve `A.T` como `B.T` por el camino de los alias. Cubre records, enums y typedefs en todas las
reaperturas del namespace. Quedan fuera las plantillas de clase, sus especializaciones y los nombres que ya
existen.

## Cambio

- `ClangSession.cpp`: `addNamespaceAliasTypeNames`, llamada desde `typeAliasesSnapshot`.

No se modificó Clang ni NSLink.

## Evidencia

Fixture nuevo `pruebas-tanda-19-alias`, con y sin `--dll`. Con `namespace A = B;` se usan:

- `A.P` (struct) en un local, en `new` con `var` y como parámetro de una función NativeSharp;
- `A.N.Q` (struct de un namespace anidado);
- `A.Count` (typedef) y `A.E` (enum class, como tipo);
- funciones de B llamadas por el alias.

Resultado 13547, igual que Clang, O0–O3. Rechazo genuino, también en Clang: `A.Z`, que B no declara.

| Variante | Evidencia |
|---|---|
| Estática | `A:/temp/nsc-t19-alias-r02fmsvp` |
| DLL | `A:/temp/nsc-t19-alias-hq574yfk` |

## Límites

- Un enumerador nombrado a través del alias (`A.E.b`) no se resolvía (`unknown identifier 'A'`); `B.E.b` sí.
  Agregar el alias al inventario de namespaces (`namespacesSnapshot`) no lo resolvió; se probó y se retiró. Lo
  resuelve el [cambio 7](t19-alias-enumeradores.md).
- Las plantillas de clase por el alias (`A.vector<int>`).
