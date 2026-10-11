# Validacion 252: mascaras exactas por ancho

## SCOPE LOCK

- Objetivo: materializar unicamente `TS-SEMA-281`.
- Naturaleza: prueba unitaria dirigida de un helper anonimo existente.
- Producto protegido: compilador, semantica, API, ABI y construccion de NSC.
- Pruebas protegidas: todos los casos distintos de `TS-SEMA-281`.
- Archivos autorizados: `TS-SEMA-281`, la seam test-only ya reutilizada, el shim
  de Sema y este documento.
- Acciones prohibidas: manifiesto, CMake, build, ejecucion y cambios de producto.
- Terminacion: comparar los seis anchos requeridos con valores exactos.

## Caso

La seam reutilizada llama al `maskForWidth` que ya vive en el namespace anonimo
de `SemanticAnalyzer.cpp`. No copia la formula ni modifica su enlace de producto.

El oraculo exacto es:

- ancho 0: `0x0`;
- ancho 1: `0x1`;
- ancho 8: `0xff`;
- ancho 32: `0xffffffff`;
- ancho 63: `0x7fffffffffffffff`;
- ancho 64: `0xffffffffffffffff`.

## Evidencia

- Caso: `tests/compiler-suite/host/modules/SemaConstantEvaluation281To298Module.cpp`.
- Seam: `SemaInfrastructureTestSupport.h` y `SemaProductTestShim.cpp`.
- Contrato: `CS-SEMA-027`.
- Validacion ejecutada: ninguna, por prohibicion expresa.
- Integracion pendiente: fila del manifiesto; el caso conserva
  `InfrastructurePending`.
