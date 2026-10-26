# Congelamiento 3.0 P1 (DEC-12) — `where` en miembros de tipos genéricos

Fecha: 2026-10-26. Rama: `development-release-candidate`. Revisión de partida: `e3e451f86`.

## Propósito y problema

DEC-12 pide que las operaciones que copian un `T` exijan `T` copiable. `where T : copy` ya funcionaba en un método
genérico (`Dup<T>(T v) where T : copy`), pero un miembro de una clase genérica no podía restringir el `T` de su
tipo:

```
class Box<T> { public T Copy() where T : copy { ... } }
// error: 'T' is not a generic or lifetime parameter of this declaration
```

Decisión de Alberto Huerta Luna (P1, 2026-10-26): extender `where` a los miembros de tipos genéricos. La
restricción sobre el `T` del tipo se valida al resolver la llamada y debe conservarse en NSMETA para los
consumidores precompilados.

## SCOPE LOCK

- **Objetivo.** Un método de una clase o struct genérica acepta una cláusula `where` sobre un parámetro de su
  tipo. Una especialización cuyo argumento no la cumple deja al miembro inválido, y la llamada se rechaza con el
  diagnóstico de restricción existente.
- **Naturaleza.** Extensión del lenguaje autorizada por P1.
- **Debe quedar idéntico.**
  - Cláusulas de tipos y de métodos genéricos.
  - El error para un parámetro que la declaración no tiene, incluidos un método de una clase no genérica y un
    tipo anidado que nombra el `T` del tipo que lo contiene.
  - Formato de NSMETA.
  - Corpus de `await` y sondas de DEC-11.
- **Modificable.**
  - AST: `GenericConstraintClause.h`.
  - Parser: `Parser.h`, `DeclarationParser.cpp`, `GenericSyntaxParser.cpp`.
  - Genéricos: `GenericCloner.cpp`, `TemplateCatalogCodec.cpp` (caché C1).
  - Restricciones: `ProjectGenericConstraintModel.h` y `.cpp`.
  - `Freeze30ContractTests.cpp`.
- **Prohibido.**
  - Validar la cláusula al declarar la especialización: una especialización que no usa el miembro compila.
  - Métodos genéricos dentro de tipos genéricos (siguen diferidos).
  - Cambiar la biblioteca (eso es P2 y P3).
- **Terminación.** Sondas de abajo en `-O0` y `-O2`, y el consumidor de una biblioteca precompilada.

## Contrato

1. **Parser.** Dentro del cuerpo de una clase o un struct, el parser recuerda los parámetros genéricos propios
   del tipo (no los de un tipo que lo contiene). En un método sin parámetros genéricos propios, una cláusula
   `where` cuyo parámetro raíz es uno de ellos se acepta y se marca `enclosingTypeParameter`. Cualquier otro
   nombre sigue dando «'X' is not a generic or lifetime parameter of this declaration».
2. **Especialización.** El clonador conserva la marca y sustituye el argumento de la cláusula con los argumentos
   de la especialización.
3. **Validación.** `validateSpecialization` llama a `validateSpecializationMemberClauses` para cada
   especialización. Para cada método con cláusulas marcadas, valida cada cláusula contra el argumento concreto,
   con el nombre `Tipo.Miembro`. Si alguna falla, el miembro entra en `invalidSpecializations_` y el diagnóstico
   queda diferido en `deferredBySpecialization_`. No se emite al especializar.
4. **Llamada.** La resolución de llamadas ya rechaza un candidato inválido y emite el diagnóstico diferido en la
   llamada: «generic parameter 'T' of 'Box.Copy' requires copyable storage ('where T : copy'), but the argument
   'Guard' does not satisfy it». El cuerpo del miembro inválido no se analiza ni se baja.
5. **Bibliotecas.** NSMETA ya lleva el texto fuente de las plantillas, y el consumidor lo vuelve a analizar con el
   mismo parser. La cláusula llega sin cambiar NSMETA. La caché C1 de plantillas sí guarda la marca nueva, así
   que su esquema pasa de `nsc-c1-template-v2` a `nsc-c1-template-v3` y las entradas viejas se descartan.

## Cambio

- `GenericConstraintClause::enclosingTypeParameter`.
- `Parser::enclosingGenericParameters_`. `parseClassBody` y `parseStructDeclaration` lo reemplazan por los
  parámetros del tipo y restauran el del tipo exterior al terminar.
- `beginGenericConstraintClause` acepta el parámetro del tipo en un método sin genéricos propios.
- `GenericCloner` copia la marca y sustituye el argumento.
- `TemplateCatalogCodec`: esquema v3 y un byte para la marca.
- `ProjectGenericConstraintModel::validateSpecializationMemberClauses`.
- `Freeze30ContractTests`: `testMemberCopyConstraint` y `testMemberCopyConstraintLibrary`.

## Validación

nsc Release recompilado (exit 0); `nsc_component_tests` Debug y Release compilan. Sondas en
`A:\temp\nsc-dec\d12`:

| Sonda | Forma | Resultado (`-O0` y `-O2`) |
|---|---|---|
| W1Accept | `Box<int>.Copy()` y `Box<Token>.Copy()` (struct sin destructor) | 7 |
| W2Reject | `Box<Guard>.Copy()`, `Guard` con destructor | rechazada en la llamada (21:19) |
| W3NoUse | `Box<Guard>` sin llamar a `Copy` | compila; 115, igual que sin el miembro (W3Baseline) |
| W4NonGeneric | `where T : copy` en un método de una clase no genérica | «'T' is not a generic…» |
| W5MethodGeneric | `Dup<T>(T) where T : copy` | 9 |
| W6Struct | `Cell<int>.Copy()` en un struct genérico | 11 |
| W7StructReject | `Cell<Guard>.Copy()` | rechazada en la llamada |
| W8Unknown | `where U : copy` en un miembro de `Box<T>` | «'U' is not a generic…» |
| W9Nested | clase anidada que nombra el `T` del tipo exterior | «'T' is not a generic…» |
| WLibOk | consumidor de `WLibApi.lib` precompilada, `Box<int>` | 13 |
| WLibBad | consumidor de `WLibApi.lib`, `Box<Guard>.Copy()` | rechazada, «template is declared in module 'WLibApi'» |

- W4 y W8 fallaban igual antes del cambio.
- Sondas de DEC-11 con `List` y `LinkedList` (C12ListGrow, C12ListRemove, C12LinkedListT, L2ReverseStruct,
  P2ListAdd): sin cambios.
- Corpus de `await` (51 programas, run `p1`): mismos resultados de compilación y de salida que `d04`.

## Observación (no corregida aquí)

W3 da 115 y no 105: `only = move(value)` destruye el valor por defecto del campo (`Id` 0) antes de asignar.
Ocurre igual sin la cláusula (W3Baseline) y no tiene relación con P1. Las reglas de DEC-11 cubren los slots de
arreglos, no los campos.

## No verificado

- `Freeze30ContractTests` no está configurado en los árboles locales; sus casos nuevos son las sondas de arriba.
- Batería completa: queda para el cierre final.
