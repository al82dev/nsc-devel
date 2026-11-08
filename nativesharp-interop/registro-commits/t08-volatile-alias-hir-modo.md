# T08-010/015: modo consumidor en accesos HIR por alias

En la indexación y la desreferencia HIR, un puntero volatile con el contrato C
interno transporta el modo explícito del archivo consumidor. Una expresión sin
modo o sin contrato no recibe uno. Las rutas C directas ya existentes conservan
su tratamiento anterior.

Este paso no asigna una alineación atómica al alias: todavía debe demostrarse
en cada ruta antes de emitir acquire/release. La protección temporal sigue
impidiendo generar código con la propiedad explícita; las asignaciones,
parámetros, conversiones, bool C y cachés aún requieren integración y pruebas.
No se ha construido ni validado este cambio.
