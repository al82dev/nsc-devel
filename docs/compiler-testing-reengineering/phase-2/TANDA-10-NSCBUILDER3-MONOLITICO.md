# Tanda 10 — NSC Builder 3 monolítico

## SCOPE LOCK

### Objetivo exacto autorizado

Crear una interfaz gráfica nueva para preparar y ejecutar la suite de la Fase 2 con granularidad de ola,
componente, contrato y caso. La interfaz debe mostrar el inventario completo, el progreso de compilación de la
infraestructura, la ejecución individual y los resultados sin ocultar limitaciones ni defectos conocidos.

Toda la implementación nueva vivirá en un único archivo `nscbuilder3.py`. El carácter monolítico de este
orquestador fue autorizado expresamente por el usuario y prevalece para esta tanda sobre la preferencia general de
dividir módulos.

### Naturaleza del cambio

Infraestructura y experiencia de usuario. No cambia el compilador, el lenguaje, los contratos de la suite ni los
oráculos existentes.

### Comportamiento que debe permanecer idéntico

- Las 684 pruebas históricas y su infraestructura permanecen intactas.
- Los 9,587 casos canónicos, sus identificadores, contratos, disposiciones y oráculos permanecen intactos.
- `nscbuilder.py` y `nscbuilder2.py` permanecen intactos.
- Los seis archivos preexistentes protegidos permanecen intactos.
- Los gates, manifiestos e inventarios de Fase 2 se consumen como fuentes; no se reescriben en esta tanda.

### Revisión conocida y rango

- Base de implementación estática: `ccdb17541`.
- La tanda comienza sobre la rama local `development`.
- No se publican commits ni ramas en remotos.

### Presupuesto de cambio

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Componente | Orquestador gráfico NSC Builder 3 |
| Código | `nscbuilder3.py` únicamente; `.gitignore` sólo excluye su evidencia local |
| Documentación | Este documento, `phase-2/SCOPE-LOCK.md` y `phase-2/TRACKING.md` |
| Configuraciones | Infraestructura de pruebas `Debug` y `Release` |
| Optimizaciones | `O0`, `O1`, `O2`, `O3`, `All` y `NA` cuando el caso lo declara |
| Selección | Total, ola, componente, contrato y caso individual |
| Evidencia | `_nscbuilder3/history` y `_nscbuilder3/work`, creados en ejecución por el usuario |

### Archivos protegidos

- Todo el código de producto bajo `include/` y `src/`.
- Todo `tests/compiler-suite/**`.
- Todo `tests/harness/**`.
- `CMakeLists.txt` y todos los archivos CMake.
- `nscbuilder.py`, `nscbuilder2.py` y `_nscbuilder2/**`.
- `docs/compiler-optimization/test-infrastructure/test-execution-parity.csv`.
- `docs/compiler-optimization/test-infrastructure/test-execution-parity.json`.
- `docs/compiler-optimization/test-infrastructure/test-execution-parity.md`.
- `tests/harness/nsc_test/suites.py`.
- `tests/harness/run_nsc_tests.py`.
- `tests/harness/tests/test_suite_adapter.py`.

### Acciones prohibidas

- No ejecutar durante esta tanda la compilación que la GUI ofrece al usuario.
- No desplegar ni escribir en `C:`.
- No ejecutar baseline, pruebas históricas ni la batería de Fase 2 durante esta tanda.
- No modificar pruebas, oráculos, manifiestos, gates, producto o CMake.
- No crear paquetes, módulos, archivos `.pyw` ni scripts auxiliares para Builder 3.
- No usar fallbacks ni artefactos antiguos cuando falle un productor.
- No hacer `push` ni modificar remotos.

### Contrato funcional del único script

`nscbuilder3.py` debe contener:

1. carga determinista de olas, diseños, asignación, manifiesto y disposiciones;
2. configuración y construcción explícitas de NSC con Clang/LLVM/LLD;
3. validación y selección del `nsc.exe` construido y de `nslink.exe`;
4. configuración separada de la infraestructura de pruebas;
5. compilación separada del target `nsc_component_tests` en `Debug` o `Release`;
6. selección por total, ola, componente, contrato o caso;
7. selección independiente de `O0`, `O1`, `O2`, `O3` o todas;
8. tabla visible de todos los casos y estados en vivo;
9. ejecución granular de casos soportados y clasificación visible de los no ejecutables;
10. cancelación del árbol de procesos activo;
11. reejecución de fallidos;
12. consola completa, errores condensados e historial;
13. evidencia progresiva y recuperable bajo `_nscbuilder3`.

### Criterio de terminación de la tanda

- Existe un solo archivo nuevo de implementación: `nscbuilder3.py`.
- La GUI separa claramente construcción/selección de NSC, infraestructura y ejecución.
- El catálogo descubre exactamente el conjunto canónico vigente sin omisiones silenciosas.
- `Debug`/`Release` y `O0`/`O1`/`O2`/`O3`/`All` son dimensiones independientes.
- La tabla permite ver y seleccionar total, ola, componente, contrato y caso.
- Ninguna disposición no ejecutable se presenta como `PASS`.
- La comprobación sintáctica de Python y el modo de auditoría del catálogo pasan.
- La compilación C++ y la ejecución funcional quedan pendientes del usuario, sin presentarse como verificadas.

