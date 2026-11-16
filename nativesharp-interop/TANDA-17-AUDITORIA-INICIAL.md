# Tanda 17: auditoría inicial

Base: `4f5bb7013` (cierre de la tanda 16). Clang 23.1.0, objetivo `x86_64-pc-windows-msvc`.

La sonda `pruebas-tanda-17-auditoria/sonda.py` (no es una prueba de soporte) compila cada caso con Clang y
un consumidor NativeSharp e imprime el diagnóstico de NSC. Evidencia: `A:/temp/nsc-t17-sonda-2s2e9fml`
(diagnósticos, depuración y caché) y `A:/temp/nsc-t17-sonda-8sfdige3` (medición).

## Lo que hace NSC hoy

- **Importación fallida (T17-001).** Una función cuyo parámetro es un puntero a miembro se rechaza en el uso
  NativeSharp (`Main.ns:4:12`, con la línea y el subrayado) con el motivo, pero sin la declaración C++: el
  mensaje no dice dónde la declara el encabezado. `CppFunctionDeclaration::location` existe y
  `CallAnalyzer` no lo usa.
- **Uso inválido (T17-002).** Una llamada con aridad o tipos que ninguna sobrecarga acepta se señala en
  `Main.ns` con línea, columna y subrayado.
- **Sobrecargas (T17-003).** `no viable overload of namespace function 'A.f' accepts the argument types`, sin
  las alternativas ni por qué cada una no sirve; lo mismo con la aridad y la ambigüedad.
  `reportNoViableNamespaceFunctionCall` tiene los candidatos evaluados (`evaluatedCandidates`, con la
  conversión de cada argumento) y no los informa.
- **Plantillas (T17-004).** Una instanciación inválida conserva las notas de Clang (`invalid operands to
  binary expression`, `in instantiation of function template specialization 'A::twice<A::S>' requested
  here`), pero sin su ubicación: `ClangSession` copia solo `message` de cada `CppSessionDiagnostic` y descarta
  `location`.
- **Encabezado o biblioteca ausentes (T17-005).** Se distinguen: `CppLibraryReference header does not
  exist: 'falta.h'` y `referenced library does not exist: '.../falta.lib'`.
- **Símbolo ausente o ABI distinta (T17-006).** Una función declarada y no definida, y una función que el
  encabezado declara `one(int)` y la biblioteca define `one(long long)`, terminan igual: el enlazador informa
  `unresolved external symbol: '?two@A@@YAHXZ'` o `'?one@A@@YAHH@Z'`, sin la declaración ni la definición
  parecida que sí está en la biblioteca.
- **Depuración (T17-007, T17-008).** Con `-g` el PDB tiene `Main.ns` y `api.cpp` (de la biblioteca compilada
  con `-g`) y los procedimientos `Main` y `A::one`. La función inline `A::two` que NSC emite desde el
  encabezado no tiene información de depuración: `emitCppInlineIR` usa las `CodeGenOptions` de la sesión, sin
  depuración.
- **Caché con opciones distintas (T17-009).** Con la caché de artefactos, cambiar `Define` (`K=1`, `K=2`,
  `K=1`) y luego el encabezado da 1, 2, 1 y 11: no se reutiliza un resultado de otras opciones.
- **Costo del análisis repetido (T17-010).** Un encabezado con `<vector>`, `<string>`, `<map>` y
  `<algorithm>` tarda unos 10 s por compilación (`precompiled-metadata` 0,8 s, `names` 2,9 s, `members`
  4,9 s); el mismo encabezado sin la STL, 0,2 s, y `clang -fsyntax-only` del encabezado, 0,5 s. Con la caché
  habilitada la segunda compilación tarda lo mismo (10,3 s): `lookupCompilationBundle` deshabilita la caché
  de compilación para toda sesión que no sea C (`options.language != C`), una barrera de cuando la identidad
  no registraba los encabezados transitivos (`registro-commits/build-cpp-sin-cache.md`). Hoy
  `sourceFileContentsSnapshot` y `preprocessingFingerprint` cubren todos los archivos que carga Clang, en C y
  en C++.
- **Documentación (T17-012).** No hay una guía de uso de `CppLibraryReference`; la información está repartida
  en los registros de las tandas.

## Plan

Ver [SCOPE-LOCK-TANDA-17.md](SCOPE-LOCK-TANDA-17.md).
