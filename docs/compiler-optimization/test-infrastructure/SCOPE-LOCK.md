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

## Componentes autorizados

- Herramientas nuevas de inventario y migración de pruebas.
- Infraestructura nueva bajo `tests/harness`, `tests/cases` y `tests/unit`.
- Integración de pruebas en CMake/CTest cuando la tanda correspondiente lo autorice.
- `nscbuilder2.py` exclusivamente en la Tanda 20.
- Documentación y tracking de esta tarea.

## Archivos protegidos

- Todo el código fuente de producción del compilador.
- `nscbuilder.py`.
- `_nscbuilder` y su historial.
- Las expectativas y entradas actuales hasta que exista paridad demostrada.
- Instalaciones y herramientas ubicadas en `C:`.

## Pruebas protegidas

- Los 684 identificadores del inventario inicial.
- Ningún caso puede desaparecer sin decisión, migración y evidencia de paridad.
- No se modifica una expectativa para acomodar la infraestructura nueva.

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
- La batería O0 completa puede ejecutarse con el compilador construido.
- La infraestructura antigua se retira sólo después de paridad.
- `nscbuilder2.py` integra al final la infraestructura terminada sin modificar `nscbuilder.py`.
- La medición final demuestra el cambio en compilación, enlaces, procesos y duración.

Sólo el usuario puede pausar, cancelar o cerrar la tarea persistente.
