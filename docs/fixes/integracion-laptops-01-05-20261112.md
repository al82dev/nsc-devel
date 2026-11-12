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

## Actualización de integración

Se consultaron nuevamente los cinco heads remotos: 01 `c11fcd7b`, 02
`acf6ab8f`, 03 `e2467ff6`, 04 `ae5164d1` y 05 `f3960652`. No habían cambiado;
la comparación de parches confirma que sus commits están en la integradora.
Los commits de numéricos y ASan disponibles también están incorporados. Los
commits de infraestructura de vistas `25f554848`, `1ce3f0252` y `fb4632c2b`
se incorporaron con resolución de conflictos como `8ae189705`, `e7d6c0d90`
y `f4d4ac479`; se conservaron las pruebas de ambos lados. Adicionalmente se
incorporaron los controles de vistas binarias `a1f9069f4`, `f4446fb5c` y
`7e7211344`. Sus resultados históricos provienen de otro compilador WIP y
no certifican el HEAD integrado.

Los cambios `db2ef97cf` (layout SIMD), `676eed63d` y `8b128ac6b` (fábricas
compartidas), `acf568799`, `2cb01ab30` y `317788ad8` (excepciones async) se
integraron como trabajo en curso. No se ha compilado ni ejecutado la versión
integrada de estos cambios: por instrucción del usuario, la compilación
incremental y las pruebas dirigidas se aplazaron hasta integrar el conjunto.
La construcción binaria de base que puede lanzar y los cleanups async con
excepción siguen abiertos. Los árboles de campos y vistas conservan cambios
locales sin registrar que aún no se han incorporado.

Posteriormente se incorporó el trabajo de campos: `f50e695a1` como
`2c96b2ad4` (construcción genérica binaria) y `c6357603d` como `94d35b132`
(`Fields`, sus pruebas y registro). Este último se aplicó sin conflictos
textuales; las zonas compartidas con vistas requieren revisión y validación
de conjunto. La evidencia del worker corresponde a su propia copia y no
certifica estos commits integrados. La tarea de campos continúa abierta por
los límites anotados en su registro.

Los runners `static_field_source_fields.py` y `static_field_binary_fields.py`
no están registrados en CTest y requieren invocación dirigida explícita. El
unitario `NativeSharp3T11TypedIntrospectionTests` puede omitir dos controles
de certificados si faltan sus proyectos: la opción `--inherited-certificate`
del runner binario prepara ambos y trata esas omisiones como fallos. Deben
usarse el compilador, el unitario y los temporales aislados en A; un retorno
77 es una omisión, no un pase. Esta revisión es estática, sin ejecución.
