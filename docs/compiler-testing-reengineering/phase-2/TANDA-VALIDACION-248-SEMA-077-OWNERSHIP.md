# Validación 248: ownership del grafo internado de tipos

## SCOPE LOCK

- Objetivo: materializar únicamente `TS-SEMA-077`.
- Naturaleza: prueba de infraestructura con oráculo sanitizer.
- Producto protegido: compilador, semántica, ABI y construcción de NSC.
- Pruebas protegidas: todos los casos distintos de `TS-SEMA-077`.
- Archivos autorizados: el cuerpo existente de `c077` y este documento.
- Acciones prohibidas: manifiesto, CMake, build, ejecución y cambios de producto.
- Terminación: el caso construye un grafo internado real y deja que el sanitizer
  certifique su destrucción sin fuga ni doble liberación.

## Caso

El fixture crea un struct nominal, su puntero, un arreglo del puntero, una firma
de función y una calificación `Owned`. Todos pertenecen a un solo `TypeContext`.
Al salir del scope, `TypeContext` destruye el grafo completo.

El oráculo funcional comprueba que el grafo fue construido. El oráculo de
ownership pertenece al perfil ASan/LSan: cualquier fuga, doble liberación o uso
inválido debe terminar la corrida. El caso permanece `InfrastructurePending` y
fuera del manifiesto ejecutable hasta integrar ese perfil.

## Evidencia

- Implementación: `tests/compiler-suite/host/modules/SemaTypeSystemModule.cpp`.
- Contrato: `CS-SEMA-009`.
- Validación ejecutada: ninguna, por prohibición expresa.
- Integración pendiente: perfil sanitizer y fila del manifiesto.
