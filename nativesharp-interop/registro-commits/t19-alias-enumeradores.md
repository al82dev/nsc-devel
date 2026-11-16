# Tanda 19, cambio 7: enumeradores y miembros estáticos a través de un alias de namespace

## Defecto

Después del [cambio 6](t19-alias.md), con `namespace A = B;`, `A.E` funciona como tipo pero `A.E.b` dice
`unknown identifier 'A' in callable/initializer context`; `B.E.b` sí funciona.

Causa en el código:

- En posición de tipo, `expandCppTypeAlias` (`QualifiedTypeNames.cpp`) expande el alias `A::E` del inventario al
  enum `B::E`.
- En una expresión, el receptor `A.E` de `A.E.b` se liga en `bindExpressionTypeReceiver`
  (`ExpressionNames.cpp`), que llama a `resolveExpressionTypeCandidates` (`SyntaxBinding.cpp`). Esa función solo
  busca tipos con ese nombre y no consulta los alias de tipo C/C++, así que `A.E` queda sin ligar y el análisis
  del cuerpo toma `A` como identificador.

El intento anterior, agregar el alias al inventario de namespaces, no tocaba este camino; se retiró.

## Cambio

`resolveExpressionTypeCandidates` (`SyntaxBinding.cpp`): si un nombre calificado no nombra un tipo, busca en los
alias de tipo C/C++ con la escalera de nombres calificados del sitio. El receptor se liga al enum o record que
nombra el alias con las mismas reglas de identidad que la posición de tipo:

- alias del proyecto o de uno visible, fuera de namespaces anónimos, con destino `x86_64-pc-windows-msvc`;
- enum o record sin punteros ni `const`;
- una sola declaración importada con ese nombre y USR compatible (y la misma base, en un enum);
- ordinal visible para el proyecto que lo usa.

Si un nombre nativo coincide, si dos alias nombran tipos distintos o si falta alguna condición, el receptor queda
sin ligar y se conserva el diagnóstico actual. No se modificó Clang ni NSLink.

## Evidencia

Fixture `pruebas-tanda-19-alias`, ampliado, con y sin `--dll`:

- `A.E e = A.E.b` (antes usaba `B.E.b`);
- `A.P.seven()`, método estático de un struct de B;
- todo lo del cambio 6.

Resultado 713547, igual que Clang 23.1.0, O0–O3.

Rechazos genuinos, también en Clang:

- `A.Z`, que B no declara;
- `A.E.z`, que `B::E` no declara: NativeSharp dice `receiver/type 'type:T19Alias::B.E' has no compatible member named 'z'`; Clang dice `no member named 'z' in 'B::E'`.

| Variante | Evidencia |
|---|---|
| Estática | `A:/temp/nsc-t19-alias-ajchrflj` |
| DLL | `A:/temp/nsc-t19-alias-4jl4haln` |

## Límites

- Las plantillas de clase por el alias (`A.vector<int>`).
- Un alias de tipo sin calificar (`using F = B::E;` y luego `F.b`) no se liga como receptor; este cambio solo
  cubre nombres calificados.
