# Tanda 19, cambio 4: miembros de instancia C++ protegidos y privados

## Defecto

En la [auditoría inicial](../TANDA-19-AUDITORIA-INICIAL.md), `k.secret` sobre un campo privado de una clase C++ dice
`has no compatible member named 'secret'`. Clang dice que es privado. Lo mismo pasa con los métodos de instancia
(t11-cierre).

Causa en el código: `projectCppSemanticInstanceField` (`CppSemanticRecord.cpp`) y
`projectCppSemanticInstanceMethod` (`CppSemanticFunction.cpp`) descartan un miembro que no es público. Los campos
y métodos estáticos ya conservaban su acceso (T10-007) y NativeSharp lo comprueba en cada uso.

## Diseño

Un campo de instancia de un record C++ y un método de instancia del propio record conservan su acceso protegido o
privado. Pasa por `CppSemanticInstanceField/Method::access` y `CppImportedInstanceField/Method::accessibility` al
`ClassMemberSymbol`. El uso se rechaza con el diagnóstico de NativeSharp (`member 'x' is not accessible`).

- Un método llamado por un adaptador de Clang (virtual o heredado) sigue exigiendo ser público, porque el
  adaptador lo llama desde fuera de la clase.
- Un campo C sigue siendo público.

## Cambio

- `CppSemanticRecord.cpp/.h`, `CppImportedInstanceField.h`.
- `CppSemanticFunction.cpp/.h`, `CppImportedInstanceMethod.h`.
- `ProjectMemberModel.cpp`.

No se modificó Clang ni NSLink. El catálogo NSMeta no cambia de formato: el acceso del campo ya estaba en él.

## Evidencia

Fixture nuevo `pruebas-tanda-19-acceso`, con y sin `--dll`. Una clase con dos campos y dos métodos protegidos o
privados (uno inline) y miembros públicos que los usan: 48, igual que Clang, O0–O3. Cada uso de un miembro
privado o protegido se rechaza (`member 'x' is not accessible`), y Clang lo rechaza también. Son cuatro casos:
campo privado, campo protegido, método privado y método protegido.

| Variante | Evidencia |
|---|---|
| Estática | `A:/temp/nsc-t19-acceso-vhdg42b2` |
| DLL | `A:/temp/nsc-t19-acceso-c9mbbra_` |

## Límites

- Los métodos virtuales o heredados privados o protegidos siguen sin importarse.
- Records y enums anidados privados o protegidos siguen pendientes (T19-005).
