# SCOPE LOCK — Refactorización de pruebas de NSC

## Objetivo autorizado

Reorganizar la infraestructura de pruebas de NSC para construir el compilador una vez, consolidar las unidades C++ internas, externalizar el código NativeSharp empotrado y ejecutar ese corpus mediante el `nsc.exe` construido.

## Naturaleza del cambio

- Infraestructura.
- Refactor de pruebas.
- Migración con paridad.
- Optimización del ciclo de desarrollo.

## Comportamiento que debe permanecer idéntico

- Semántica del lenguaje.
- Diagnósticos del compilador.
- Comandos públicos de NSC.
- Resultados de las 684 pruebas inventariadas.
- Identificadores actuales durante la fase de paridad.
- Selección explícita del compilador y de NSLink.

## Revisión conocida como correcta

El árbol `development` existente al iniciar la Tanda 1 el 2026-10-08. Esta tarea no promueve ni certifica una nueva revisión del compilador.

## Rango de commits evaluado

No aplica a una regresión entre commits. La matriz inicial se deriva del árbol y del CTest configurado existentes al comenzar la tarea.

## Repositorios autorizados

- `A:\nsc-devel`.
- `A:\nsc-refactor-docs` únicamente para mantener el plan maestro externo.
- `A:\nslink-devel`, `A:\nsasm-devel` y `A:\nsc-debugger` únicamente como
  productores externos de herramientas: se pueden inspeccionar y construir
  desde Builder 2, pero sus fuentes permanecen protegidas.

## Componentes autorizados

- Herramientas nuevas de inventario y migración de pruebas.
- Infraestructura nueva bajo `tests/harness`, `tests/cases` y `tests/unit`.
- Integración de pruebas en CMake/CTest cuando la tanda correspondiente lo autorice.
- `nscbuilder2.py` exclusivamente en la Tanda 20.
- Integración de construcción, staging y deploy de NSLink, NSASM y NSDBG
  exclusivamente dentro de Builder 2.
- Documentación y tracking de esta tarea.

## Archivos protegidos

- Todo el código fuente de producción del compilador.
- `nscbuilder.py`.
- `_nscbuilder` y su historial.
- Las expectativas y entradas actuales hasta que exista paridad demostrada.
- Instalaciones y herramientas ubicadas en `C:`.
- Fuentes, pruebas y builders propios de `A:\nslink-devel`,
  `A:\nsasm-devel` y `A:\nsc-debugger`.

## Pruebas protegidas

- Los 684 identificadores del inventario inicial.
- Ningún caso puede desaparecer sin decisión, migración y evidencia de paridad.
- No se modifica una expectativa para acomodar la infraestructura nueva.
- Cada identificador histórico debe conservar una ruta individual, inequívoca y
  realmente ejecutable; un conteo agregado, una suite que sólo cubra parte del
  contrato o la existencia de un archivo no demuestran esa ruta.
- La auditoría de paridad se realiza fila por fila sobre los 684 identificadores
  y distingue entre registro, posibilidad de ejecución y destino final correcto.
- Un adaptador transitorio puede conservar ejecución durante la migración, pero
  no permite declarar migrado un caso destinado a manifiesto o unidad consolidada.

## Configuración autorizada

- Windows x64.
- O0 para las corridas funcionales de esta migración.
- NSLink explícito para los casos que requieran enlace.

## Acciones prohibidas

- Modificar el compilador para facilitar una prueba.
- Crear características o casos nuevos de lenguaje durante la migración.
- Instalar o desplegar en `C:`.
- Ejecutar baseline, promoción o certificación.
- Ejecutar O1, O2 u O3.
- Ejecutar deploy o copiar artefactos a `C:` durante la implementación.
- Copiar una herramienta a staging o deploy sin recibo vigente y SHA-256.
- Ejecutar silenciosamente una construcción de herramientas: la selección
  `Autogenerar antes del deploy` debe ser explícita.
