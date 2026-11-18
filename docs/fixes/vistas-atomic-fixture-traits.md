# Dependencias del fixture de concurrencia de punteros

## Validación en la rama integradora

El commit de origen `3a99f2d47` se integró como `e0840f127`. El runner se
ajustó para ejecutar los procesos desde la raíz de la copia integradora,
conservando los proyectos y resultados temporales en A. Con
`A:/nsc-fixes-evidencia/build/bin/nsc.exe` SHA-256
`d6f7c8e534461a4df3394ae98d09a6ba9e26ebc24a3dc7de72a53efba88b38d1`,
las variantes default, last-strong y weak-clone devolvieron 42 en O0/O2;
el control de familia local se rechazó en análisis como se esperaba.
Los logs `A:/nsc-fixes-evidencia/vistas-arc-race-*-20261112.log` conservan
las salidas. Son pruebas dirigidas de las intercalaciones del fixture y no
demuestran la ausencia de carreras para todos los programas o plataformas.

El fixture `tests/integration/shared_public_atomic_race.py` construye un
proyecto aislado copiando las fuentes Core necesarias. En la revisión inicial
`3b5df359a2d34992a99e4ae292d798e4d213f618`, `Arc` y `WeakArc` requieren los
traits `Send` y `Sync`, pero el fixture no copiaba sus declaraciones. Por ello
el análisis fallaba antes de ejecutar cualquier comprobación de concurrencia.

La corrección añade `Send.ns` y `Sync.ns` a la lista de fuentes copiadas.
No cambia los tipos del SDK, las aserciones ni el comportamiento de los
contadores. El ejemplo y los fixtures originales de la campaña se conservan.

La comprobación usó el compilador de esta copia, SHA-256
`03d8616999e7d27590d28e29cfa62331af4b97a94428c2b09c38fb2a469b87fc`,
y fuentes Core de esta copia. Para respetar el directorio de ejecución exigido
se ejecutó una copia del fixture cuyo único cambio adicional fija el cwd de
los subprocesos a la raíz del repositorio. Su diff y comandos se conservaron
en `A:/nsc-vistas-evidencia/shared_public_atomic_race_fixed_cwd.diff` y
`A:/nsc-vistas-evidencia/shared-public-atomic-evidence.md`.

Las variantes `default`, `last-strong` y `weak-clone` terminaron con código 0;
sus programas devolvieron 42 en O0 y O2. El control `local-negative` también
terminó con código 0 tras comprobar el rechazo de `WeakRc` estático. Los logs
están en `A:/nsc-vistas-evidencia/shared-public-atomic-fixed-*.log`.
Este resultado cubre esas pruebas con fuentes Core; no demuestra toda la
seguridad concurrente ni la descarga dinámica del módulo productor.
