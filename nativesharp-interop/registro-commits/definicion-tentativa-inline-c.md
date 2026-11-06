# Definiciones tentativas C en el módulo inline

El primer enlace dejó `shared_state` sin definición porque la generación aislada no notificaba las definiciones tentativas. Se usa la definición activa indicada por Clang y `CompleteTentativeDefinition`, preservando el estado interno compartido. Se vuelve a validar el grupo completo.

## Evidencia del 2026-11-06

- Construcción incremental de `nsc` y `nsc_cpp_session_tests`: correcta, build `3aa5707a6d2b2cc7`.
- `pruebas-tanda-08-inline-c/validar.cmd`: pasó en O0, O1, O2 y O3, tanto para el ejecutable directo como para la biblioteca NativeSharp precompilada y su consumidor. Incluye llamadas a funciones internas, estado local estático y estado estático compartido.
- `ctest --test-dir A:\nsc-interop-build\tanda1-ON -R nsc.cpp-interop.session --output-on-failure`: 1/1 pasó.
- `pruebas-tanda-08-prototipos-c/validar.cmd`: pasó en O0–O3 y sus comprobaciones negativas; Clang emitió advertencias sobre declaraciones sin prototipo.

Esta evidencia cubre los casos de esas pruebas; no cierra la tanda 08 ni demuestra todas las combinaciones de funciones C inline. El descenso actual de objetos generados rechaza ThinLTO y requiere validación adicional para configuraciones con sesiones y módulos múltiples.