- Utilizar el NSC instalado en lugar del binario construido seleccionado.
- Modificar la infraestructura actual de `nscbuilder.py`.
- Crear runners separados por componente.
- Retirar la representación antigua antes de demostrar paridad.
- Usar remotos Git o publicar cambios.

## Presupuesto de la Tanda 1

| Dimensión | Autorizado |
|---|---|
| Repositorios | `A:\nsc-devel` |
| Componentes | Inventario y documentación de infraestructura de pruebas |
| Archivos | `tools/generate_test_destination_matrix.py`, `docs/compiler-optimization/test-infrastructure/*` |
| Símbolos | Generador estático nuevo; no modifica símbolos existentes |
| Comportamientos | Clasificar destinos sin mover ni ejecutar pruebas |
| Pruebas | Validación estática del inventario completo de 684 casos |
| Configuraciones | No aplica; no se construye ni ejecuta NSC |
| Perfiles | No aplica |

## Criterio exacto de terminación

- Las 20 tandas están implementadas y documentadas.
- Cada tanda tiene validación proporcional, Markdown y commit autocontenido.
- Los 684 casos conservan trazabilidad.
- Los 684 casos poseen exactamente una ruta ejecutable comprobable.
- Los 684 casos están en la capa final indicada por la matriz de destino.
- No existen casos bloqueados, ambiguos ni sin mapear en la auditoría uno a uno.
- La batería O0 completa puede ejecutarse con el compilador construido.
- La infraestructura antigua se retira sólo después de paridad.
- `nscbuilder2.py` integra al final la infraestructura terminada sin modificar `nscbuilder.py`.
- La medición final demuestra el cambio en compilación, enlaces, procesos y duración.

Sólo el usuario puede pausar, cancelar o cerrar la tarea persistente.

## Enmienda autorizada — detalle individual de pruebas en NSC Builder 3

Fecha: 2026-10-10.

### Objetivo exacto

Permitir que un doble clic sobre cualquier fila de prueba actual, fallida o
histórica abra una pestaña de diagnóstico de esa prueba sin ejecutarla. La
pestaña debe exponer el escenario canónico, sus entradas, el oráculo, la
configuración exacta, el resultado observado y la salida individual completa
conservada como evidencia.

### Naturaleza

- Infraestructura de consulta y diagnóstico.
- Cambio de interfaz de usuario sin cambios en ejecución ni clasificación.

### Presupuesto de cambio

| Dimensión | Autorizado |
|---|---|
| Repositorios | `A:\nsc-devel` |
| Componentes | NSC Builder 3 e historial de Phase 2 |
| Archivos | `nscbuilder3.py`, su prueba histórica dirigida y este documento |
| Símbolos | Proyección de detalle, lectura de evidencia y manejadores de doble clic |
| Comportamientos | Abrir y cerrar pestañas de sólo lectura; conservar la reejecución explícita |
| Pruebas | Sintaxis y prueba unitaria histórica dirigida |
| Configuraciones | No aplica; no se construye ni ejecuta NSC |
| Perfiles | Todos, únicamente como datos mostrados |

### Fronteras protegidas

- Código fuente del compilador.
- Manifiestos, oráculos, entradas y resultados guardados.
- Formato de `results.json`, `metadata.json` y `events.jsonl`.
- Ejecución, reejecución, selección, clasificación y persistencia de corridas.
- Historial existente, que permanece inmutable.

### Acciones prohibidas

- Ejecutar una prueba como consecuencia del doble clic.
- Inferir o inventar una causa que no aparezca en el resultado o su evidencia.
- Truncar la salida individual presentada.
- Reescribir evidencia histórica.
- Ejecutar la batería, construir NSC o modificar el compilador.

### Criterio de terminación de la enmienda

- El doble clic abre una pestaña y no dispara la reejecución pendiente.
- La vista funciona desde `Pruebas`, `Fallos exactos` e `Historial`.
- El resumen identifica escenario, contrato, inputs, oráculo y resultado.
- `Salida completa` muestra el log principal y los logs hermanos del mismo caso.
- La validación dirigida pasa sin ejecutar la batería del compilador.
