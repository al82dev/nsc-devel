# Tanda 13: auditoría inicial de plantillas C++

Base: cierre de la tanda 12. Destino: `x86_64-pc-windows-msvc`, Clang 23.1.0.

## Método

- Sonda `pruebas-tanda-13-auditoria/sonda.py`, con 12 casos. Cada caso compila con Clang una biblioteca
  C++20 estática a partir de un encabezado y una implementación, la consume desde un proyecto
  NativeSharp con `CppLibraryReference` en O0 y ejecuta el resultado, que devuelve 0 si los valores
  coinciden. Evidencia: `A:/temp/nsc-t13-sonda-gytrs6aq`.
- Fragmentos NativeSharp puros con genéricos propios (evidencia `A:/temp/nsc-sonda-ns13-9k30jioe`).

## Modelo existente en NSC (T13-001)

- Los genéricos NativeSharp se monomorfizan (`ProjectGenericModel`): cada especialización es un tipo o
  método propio, con una clave que incluye los argumentos de tipo.
- El analizador sintáctico decide si `A.B<int> x` es una declaración con los nombres de tipo conocidos
  (`collectParserKnownTypeNames`): los de las fuentes NativeSharp y los records C/C++ importados. Un
  nombre genérico desconocido se lee como comparación (`expected expression`).
- Un record C++ dentro de una especialización de plantilla (`specializationContext`) no tiene identidad
  NativeSharp (`projectCppRecordIdentity`), y las especializaciones implícitas no están entre las
  declaraciones que recorre `collectRecords`.
- `emitCppInlineIR` solo emite funciones `TK_NonTemplate` con cuerpo; un miembro de una especialización
  implícita no tiene cuerpo hasta que Sema lo instancia.
- Ninguna proyección mira las plantillas de función.

## Resultado

| Ítem | Caso | Resultado |
|---|---|---|
| T13-004, T13-023 | alias `using IntBox = Box<int>` y método de la especialización | `C/C++ type alias cannot yet be represented without losing type information: 'A.IntBox'` |
| T13-004, T13-006 | `A.Box<int> b` | `expected expression`: el analizador no conoce `A.Box` como genérico |
| T13-005 | `A.twice<int>(3)` | `generic call 'twice<...>' has no visible Stage 44 generic method template` |
| T13-005 | `A.twice(3)` (deducción) | `unknown identifier 'A'` |
| T13-010 | instanciación explícita en la biblioteca, alias `LAcc` | mismo rechazo del alias |
| T13-012 | `Arr<4>` por alias | mismo rechazo del alias |
| T13-015 | especialización parcial elegida por Clang en una función inline | pasa |
| T13-017 | plantilla restringida por un concept, `inc<int>` | `no visible Stage 44 generic method template` |
| T13-021 | función `constexpr` inline | pasa |
| T13-022 | variables `constexpr` | `unknown identifier 'A'` |
| T13-024 | `A.Box<P>` con `P` struct NativeSharp | `unknown identifier` |
| T13-014 | paquete de parámetros `count<int, int>` | `no visible Stage 44 generic method template` |

Lo que pasa no usa plantillas desde NativeSharp: la especialización parcial y la función `constexpr`
son funciones inline que Clang emite (T10-012).

## Código relevante

| Componente | Archivo |
|---|---|
| Nombres de tipo conocidos por el analizador | `src/Frontend/ProjectCompilation.cpp` (`collectParserKnownTypeNames`) |
| Alias C/C++ hacia tipos importados | `src/Frontend/NameLookup/QualifiedTypeNames.cpp` |
| Identidad de records | `src/Interop/Cpp/CppSemanticRecord.cpp` (`projectCppRecordIdentity`) |
| Captura de records y funciones | `src/Interop/Cpp/ClangSession.cpp` (`collectRecords`, `collectFreeFunctions`) |
| Definiciones inline | `ClangSession.cpp` (`emitCppInlineIR`) |
| Genéricos NativeSharp | `include/nativesharp/Frontend/ProjectGenericModel.h` |
