# Validación 250: nota de declaración previa nula y presente

## SCOPE LOCK

- Objetivo: materializar únicamente `TS-SEMA-101`.
- Naturaleza: prueba unitaria dirigida de un helper privado existente.
- Producto protegido: compilador, semántica, API, ABI y construcción de NSC.
- Pruebas protegidas: todos los casos distintos de `TS-SEMA-101`.
- Archivos autorizados: `c101`, una seam test-only estrecha, el shim de Sema y
  este documento.
- Acciones prohibidas: manifiesto, CMake, build, ejecución y cambios de producto.
- Terminación: observar cero notas para entradas nulas y una nota exacta para
  una declaración existente.

## Caso

La seam se limita a invocar `SemanticAnalyzer::reportPreviousDeclaration` desde
el target de componentes. No replica el algoritmo ni altera la declaración del
producto.

El caso cubre tres entradas: símbolo nulo, símbolo con `declaration == nullptr`
y símbolo con una declaración real. Las dos primeras deben dejar vacío el
engine. La tercera debe emitir exactamente una nota `PreviousDeclaration`, con
severidad `Note` y texto `previous declaration is here`.

## Evidencia

- Caso: `tests/compiler-suite/host/modules/SemaAnalyzerLifecycleModule.cpp`.
- Seam: `SemaInfrastructureTestSupport.h` y `SemaProductTestShim.cpp`.
- Contrato: `CS-SEMA-012`.
- Validación ejecutada: ninguna, por prohibición expresa.
- Integración pendiente: fila del manifiesto; el caso conserva
  `InfrastructurePending`.
