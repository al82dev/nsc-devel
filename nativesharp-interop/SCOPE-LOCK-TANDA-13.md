# Alcance de ejecucion: tanda 13

Base: development-cpp-clang al cierre de la tanda 12 (`bec73b6ea`), arbol limpio al inicio.

Objetivo: completar T13-001 a T13-025 (plantillas C++, sus argumentos, instanciacion, constexpr) con auditoria, implementacion, pruebas dirigidas, validacion agrupada, documentacion y commits locales. El usuario autoriza las decisiones de diseno. Direccion: bibliotecas C++ consumidas desde NativeSharp.

Restricciones: Clang oficial 23.1.0. No se modifican Clang ni NSLink sin autorizacion posterior y necesidad demostrada. No se despliega en C:, ni se publica o reescribe la rama remota. NSMeta mantiene un unico contrato vigente, sin versiones historicas. Daikon y el camino inverso quedan fuera. Se conservan las pruebas existentes; un negativo que un cambio deje obsoleto pasa a comparacion positiva contra Clang, con un rechazo genuino separado.

Auditoria inicial: [TANDA-13-AUDITORIA-INICIAL.md](TANDA-13-AUDITORIA-INICIAL.md), con la sonda pruebas-tanda-13-auditoria/sonda.py (evidencia A:/temp/nsc-t13-sonda-gytrs6aq).

Diseno: NSC no reimplementa la deduccion, la sustitucion, la seleccion de especializaciones ni los concepts: los aplica Sema de Clang en la sesion. Una especializacion pedida desde NativeSharp (`A.Box<int>`, `A.twice<int>(3)`) o nombrada en el encabezado (un alias, una firma) se instancia en la sesion con los argumentos resueltos; sus diagnosticos son los de Clang. La especializacion de clase es un record importado con un nombre NativeSharp propio derivado de su tipo canonico, el mismo en toda sesion; sus metodos se llaman por adaptadores de Clang y sus definiciones las emite Clang con weak_odr, asi que dos modulos que piden la misma especializacion enlazan una sola definicion.

Plan de cambios, en orden; cada uno con su registro y commit:

1. Especializaciones de clase nombradas en el encabezado (alias, firmas): instanciacion, identidad, campos, metodos, constructores y destructor (T13-003, T13-004, T13-010, T13-011, T13-012, T13-015, T13-018, T13-023).
2. Especializaciones de clase escritas en NativeSharp (`A.Box<int>`), con argumentos de tipo C++, enteros y punteros, y diagnosticos de sustitucion y concepts de Clang (T13-002, T13-006, T13-016, T13-017).
3. Plantillas de funcion con argumentos explicitos, deducidos y paquetes (T13-005, T13-013, T13-014).
4. constexpr: funciones y variables evaluadas por Clang (T13-021, T13-022).
5. Structs NativeSharp como argumentos (T13-007, T13-008, T13-009, T13-024, T13-025).
6. COMDAT y la misma especializacion desde dos modulos (T13-019, T13-020); validacion agrupada y cierre.

Avance: cambio 1 en [t13-especializaciones.md](registro-commits/t13-especializaciones.md); cambio 2 y los argumentos explicitos y paquetes del cambio 3 en [t13-plantillas-ns.md](registro-commits/t13-plantillas-ns.md). Cambio 4 en [t13-constexpr.md](registro-commits/t13-constexpr.md). Cambio 5 en [t13-structs-ns.md](registro-commits/t13-structs-ns.md). Cambio 6 (COMDAT y dos modulos) en [t13-comdat.md](registro-commits/t13-comdat.md). La deduccion desde los argumentos de una llamada en [t13-deduccion.md](registro-commits/t13-deduccion.md).

Cada capacidad se compara con un control compilado por Clang. Se prueba en estatica y DLL, de O0 a O3.

Fuera: plantillas NativeSharp consumidas desde C++.

Presupuesto inicial: ClangSession.cpp, CppInteropSession.h, CppRecordDeclaration.h, CppFunctionDeclaration.h, CppSemanticRecord.cpp, CppSemanticFunction.cpp, ProjectCompilation.cpp, NameLookup, DriverArtifactGeneration.cpp, fixtures pruebas-tanda-13-* y documentacion.
