# Alcance de ejecucion: tanda 11

Base: development-cpp-clang 7c7d76fcf (cierre de la tanda 10), arbol limpio al inicio.

Objetivo: completar T11-001 a T11-022 (clases C++ y vida de objetos) con auditoria, implementacion, pruebas dirigidas, validacion agrupada, documentacion y commits locales. El usuario autoriza las decisiones de diseno. Direccion: bibliotecas C++ consumidas desde NativeSharp.

Restricciones: Clang oficial 23.1.0. No se modifican Clang ni NSLink sin autorizacion posterior y necesidad demostrada. No se despliega en C:, ni se publica o reescribe la rama remota. NSMeta mantiene un unico contrato vigente, sin versiones historicas. Daikon y el camino inverso quedan fuera. Se conservan las pruebas existentes; un negativo que un cambio deje obsoleto pasa a comparacion positiva contra Clang, con un rechazo genuino separado.

Auditoria inicial: [TANDA-11-AUDITORIA-INICIAL.md](TANDA-11-AUDITORIA-INICIAL.md), con la sonda pruebas-tanda-11-auditoria/sonda.py (evidencia A:/temp/nsc-t11-sonda-fp52zaz2).

Plan de cambios, en orden; cada uno con su registro y commit:

1. Especificacion de excepciones resuelta con Sema: destructores y miembros implicitos noexcept (T11-007, T11-013).
2. Nombres de miembro que son palabras reservadas de NativeSharp (get, set) tras '.' y '->' (T11-005).
3. Metodos de instancia con enums, records triviales por valor, referencias lvalue y retorno por referencia (T11-005, T11-010).
4. Acceso de campos, metodos, constructores y destructor de instancia, con diagnostico explicito (T11-004, T11-005, T11-006, T11-007).
5. Constructores con enums, punteros, records triviales y referencias; los constructores de copia y movimiento se clasifican sin descartar la familia (T11-006).
6. Copia: un record trivialmente copiable se copia; un constructor de copia accesible se invoca de forma explicita; uno eliminado o inaccesible se rechaza (T11-015, T11-017).
7. Movimiento con constructor de movimiento accesible (T11-016).
8. Operadores miembro sobre expresiones NativeSharp (pendiente de T10-016).
9. Representacion, tamano y alineacion, campos por layout, construccion y destruccion, documentados y probados con contadores (T11-002, T11-003, T11-008, T11-009, T11-011, T11-012, T11-013, T11-020).
10. Objetos dinamicos y punteros no propietarios (T11-018, T11-019), construccion parcial (T11-014) y propiedad con DLL (T11-022).
11. Fixture integral de copia y movimiento no triviales (T11-021), validacion agrupada y cierre.

Cada capacidad se compara con un control compilado por Clang. Se prueba en estatica y DLL, de O0 a O3 y, donde aplique, desde plantilla precompilada con y sin encabezado.

Fuera: constructores que pueden lanzar en locales y la limpieza durante el desenrollado (tanda 15); herencia y despacho (tanda 12); plantillas (tanda 13); inicializacion global y CRT (tanda 16).

Presupuesto inicial: ClangSession.cpp, CppSemanticFunction.cpp, CppSemanticRecord.cpp, ProjectMemberModel.cpp, OwnershipQueries.cpp, ProjectCompilation.cpp, ExpressionParser.cpp, PostfixIterationMachine.cpp, emisores HIR/LLVM afectados, CppCRecordMetadataCodec.cpp, fixtures pruebas-tanda-11-* y documentacion.

T11-007 y T11-013, implementacion 2026-11-15 (destructores noexcept implicitos): causa confirmada en codigo con la sonda. collectRecords tomaba solo especificaciones de excepciones resueltas; la de un destructor sin noexcept queda EST_Unevaluated y NSC la trataba como lanzadora. Resolverla con Sema como en los miembros implicitos. Al resolverla, un destructor por defecto en la clase no tenia definicion (emitCppInlineIR excluia constructores y destructores) y uno trivial se llamaba sin existir en la DLL: emitir con Clang los constructores y destructores inline o por defecto, y no llamar a un destructor trivial. Fuera: constructores lanzadores (tanda 15) y plantillas precompiladas. Presupuesto: ClangSession.cpp, CppRecordDeclaration.h, CppSemanticFunction.h/.cpp, OopLlvmCleanup.cpp, fixture pruebas-tanda-11-destructores y documentacion.

T11-005 y T11-010, implementacion 2026-11-15 (metodos de instancia): causa confirmada en codigo con la sonda. projectCppSemanticInstanceMethod no tenia busqueda de nombres ni referencias, el modelo de miembros solo admitia escalares y punteros, y el analizador exigia un identificador tras '.'. Mapear enums y records triviales por valor y referencias lvalue como en una funcion libre, ligar el retorno ref a this y aceptar palabras reservadas como nombre de miembro. Al comparar con Clang aparecio que el MS ABI devuelve en memoria todo tipo clase desde un metodo de instancia: hacerlo asi en MIR y en la llamada. Fuera: retornos const T&, T&&, virtuales y plantillas precompiladas. Presupuesto: CppImportedInstanceMethod.h, CppSemanticFunction.h/.cpp, ProjectMemberModel.cpp, ExpressionParser.cpp, MirFunctionLowerer.cpp, OopLlvmCalls.cpp, fixture pruebas-tanda-11-metodos y documentacion.

T11-006, T11-015, T11-016 y T11-017, implementacion 2026-11-15 (constructores, copia y movimiento): causa confirmada en codigo con la sonda. projectCppSemanticLifetimeMethod no tenia busqueda de nombres ni referencias y rechazaba los constructores no publicos; la familia de constructores se descarta entera si uno no se proyecta, asi que un constructor eliminado, de movimiento o privado ocultaba a los demas; tres comprobaciones repetian la restriccion a escalares. Mapear los parametros como en un metodo, importar el acceso, saltar los eliminados y los que toman T&&, proyectar el constructor de movimiento como ref cuando no hay constructor de copia invocable, y aceptar esos parametros en el modelo de miembros, la vida experimental y MIR. Fuera: copia implicita, constructores lanzadores y records con bases. Presupuesto: CppImportedInstanceMethod.h, CppSemanticFunction.h/.cpp, ProjectMemberModel.cpp, ProjectCompilation.cpp, MirNodeMachine.cpp, fixture pruebas-tanda-11-constructores y documentacion.

T11-002, T11-003, T11-008, T11-009, T11-011, T11-012, T11-018, T11-019 y T11-022, 2026-11-15 (objetos y propiedad): sin cambios de produccion. Fixtures pruebas-tanda-11-objetos (layout alignas(32) con relleno, construccion y campos) y pruebas-tanda-11-propiedad (objetos dinamicos de la biblioteca, punteros no propietarios y DLL con CRT), y documentacion.
