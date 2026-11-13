# Operaciones de punteros compartidos con receptor de solo lectura

En la revisión `c2810b371`, el compilador rechazaba `Clone` y `Upgrade` al
recibir `Rc` o `WeakRc` por `in`, porque sus métodos carecían del calificador
`readonly`. La reproducción está en
`A:\nsc-fixes-evidencia\shared-clone-in-check-20261113\O0-compile.log`.
`IsEmpty` de `Rc` fallaba por la misma razón en
`A:\nsc-fixes-evidencia\shared-empty-in-before-20261113\O0-compile.log`.

Los métodos de consulta y creación de dueños correspondientes ahora declaran
el receptor de solo lectura en `Rc`, `WeakRc`, `Arc`, `WeakArc` y sus handles
internos. Las operaciones de clonación, degradación y promoción siguen
actualizando los contadores y creando un dueño nuevo; no modifican los campos
del receptor ni permiten escribir el objeto a través de `Get`. El conteo débil
de Arc sigue siendo una observación aproximada durante operaciones concurrentes.

`tests/integration/shared_readonly_clone_binary.py` construyó bibliotecas Core
mínimas desde esta copia y consumidores para ambas familias, con productor y
consumidor en O0 y O2: ocho ejecuciones devolvieron 42, incluidos estados
vacío, vivo y expirado, conteos, supervivencia y destrucción única. Las cuatro
comprobaciones negativas rechazaron escritura por `Get`. Comandos y resultados:
`A:\nsc-fixes-evidencia\shared-readonly-clone-isempty-20261113.log`. Esta
prueba no reconstruye ni acredita la biblioteca Core completa.

La prueba dirigida `shared_arc_binary_upgrade_race.py` pasó O0 y O2 usando el
Core mínimo anterior; salida en
`A:\nsc-fixes-evidencia\shared-readonly-isempty-thread-20261113.log`. No cubre
todas las intercalaciones ni descarga de módulos. La prueba de restricciones
de hilo pasa con Core fuente, pero cuatro variantes con `Arc<Arc<Safe>>` o
`Arc<WeakArc<Safe>>` fallan al consumir Core binario por resolución de
`DropStrong`; el mismo fallo aparece con un Core anterior a este cambio.
Evidencia: `shared-readonly-clone-constraints-source-20261113.log`,
`shared-readonly-clone-constraints-20261113.log` y
`shared-readonly-clone-constraints-control-20261113.log` en
`A:\nsc-fixes-evidencia`. Ese defecto permanece pendiente.
