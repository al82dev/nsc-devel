# Alcance de ejecucion: tanda 12

Base: development-cpp-clang al cierre de la tanda 11 (`89f91c97a`), arbol limpio al inicio.

Objetivo: completar T12-001 a T12-012 (herencia, despacho y tipos dinamicos C++) con auditoria, implementacion, pruebas dirigidas, validacion agrupada, documentacion y commits locales. El usuario autoriza las decisiones de diseno. Direccion: bibliotecas C++ consumidas desde NativeSharp.

Restricciones: Clang oficial 23.1.0. No se modifican Clang ni NSLink sin autorizacion posterior y necesidad demostrada. No se despliega en C:, ni se publica o reescribe la rama remota. NSMeta mantiene un unico contrato vigente, sin versiones historicas. Daikon y el camino inverso quedan fuera. Se conservan las pruebas existentes; un negativo que un cambio deje obsoleto pasa a comparacion positiva contra Clang, con un rechazo genuino separado.

Auditoria inicial: [TANDA-12-AUDITORIA-INICIAL.md](TANDA-12-AUDITORIA-INICIAL.md), con la sonda pruebas-tanda-12-auditoria/sonda.py (evidencia A:/temp/nsc-t12-sonda-dm_8r292).

Diseno: el ajuste de this, el despacho virtual por la vtable del MS ABI, la herencia multiple y la virtual no se reimplementan en NSC. Para cada metodo heredado o virtual que el programa usa, Clang genera un adaptador extern "C" con el ABI fisico de un metodo de instancia (this primero, retorno en memoria despues), cuyo cuerpo es la llamada C++ `self->m(args)` sobre el tipo derivado. NSC llama al adaptador como a cualquier metodo de instancia. Los campos de las bases publicas no virtuales se promueven al derivado con el offset de Clang.

Plan de cambios, en orden; cada uno con su registro y commit:

1. Campos de bases publicas no virtuales en el derivado, con el offset de Clang, y ocultacion por nombre (T12-001, T12-002, T12-008).
2. Adaptadores generados por Clang para metodos heredados y virtuales, tambien de bases no primarias y virtuales; overrides (T12-004, T12-005, T12-006, T12-008, T12-009).
3. Conversion de puntero de derivado a base publica (T12-003), con el ajuste de Clang.
4. Destruccion mediante base con destructor virtual, RTTI y conversiones dinamicas: en la biblioteca, probados con una DLL que enlaza su CRT (T12-007, T12-010, T12-011).
5. Fixture integral frente a un consumidor C++ equivalente (T12-012), validacion agrupada y cierre.

Cada capacidad se compara con un control compilado por Clang. Se prueba en estatica y DLL, de O0 a O3.

Fuera: herencia de un tipo C++ desde NativeSharp, constructores de records con bases virtuales, plantillas precompiladas (T13-001).

Presupuesto inicial: ClangSession.cpp, CppInteropSession.h, CppRecordDeclaration.h, CppSemanticFunction.cpp, CppSemanticRecord.cpp, ProjectMemberModel.cpp, conversiones de tipos del frontend, DriverArtifactGeneration.cpp, fixtures pruebas-tanda-12-* y documentacion.
