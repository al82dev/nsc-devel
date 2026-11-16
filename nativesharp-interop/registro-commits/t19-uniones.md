# Tanda 19, cambio 5: campos de una `union` C++ de miembros escalares

## Defecto

En la [auditoría inicial](../TANDA-19-AUDITORIA-INICIAL.md), `union U { int i; float f; }` en C++ no da sus
campos (`has no compatible member named 'i'`); la misma union en C sí.

Causa en el código: `projectCppSemanticInstanceField` (`CppSemanticRecord.cpp`) descarta todo campo de una union
de un record C++ (`record.kind == Union && record.isCxx`).

## Diseño

Una union C++ cuyos miembros son todos escalares, punteros, enums o arreglos de ellos tiene el layout y la
semántica de una union C. Escribir un miembro lo hace activo, y ningún miembro tiene constructor ni destructor.
Sus campos se proyectan como los de una union C. Una union con un miembro record sigue fuera, porque sus
miembros pueden tener vida propia.

## Cambio

- `CppSemanticRecord.cpp`: `cppUnionHasOnlyScalarMembers` y su uso en `projectCppSemanticInstanceField`.

No se modificó Clang ni NSLink. El catálogo NSMeta no cambia.

## Evidencia

Fixture nuevo `pruebas-tanda-19-uniones`, con y sin `--dll`, sobre `union A::U { int i; float f; unsigned char
b[4]; }`:

- escritura de `i` y lectura de `b[0]`;
- escritura de `f` y paso por puntero;
- paso por valor;
- una union dentro de un struct pasado por referencia.

Resultado 1127, igual que Clang, O0–O3.

| Variante | Evidencia |
|---|---|
| Estática | `A:/temp/nsc-t19-uniones-pa05h4s6` |
| DLL | `A:/temp/nsc-t19-uniones-suxihlsy` |

## Límites

- Unions C++ con miembros record (con o sin vida no trivial) y unions anónimas dentro de una clase C++.