## Estado

`IMPLEMENTACIÓN LISTA; VALIDACIÓN FUNCIONAL DIFERIDA AL USUARIO`.

## Resultado implementado

- Un solo archivo de código nuevo: `nscbuilder3.py`.
- Catálogo completo con `9,587` casos, `1,208` contratos y `44` componentes.
- `5,695` casos ejecutables y `3,892` disposiciones no ejecutables visibles.
- Configuración y construcción explícitas de NSC en `Debug`, `Release` o ambas,
  con enlace host estático o dinámico y optimización host seleccionable.
- `O0` es la optimización host predeterminada; `O1`, `O2` y `O3` permanecen
  disponibles como selecciones explícitas.
- Autodescubrimiento de Clang/LLVM/LLD en `C:\clang-llvm`, CMake/Ninja desde
  `PATH`, fuentes desde el repositorio y NSLink junto al NSC o en su salida
  Release conocida.
- Los botones de construcción ejecutan primero su configuración con las rutas
  actuales; no requieren una configuración manual previa ni reutilizan
  silenciosamente una caché con otros parámetros.
- Configuración y construcción separadas del target `nsc_component_tests` en
  `Debug`, `Release` o ambas; construir las pruebas no reconstruye NSC.
- Selección de ejecución por total, ola, componente, contrato o caso.
- Selección independiente de `O0`, `O1`, `O2`, `O3` o todas; los casos sin
  optimización usan `NA`.
- Estados en vivo, cancelación del árbol activo, reejecución de fallos, log
  completo, reporte condensado e historial persistente.
- La pestaña de construcción interpreta las líneas nativas `[n/total]` de
  Ninja y muestra barra, contador y descripción del paso actual sin retirar la
  línea original de la consola ni de `full.log`.
- La misma pestaña expone `Cancelar construcción` inmediatamente al lado de
  `Construir NSC` durante configuración o compilación y lo conecta a la
  cancelación del árbol de procesos ya existente; el botón de ejecución
  permanece reservado para la batería de pruebas.
- Recibo SHA-256 del `nsc.exe` y `nslink.exe` externos seleccionados.

## Evidencia ejecutada

- `python -m py_compile nscbuilder3.py`: correcto.
- `python nscbuilder3.py --audit-catalog`: correcto; `unclassified = 0`.
- `python nscbuilder3.py --gui-smoke` con plataforma Qt fuera de pantalla:
  correcto.
- auditoría estática del comando CMake: todos los valores `-D` usan rutas con
  `/`, el árbol Release es `A:/nsc-devel/build-clang` y no queda el escape
  inválido `\c` de CMake.
- comprobación de líneas mayores de 120 caracteres: ninguna.
- `git diff --check` sobre los archivos de la tanda: sin errores de formato.
- análisis sintáctico mediante `ast.parse` y prueba aislada del contrato
  `[13/337] Building CXX object ...`: correctos; no construyó NSC ni ejecutó
  pruebas.
- análisis sintáctico y auditoría del cableado del botón de cancelación:
  correctos; la terminación real del árbol queda pendiente de una ejecución
  iniciada por el usuario.
- análisis sintáctico, JSON del cursor y contrato estático del cierre terminal:
  correctos; no se compiló NSC ni se ejecutaron pruebas.

La implementación no lanzó compilaciones por cuenta propia. Posteriormente el
usuario ejecutó Builder 3 y produjo la evidencia
`_nscbuilder3/history/20261009-225838-013b3717`: la configuración Release y la
construcción del target `nsc` pasaron `2/2` sobre `c9a1c52ce`. La corrida usó
Clang/LLVM 23.1.0, LLD, runtime estático VS2026, enlace host `STATIC`,
optimización host `O0` y produjo `A:/nsc-devel/build-clang/bin/nsc.exe`.

No se ha construido todavía `nsc_component_tests` mediante Builder 3 ni se ha
ejecutado la batería de Fase 2. Esas comprobaciones permanecen explícitamente
diferidas y no se presentan como aprobadas.

La visualización estructurada del progreso de Ninja permanece igualmente
pendiente de comprobación funcional en la siguiente ejecución del usuario. El
log histórico `20261009-225838-013b3717` demuestra que la fuente de datos ya
contiene las líneas completas `[0/2]`, `[1/14]` a `[13/14]`; el cambio actual
sólo las expone de forma persistente en la interfaz.

La corrida `20261009-231217-72aa28dd` confirmó que el proceso terminaba
correctamente aunque Ninja dejara como última línea de progreso `288/289`. La
interfaz ahora completa la barra al recibir el resultado correcto del proceso,
muestra `total/total — TERMINADO (100%)` y reemplaza la descripción del último
paso por una confirmación terminal. Esta corrección no oculta ni altera la
salida conservada en `full.log`.

