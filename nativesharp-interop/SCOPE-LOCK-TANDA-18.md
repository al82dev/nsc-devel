# Alcance de la tanda 18

Base: `2dcb699d5`. [Auditoría inicial](TANDA-18-AUDITORIA-INICIAL.md).

Cambios, en este orden:

1. Constructores C++ que pueden lanzar en locales NativeSharp (T18-011): sin la exigencia de `noexcept` en el
   constructor; el destructor sigue sin poder lanzar.
2. Línea base e inventario de características, reglas, evidencia y combinaciones (T18-001 a T18-007).
3. zlib 1.3.2 consumida desde NativeSharp (T18-008, T18-009).
4. ICU4C `icuuc` consumida desde NativeSharp: `ParsePosition`, `Edits`, `ErrorCode` (T18-010, T18-011).
5. Especializaciones de la STL (T18-012, T18-013).
6. Regresión completa de NSC, de interoperabilidad estática y DLL (T18-014 a T18-016).
7. Distribución, entorno limpio, requisitos y reproducción (T18-017 a T18-020).
8. Pendientes, informe final y criterio de terminación (T18-021 a T18-023).

Cada biblioteca se compara con un control compilado por Clang que hace las mismas llamadas, en estática y DLL
cuando la biblioteca tiene parte binaria, de O0 a O3.

Fuera: descargar bibliotecas de la red; desplegar en `C:`; el runtime dinámico de MSVC.

Presupuesto inicial: ProjectCompilation.cpp, fixtures pruebas-tanda-18-* y documentación.
