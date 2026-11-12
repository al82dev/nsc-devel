# Integración de los cinco trabajos de laptop

Las ramas `development-fixes-limitaciones-laptop-01` a `-05` se consultaron
en el remoto y sus commits propios se incorporaron mediante cherry-pick con
procedencia. No se incorporaron ramas de interoperabilidad C/C++. La base
compartida de las cinco fue `71cd86d35`; los trabajos previos comunes no se
repitieron. Las instrucciones de cada worker se conservan con sus commits.

* 01, caso 99: la reproducción equivalente de Dictionary pasó desde fuente
  y con bibliotecas Core/Collections independientes en O0/O2. Un primer par
  de bibliotecas falló por incluir Core dentro de Collections; no se atribuye
  al cambio. El par que pasó proviene del checkout ASan anterior, no acredita
  un SDK recompilado del HEAD. El ejemplo histórico exacto no está disponible.
  Evidencia: `A:/nsc-fixes-evidencia/laptop-01-integrado-20261112/`.
* 02, caso 89: la matriz original confirmó el defecto de firmas binarias;
  los resultados y la corrección posterior se registran en su informe.
* 03, caso 73: los commits incluyen pruebas y una propuesta, no aplican los
  cambios propuestos de cancelación tardía ni de límites. Su prueba unitaria
  se registró y pasó 1/1 en CTest dirigido. La implementación original de
  `#run` ya estaba incorporada y se compararon sus cambios por patch-id.
* 04, casos 43, 68 y 69: siete controles dirigidos fuente y binarios pasaron;
  se probaron cruces O0/O2. No es una medición de rendimiento ni prueba todo
  el SDK binario. Evidencia: `A:/nsc-fixes-evidencia/laptop-04-integrado-20261112/`.
* 05, caso 102: pasaron el runner dirigido, fuentes explícitas y glob en O0/O2,
  y dos pruebas unitarias existentes sin reconstruir. Junction y dos symlinks
  se omitieron por permisos; el fixture es comparable, no el original completo.
  Evidencia: `A:/nsc-fixes-evidencia/case102-laptop05-recheck.md`.

El compilador y el runtime usados son los aislados en A y se identifican por
hash en los logs. Las validaciones no equivalen a una batería completa. Los
frentes locales de vistas y campos seguían trabajando con cambios sin commit;
esos cambios no se copiaron ni se consideraron integrados.
