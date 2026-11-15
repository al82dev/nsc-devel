# Alcance de ejecucion: tanda 10

Base: development-cpp-clang 4d40c9373 (cierre de la tanda 9), arbol limpio al inicio.

Objetivo: completar T10-001 a T10-017 (nombres y operaciones de C++) con auditoria, implementacion, pruebas dirigidas, validacion agrupada, documentacion y commits locales. El usuario autoriza las decisiones de diseno. Direccion: bibliotecas C++ consumidas desde NativeSharp.

Restricciones: Clang oficial 23.1.0. No se modifican Clang ni NSLink sin autorizacion posterior y necesidad demostrada. No se despliega en C:, ni se publica o reescribe la rama remota. NSMeta mantiene un unico contrato vigente, sin versiones historicas. Daikon y el camino inverso quedan fuera. Se conservan las pruebas existentes; un negativo que un cambio deje obsoleto pasa a comparacion positiva contra Clang, con un rechazo genuino separado.

Auditoria inicial: [TANDA-10-AUDITORIA-INICIAL.md](TANDA-10-AUDITORIA-INICIAL.md), con la sonda pruebas-tanda-10-auditoria/sonda.py (evidencia A:/temp/nsc-t10-sonda-b5bb2s48).

Plan de cambios, en orden; cada uno con su registro y commit:

1. Parametros y retornos por referencia C++ (T10-003).
2. Argumentos predeterminados evaluados por Clang (T10-010).
3. Funciones inline y miembros estaticos constexpr sin simbolo en la biblioteca, emitidos por Clang en un objeto generado, como ya se hace con static inline de C (T10-012, T10-006).
4. Nombres de namespace: inline namespace explicito, alias y declaraciones using (T10-002).
5. Tipos anidados en clases: metodos estaticos de clases anidadas y enums anidados (T10-005, T10-002; pendiente heredado de la tanda 9).
6. Globales y campos estaticos de tipo arreglo, record y thread_local (T10-004, T10-006, T10-014, T10-015).
7. Acceso publico, protegido y privado, con diagnostico explicito (T10-007).
8. Operadores C++ libres sobre records (T10-016). Los operadores miembro dependen de los metodos de instancia (tanda 11).
9. Diseno de identidad (T10-001), resolucion de sobrecargas y conversiones (T10-008, T10-009), y simbolo de Clang (T10-011), documentados y probados.
10. Fixture integral con biblioteca de Clang oficial (T10-017), validacion agrupada y cierre.

Cada capacidad se compara con un control compilado por Clang. Se prueba en estatica y DLL, de O0 a O3 y, donde aplique, desde plantilla precompilada con y sin encabezado.

Presupuesto inicial: ClangSession.cpp, CppSemanticInventory.cpp, CppSemanticFunction*.cpp, ProjectNamespaceFunctionModel.cpp, ProjectNameLookup.cpp, CppCRecordMetadataCodec.cpp, emisores HIR/LLVM afectados, CppLifetimeObjectEmitter.cpp, DriverArtifactGeneration.cpp, fixtures pruebas-tanda-10-* y documentacion.

T10-003, implementacion 2026-11-14 (referencias lvalue en funciones libres): causa confirmada en codigo con la sonda (A:/temp/nsc-t10-sonda-b5bb2s48). projectFunction mapeaba la referencia con mapFunctionType, que solo conoce escalares, punteros, enums, records y punteros a funcion, y descartaba la funcion. Mapear el portador como puntero al referente y fijar ref/in y retorno ref, como las declaraciones NativeSharp. Fuera: T&&, retorno const T&, metodos estaticos. Presupuesto: CppSemanticFunction.cpp, fixture pruebas-tanda-10-referencias y documentacion.
