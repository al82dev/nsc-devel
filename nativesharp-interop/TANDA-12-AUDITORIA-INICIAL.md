# Tanda 12: auditoría inicial de herencia, despacho y tipos dinámicos C++

Base: cierre de la tanda 11. Destino: `x86_64-pc-windows-msvc`, Clang 23.1.0.

## Método

Sonda `pruebas-tanda-12-auditoria/sonda.py`, con 12 casos. Cada caso compila con Clang una biblioteca
C++20 estática a partir de un encabezado y una implementación, la consume desde un proyecto
NativeSharp con `CppLibraryReference` en O0 y ejecuta el resultado, que devuelve 0 si los valores
coinciden. La sonda no prueba DLL, plantilla precompilada ni otros niveles de optimización. Evidencia:
`A:/temp/nsc-t12-sonda-dm_8r292`.

## Modelo existente en NSC

- `ClangSession.cpp` captura las bases de cada record (tipo, acceso, si es virtual y offset de Clang),
  pero ninguna proyección las usa: un record derivado solo tiene sus propios campos y métodos.
- Los métodos virtuales se rechazan (`projectCppSemanticInstanceMethod`), y los métodos de vida solo
  se importan para records sin bases.
- NativeSharp no tiene herencia de structs ni conversión de puntero de derivado a base para un tipo C++.

## Resultado

Ningún caso pasa.

| Ítem | Caso | Resultado |
|---|---|---|
| T12-001 | campos de una base pública | `no compatible member named 'x'` |
| T12-003 | `D*` pasado donde se espera `B*` | además, `no viable overload … accepts the argument types` |
| T12-004 | método de la base sobre el derivado | `no compatible member named 'getX'` |
| T12-002, T12-008 | método de la segunda base en herencia múltiple | `no compatible member named …` |
| T12-005, T12-006 | método virtual por un `B*` que apunta a un `D` | `no compatible member named 'f'` |
| T12-006 | método virtual de un objeto construido en NativeSharp | `no compatible member named 'f'` |
| T12-007 | destrucción mediante base con destructor virtual (`delete` en la biblioteca) | NSLink: `unresolved external symbol '??2@YAPEAX_K@Z'` |
| T12-009 | herencia virtual | NSLink: `_Init_thread_epoch` sin resolver (estática con guarda) |
| T12-010, T12-011 | `dynamic_cast` en la biblioteca | NSLink: `??3@YAXPEAX_K@Z` sin resolver |
| T12-001 | base privada | `no compatible member named 'x'` (correcto) |
| T12-005 | override en el derivado | `no compatible member named 'f'` |

Los fallos de enlace no son de herencia: la biblioteca estática usa `new`, `delete` o estáticas con
guarda del CRT, que un ejecutable NativeSharp no enlaza (tanda 16). Con una DLL enlazada con su CRT el
objeto, su vtable y su RTTI viven en la DLL.

## Código relevante

| Componente | Archivo |
|---|---|
| Captura de bases, campos y métodos | `src/Interop/Cpp/ClangSession.cpp` (`collectRecords`) |
| Proyección de métodos de instancia | `src/Interop/Cpp/CppSemanticFunction.cpp` |
| Campos de instancia | `src/Interop/Cpp/CppSemanticRecord.cpp` |
| Definiciones emitidas por Clang | `ClangSession.cpp` (`emitCppInlineIR`), `DriverArtifactGeneration.cpp` |
