# Retención débil durante la liberación atómica

`WeakArc` necesita impedir que un intento de upgrade recupere un payload
después de que el último propietario fuerte lo haya liberado. La prueba
`nsc.sdk.shared-atomic-upgrade-race` comprueba el control atómico interno con un
débil explícito que mantiene el bloque vivo durante todo el ensayo.

En cada nivel O0 y O2, 128 rondas coordinadas fuerzan que un hilo retenga un
fuerte antes de que el otro libere el suyo; después se comprueba que el último
fuerte destruye una sola vez y la siguiente retención falla. Otras 128 rondas
lanzan intentos de retención y liberación desde hilos distintos, verifican que
una retención nunca tenga éxito después de observar expiración, y comprueban
destrucción única, conteo fuerte cero y liberación del último débil.

La selección dirigida pasó 1/1 en Windows (`A:/nsc-fixes-evidencia/atomic-upgrade-ctest.log`);
la ejecución directa con sus fuentes y compilador aislados registró salida 42
en O0 y O2 (`shared-atomic-upgrade-race.log`). La segunda parte intenta crear
contienda, pero no garantiza que cada ronda se solape; la primera sí establece
el orden descrito. La prueba usa el control interno y punteros de prueba, no
demuestra que `Arc` o `WeakArc` públicos satisfagan `Send`/`Sync`, ni cubre la
vida del módulo productor o el consumo binario. En sistemas distintos de Windows
la prueba comprueba HIR pero omite la ejecución nativa.
