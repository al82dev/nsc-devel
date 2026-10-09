# Tanda 01.11 — Seed de stage de Host.Basic

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Componente | `Host.Basic` |
| Contrato | `CB-BAS-016`, seed de un artefacto publicado hacia un stage aislado |
| Casos | `TB-BAS-083` a `TB-BAS-088` |
| Código de producto | Sólo compilar y observar `ArtifactTransaction.cpp`; no modificar |
| Pruebas históricas | Las 684 quedan protegidas; no modificar ni ejecutar |
| Configuraciones | Debug y Release del ejecutable consolidado |
| Acciones prohibidas | NSC, NSLink, baseline, cobertura, despliegue, remoto y `C:` |

## Contratos ejercitados

`TB-BAS-083` demuestra el cold path: una fuente ausente produce éxito, limpia un error previo y no fabrica el
stage. `TB-BAS-084` copia una fuente regular con NUL y bytes fuera de ASCII y compara el contenido completo.
`TB-BAS-085` ejercita por separado fuente vacía y stage vacío y exige el diagnóstico estable exacto.

`TB-BAS-086` pasa un directorio como fuente. En Windows la inspección sólo determina presencia; la copia
posterior lo rechaza con un error clasificado que nombra el stage y no publica un archivo. `TB-BAS-087` entrega
un stage cuyo padre todavía no existe y comprueba la creación del directorio y la copia byte por byte.

`TB-BAS-088` cubre las tres familias de fallo del contrato:

- la inspección se rechaza con `ERROR_ACCESS_DENIED`, exige `permission-denied` y no crea el stage;
- la copia usa un directorio real ocupando la ruta del stage, por lo que `CopyFileW` falla sin simulación;
- el flush se rechaza con `ERROR_WRITE_FAULT`, exige `io-error` y confirma que los bytes copiados permanecen en
  el stage aunque la operación no declare éxito durable.

## Inyección acotada de fallos

Inspección y flush no tienen una reproducción de filesystem determinista que sea independiente de permisos,
antivirus o configuración de la máquina. `ScopedSeedFailure` modifica temporalmente una única celda de la tabla
de imports del ejecutable de prueba. La selección se limita al basename del fixture; las demás llamadas delegan
a la función original.

La búsqueda del import es iterativa y cuesta O(símbolos importados) una vez. La comparación del basename no
asigna memoria. El buffer usado para consultar el nombre de un handle es `thread_local`, no recursivo ni alojado
en el stack de cada llamada. La restauración ocurre antes de cualquier aserción o salida del runner. No se
alteran DLL del sistema, otros procesos, dispositivos DOS ni código de producto.

## Integración

- `BasicArtifactSeedModule.cpp` registra los seis casos en el host consolidado.
- `WriterSeedFailure.h` contiene el único tipo nuevo de soporte para fallos de inspección y flush.
- El manifiesto conserva perfil, clase, entradas y oráculo por ID.
- El gate dedicado selecciona `CB-BAS-016` con perfil `NIGHTLY` y exige `TB-BAS-083..088` en orden.
- El gate PR general incorpora `TB-BAS-083..085` y `TB-BAS-087`.
- CMake conserva un único ejecutable host y añade el décimo gate de controlador.

## Ownership, limpieza y complejidad

Cada callback posee su entorno temporal, árbol de fixture y archivos. La limpieza es iterativa: hojas y
directorios se registran de forma explícita y se eliminan de mayor a menor longitud. Los handles Win32 se
cierran mediante `Win32Handle`. El hook no posee recursos externos y nunca existe más de uno activo.

Las rutas nominales realizan O(n) tiempo y memoria para n bytes copiados. Las comprobaciones de presencia y
errores son O(longitud de ruta). El fallo de copia usa únicamente filesystem real y el fallo de flush conserva
el stage para verificar el estado observable exacto de la implementación vigente.

## Evidencia de cierre

- Gate dirigido `CB-BAS-016`: 6/6 aprobado en Debug y Release.
- Pruebas Python del controlador: 51/51 aprobadas.
- Debug: build con warnings como errores y CTest de la suite nueva 10/10 aprobado.
- Release: build con warnings como errores y CTest de la suite nueva 10/10 aprobado.
- Análisis `clang-tidy` con `clang-analyzer-*` y `bugprone-*`: 0 hallazgos en código propio; 283 advertencias de
  dependencias o cabeceras del sistema fueron identificadas y suprimidas por la herramienta como código ajeno.
- Diferencial de código de producto bajo `src` e `include/nativesharp`: 0 archivos.
- Recursión añadida: 0.

La validación fue exclusiva de la suite nueva. No se ejecutaron el compilador NativeSharp, las 684 pruebas
históricas, minibaselines, baseline, cobertura, instalación ni despliegue.
