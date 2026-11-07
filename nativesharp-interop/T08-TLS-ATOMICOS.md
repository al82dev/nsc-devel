# TLS C atómico: trabajo pendiente

El control `pruebas-tanda-08-tls-c/atomic` cubre un global
`_Thread_local _Atomic(int)` que Clang compila y NSC rechazaba inicialmente.
Actualmente NSC admite un primer subconjunto de operaciones. La aceptación no
debe introducir accesos no atómicos ni tratar `+=` como carga más escritura:
esa actualización debe ser una operación indivisible. El fixture comprueba el
resultado y el verificador de IR asocia las operaciones comprobadas al global
importado. Esa comprobación todavía no demuestra todas las operaciones posibles.

## Cadena que necesita un contrato explícito

1. `ClangSession.cpp::describeType` debe capturar `clang::AtomicType` y su tipo
   subyacente sin confundirlo con `volatile`, incluyendo tamaño y alineación.
   `CppType` y los codecs que serializan tipos necesitan conservar el dato con
   una versión compatible; los formatos antiguos no deben adquirir atomicidad.
2. `CppSemanticInventory.cpp` debe conciliar redeclaraciones y proyectar el
   contrato hacia `NamespaceStateSymbol`, con identidad y alineación conocidas.
   Debe distinguir almacenamiento atómico de puntero a objeto atómico; los
   tipos o alineaciones no soportados deben continuar rechazándose.
3. El análisis de expresiones, HIR y MIR deben preservar atomicidad en lectura,
   asignación, operadores compuestos, incremento, direcciones y sus escapes.
   Los caches de HIR/MIR y sus identidades también necesitan reflejarla. No es
   correcto heredar el modo `volatile` de Microsoft para representar `_Atomic`.
4. El descenso LLVM debe emitir lectura y escritura `seq_cst`, y `atomicrmw` o
   un bucle `cmpxchg` de orden adecuado para actualizaciones. Conversiones,
   valores retornados por pre/post incremento y posibles operaciones no
   soportadas deben mantener semántica C; no bastan dos accesos atómicos.
5. La publicación precompilada debe conservar el contrato y sus dependencias;
   comprobar productor y consumidor independientes, con y sin header, así como
   biblioteca estática y DLL. Los punteros escapados deben preservar el contrato
   atómico o rechazarse antes de perderlo.
6. La matriz debe comparar IR específico del global y los resultados con Clang
   en O0–O3, incluir accesos entre hilos mediante un puntero al objeto TLS e
   invalidación de caché. Repetir el gate de la tanda al agrupar correcciones.

El primer fixture cubre solo `int`, pero no es la definición final del alcance:
faltan otros enteros, punteros, agregados y los órdenes explícitos de las API
atómicas de C. No está decidido aún cómo representar esos contratos en la API
NativeSharp. T08 permanece abierta.

## Caché de artefactos y referencias C/C++

`PrecompiledModuleConfiguration.cpp` rechaza la caché de artefactos si el
grafo contiene `CppLibraryReference`, mientras no se rastreen encabezados
transitivos. El control atómico comprueba el diagnóstico con una caché
explícita; las ejecuciones O0–O3 usan `--no-artifact-cache`. No hay evidencia de
reutilización de caché para ese proyecto. Una biblioteca NativeSharp precompilada
que publique atómicos requiere verificación independiente de su metadata y de
la caché del consumidor; no se infiere de esta prueba.
