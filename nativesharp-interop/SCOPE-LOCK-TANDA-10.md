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

T10-010, implementacion 2026-11-14 (argumentos predeterminados): causa confirmada en codigo con la sonda. ClangSession solo guardaba hasDefaultArgument y projectFunction no asignaba defaultValueSyntax, asi que todos los parametros eran obligatorios. Evaluar el predeterminado con Expr::EvaluateAsRValue y conservarlo si es una constante del tipo del parametro; crear el literal NativeSharp equivalente que materializa lowerDefaultArgument; transportar tipo y bits en el byte del catalogo que guardaba la marca (contrato unico vigente). Fuera: enums, records y predeterminados no constantes. Presupuesto: CppFunctionDeclaration.h, ClangSession.cpp, CppSemanticFunction.cpp, CppSemanticRecord.cpp, CppCRecordMetadataCodec.cpp, fixture pruebas-tanda-10-predeterminados y documentacion.

T10-012 y T10-006, implementacion 2026-11-15 (definiciones inline emitidas por Clang): causa confirmada con la sonda (error de enlace por ?sq@A@@YAHH@Z y ?k@S@A@@2HB). Una funcion o variable inline de C++ se define en cada unidad que la usa y la biblioteca no la exporta; NSC importaba la declaracion sin aportar la definicion. Ofrecer a cada sesion C++ los simbolos que importa el programa y emitir con CodeGen las definiciones inline encontradas, con weak_odr y COMDAT, como emitImplicitLifetimeIR. Fuera: dllimport inline, plantillas, constructores y destructores. Presupuesto: CppInteropSession.h, ClangSession.cpp, CppLifetimeObjectEmitter.h/.cpp, DriverArtifactGeneration.cpp, fixture pruebas-tanda-10-inline y documentacion.

T10-002, implementacion 2026-11-15 (nombres alternativos de namespace): causa confirmada en codigo con la sonda. ClangSession guardaba un solo nombre calificado (impreso sin inline namespaces), la seleccion por demanda solo buscaba ese nombre y el indice de funciones de namespace registraba un solo namespace. Calcular la ruta con inline namespaces, los alias de namespace que alcanzan la funcion y las declaraciones using de namespace; seleccionar por cualquiera de ellos e indexar el mismo ordinal bajo cada namespace, sin duplicar el simbolo; transportarlos en el catalogo. Fuera: variables, tipos, using namespace. Presupuesto: CppFunctionDeclaration.h, NamespaceFunctionSymbol.h, ClangSession.cpp, CppSemanticFunction.cpp, CppSemanticFunctionGroups.cpp, ProjectNamespaceFunctionModel.cpp, CppCRecordMetadataCodec.cpp, fixture pruebas-tanda-10-namespaces y documentacion.

T10-005 y T10-002, implementacion 2026-11-15 (tipos anidados en records): causa confirmada en codigo con la sonda. prepareCppRecordObservation aplazaba todo record con contenedores; el parser recibia el nombre de identidad con + entre contenedores; collectEnums no entraba en records C++ y la identidad de enum tomaba cada calificador como namespace; los metodos estaticos se proyectaban sin busqueda de nombres. Importar los records publicos anidados cuya cadena contenedora es importada y publica, dar al parser la escritura con puntos, recolectar enums publicos de records y separar contenedores de namespaces, nombrar sus enumeradores por el tipo, mapear enums en metodos estaticos y reconciliar el simbolo compartido entre catalogo y consumidor por identidad de Clang. Fuera: inyeccion de enumeradores en el record, plantillas y records anonimos, records por valor en metodos estaticos. Presupuesto: CppSemanticRecord.cpp, CppParserProjectInventory.h, CppType.h, CppEnumDeclaration.h, CppSemanticEnumType.h, ClangSession.cpp, CppSemanticInventory.cpp, CppSemanticFunction.h/.cpp, ProjectMemberModel.cpp, CppCRecordMetadataCodec.cpp, fixture pruebas-tanda-10-anidados y documentacion.
