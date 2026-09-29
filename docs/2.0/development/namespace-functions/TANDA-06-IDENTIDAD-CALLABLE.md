# Tanda 6: identidad callable y mangling

## Objetivo

Asignar a cada función de namespace una identidad semántica completa y un nombre físico estable antes del lowering.

## Cambios

- Cada símbolo contiene `callableIdentityKey`, separado de la firma de sobrecarga.
- Cada símbolo contiene un `mangledName` determinista con prefijo exclusivo `__nsc_function_`.
- El nombre físico codifica módulo, namespace, nombre, parámetros, ownership, `ref`/`out`/`in`, variadicidad y convención externa.
- El prefijo y el propietario de namespace mantienen estas funciones físicamente separadas de métodos, constructores y destructores.
- La identidad no usa nombre de archivo, ordinal de fuente ni orden de recorrido.

## Archivos modificados

- `include/nativesharp/Frontend/ProjectNamespaceFunctionModel.h`: almacena las dos identidades.
- `src/Frontend/ProjectNamespaceFunctionModel.cpp`: construye claves y nombres físicos estables.
- `CMakeLists.txt`: registra la prueba focalizada.

## Archivos nuevos

- `tests/unit/NamespaceFunctionIdentityTests.cpp`: verifica namespaces, sobrecargas y permutación de archivos.
- Este documento.

## Mejora obtenida

Las fases de cuerpos, HIR, MIR, LLVM, caché y metadata podrán referirse al mismo callable sin reconstruir nombres ni depender de clases falsas. Dos funciones equivalentes conservan el mismo símbolo aunque cambie su archivo, y símbolos semánticamente distintos nunca colapsan por compartir nombre visible.

## Verificación específica

- Construcción incremental exclusiva de `nsc`: correcta.
- Construcción y ejecución exclusiva de `nsc_namespace_function_identity_tests`.
- Resultado: `PASS: 7 checks`.
- No se ejecutó baseline, CTest general, fixtures generales ni matriz de optimización.