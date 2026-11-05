# Tanda 03: contrato público de sesión Clang

Estado: **contrato público definido y validación agrupada aprobada el 2026-11-05**. La implementación interna y
la cobertura de interoperabilidad permanecen en las tandas posteriores.

`CppSessionOptions.h` define modo C/C++, estándar solicitado, triple, ruta de referencia del driver, directorio
de recursos, directorio de trabajo, inclusiones ordenadas con clase usuario/sistema y macros ordenadas de
definición o eliminación. La entrada es una variante: ruta física o ruta virtual con contenido poseído. El
entorno de headers del sistema se representa mediante inclusiones `System`.

`cppSessionConfigurationIdentity` serializa con campos y longitudes explícitas, versión `v1`, rutas en su
representación genérica UTF-8 sin resolver ni normalizar `..`, orden de listas y contenido de entrada virtual, y
devuelve SHA-256 hexadecimal. Es una
identidad de los valores de opciones: **no** incluye contenido del archivo físico, headers transitivos, binarios
del toolchain ni sondeos de headers ausentes, y por ello no es una clave suficiente para reutilizar caché.

`CppSessionDiagnostic.h` transporta severidad, mensaje, identificador original opcional y punto opcional con
archivo, línea y columna; los diagnósticos de nota asociados conservan sus propios puntos en la lista `notes`.
No se afirma que el contrato represente todos los rangos, arreglos automáticos o relaciones posibles de Clang.

`CppInteropSessionFactory.h` recibe opciones poseídas y un token de cancelación compartido. La apertura retorna
una variante definida en `CppInteropSession.h`: sesión única lista o fallo con fase, mensaje y diagnósticos. El token comparte un indicador atómico;
la cancelación solicitada no implica que cualquier operación ya iniciada termine inmediatamente.

`CppInteropSession.h` expone estados `Ready`, `Closing` y `Closed`; `close()` retorna resultado y es idempotente.
Las instantáneas devueltas son valores poseídos que pueden sobrevivir al cierre. Las consultas dependientes del
AST indican indisponibilidad con `nullopt`; no se entrega propiedad del AST ni tipos internos de Clang.

La implementación `ClangSession` serializa llamadas de cada sesión viva con su mutex y bloquea ambas sesiones
en la comparación estructural. El llamador debe mantener vivo el objeto durante cada llamada y coordinar su
destrucción. No se promete seguridad de concurrencia para otras implementaciones ni para aperturas simultáneas
de fábrica sin evidencia adicional.

Validación agrupada: construcción incremental ON (`nsc` y `nsc_cpp_session_tests`) y OFF (`nsc`) con código 0;
repetición incremental de ON tras agrupar la corrección de rutas e inicializadores, código 0. La suite
`nsc_cpp_session_tests.exe A:/al82dev-clang/lib/clang/23 A:/nsc-interop-build A:/al82dev-clang/bin/clang.exe`
terminó con código 0: incluye las pruebas de identidad por campo, entrada y orden junto con las pruebas
existentes de sesión, cancelación y cierre. `verificar-interfaces-tanda1.cmd` pasó con `/W4 /WX`; una búsqueda
de includes `clang/` en `include/nativesharp/Interop` no encontró resultados. Las notas y ubicaciones están
definidas en el contrato; esta validación no afirma cobertura de todas las formas de diagnóstico de Clang.
La revisión posterior detectó que el resultado de apertura estaba en el encabezado de fábrica; se trasladó sin
cambiar sus tipos ni namespace a `CppInteropSession.h`. El fixture comprueba su disponibilidad antes de incluir
la fábrica, y `verificar-interfaces-tanda1.cmd` volvió a pasar con `/W4 /WX`, sin reconstrucción general.
