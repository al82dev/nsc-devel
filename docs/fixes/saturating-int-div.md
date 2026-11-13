# División saturada de int

En `81574e441`, `saturating(minimum / -1)` producía minimum por wrapping
en O0/O2. La reproducción y comandos se conservan en
`A:/nsc-fixes-evidencia/int-saturating-div-before-20261112.log`.

La división saturada int identifica ese par, emplea un divisor seguro para
evitar la operación LLVM inválida y selecciona el máximo. La guarda de cero
permanece anterior; el resto, división ordinaria y otras políticas y tipos
conservan su ruta. La reproducción original del reporte permanece intacta.

El build incremental terminó 0; compilador SHA-256
`535a0073ea9f94e0eb936882e8c3bac5fe0ae990fbab48aaad5a6932d8471899`.
En O0/O2 pasaron el par desbordado, división entre cero (trap 129), valores
representables, signos, división ordinaria y resto. Las cuatro pruebas
dirigidas de saturación int pasaron 4/4, con suma solo en O0; evidencia en
`A:/nsc-fixes-evidencia/int-saturating-div-ctest-20261112.log`. No acredita
otras plataformas ni otras operaciones o tipos. `CMakeLists.txt` se comparte
con la rama de interoperabilidad y requiere revisión al integrar. No se
ejecutó batería completa.
