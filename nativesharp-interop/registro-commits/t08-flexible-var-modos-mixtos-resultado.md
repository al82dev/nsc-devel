# T08-010/015: referencias del mismo encabezado en modos divergentes

`diagnosticar-var-volatile.cmd` del commit `857d3d0d1` terminó con código 1
con NSC build `5c1e6c7fa15d8a3e` y Clang 23.1.0. Comparó lectura y escritura
directas y mediante alias local, O0–O3, con alineación 4 en todos los casos:

- Con una sesión Microsoft, el acceso directo coincide con acquire/release;
  el alias emite `volatile` sin orden y difiere.
- Con una sesión ISO, ambos accesos coinciden con `volatile` sin orden.
- Con ambas sesiones sobre el mismo encabezado, ambos accesos emiten
  `volatile` sin orden: coinciden con ISO y difieren de Microsoft.

La observación de `CppSemanticRecord.cpp` conserva modo desconocido si las
sesiones del mismo record discrepan. El resultado ilustra que el modo capturado
por referencia no define unívocamente el modo de la unidad NativeSharp; tampoco
acredita concurrencia ni todas las combinaciones de proyectos y metadata.
Los IR generados permanecen en `A:/nsc-interop-build/tanda08-flexible-var-volatile/`.
No se cambió producción y el problema sigue abierto.
