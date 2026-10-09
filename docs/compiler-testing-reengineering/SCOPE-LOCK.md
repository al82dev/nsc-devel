# SCOPE LOCK — Reingeniería de pruebas de NSC, Fase 1

## Objetivo autorizado

Auditar de forma exhaustiva el código actual del compilador NativeSharp y producir:

1. Un inventario trazable de componentes, archivos, tipos, funciones y contratos.
2. El grafo de dependencias y las fronteras de responsabilidad entre componentes.
3. Test plans nuevos, diseñados desde los contratos observables del compilador.
4. Una matriz maestra que permita implementar después una suite nueva desde cero.

## Naturaleza, referencia y revisión

- **Naturaleza autorizada:** auditoría estática y documentación técnica.
- **Repositorio canónico:** `A:\nsc-devel`.
- **Rama observada:** `development`.
- **Revisión congelada:** `3bb175443a18b2049c20d75443dd6743e2993903`.
- **Commit observado:** `Migrate 43 direct NSC tests to manifests`.
- **Rango de commits evaluado:** no aplica; la auditoría describe el árbol de producción en la revisión
  congelada, sin reconstruir todavía su historia.
- **Último estado certificado:** no aplica como oráculo de ejecución porque esta fase no compila ni ejecuta;
  cualquier afirmación de comportamiento se limita a evidencia estática y queda rotulada como tal.
- **Comportamiento que debe permanecer idéntico:** todo el comportamiento del compilador y de la batería
  histórica; esta fase no puede modificarlo.

## Trabajo permitido

- Lectura y análisis estático del árbol `src/`, `include/` y archivos de construcción que definan el compilador.
- Consulta de las pruebas históricas exclusivamente como referencia de comportamientos conocidos.
- Creación y actualización de documentos, inventarios, matrices, evidencia y cursor dentro de esta carpeta.
- Commits locales que contengan solamente entregables documentales de esta fase.

## Presupuesto de cambio

| Dimensión | Autorizado |
|---|---|
| Repositorios | Únicamente `A:\nsc-devel` |
| Componentes analizados | Todo el compilador versionado, sus definiciones de build y dependencias directas |
| Archivos modificables | Sólo `docs/compiler-testing-reengineering/**` |
| Símbolos modificables | Ninguno de producto o pruebas; no aplica a documentos nuevos |
| Comportamientos modificables | Ninguno |
| Pruebas modificables | Ninguna |
| Configuraciones ejecutables | Ninguna; sólo análisis estático de la revisión congelada |
| Perfiles | Ninguno; no se construye ni ejecuta `Debug` o `Release` |

## Archivos y subsistemas protegidos

- Todo `src/**`, `include/**`, `selfhost/**`, `lib/**`, `cmake/**`, `tests/**`, `tools/**` y archivos raíz.
- `nscbuilder.py`, `nscbuilder2.py` y toda infraestructura existente.
- Los seis archivos ya modificados antes de esta fase:
  - `docs/compiler-optimization/test-infrastructure/test-execution-parity.csv`;
  - `docs/compiler-optimization/test-infrastructure/test-execution-parity.json`;
  - `docs/compiler-optimization/test-infrastructure/test-execution-parity.md`;
  - `tests/harness/nsc_test/suites.py`;
  - `tests/harness/run_nsc_tests.py`;
  - `tests/harness/tests/test_suite_adapter.py`.
- La batería histórica completa de 684 pruebas y todos sus fixtures, expectativas y manifiestos.

## Trabajo prohibido en esta fase

- Modificar código del compilador.
- Modificar, renombrar, mover o borrar pruebas históricas.
- Modificar la infraestructura de pruebas existente, `nscbuilder.py` o `nscbuilder2.py`.
- Crear o implementar pruebas nuevas.
- Compilar NSC o cualquier herramienta.
- Ejecutar pruebas, baseline, minibaselines o cobertura.
- Tocar o desplegar en `C:`.
- Hacer `push`, reescribir historia o alterar cambios ajenos no comprometidos.
- Convertir conteos de ramas internas o de `if` en contratos manuales de mantenimiento.

## Frontera metodológica

La unidad de diseño es el contrato estable y observable, no la forma accidental de una implementación. Cada elemento inventariado debe registrar:

- responsabilidad;
- entradas y precondiciones;
- salidas y poscondiciones;
- estados e invariantes;
- errores y diagnósticos;
- dependencias;
- superficie observable;
- estrategia de prueba futura;
- trazabilidad al código fuente.

La cobertura instrumental pertenece a una fase posterior. Esta fase no promete cobertura de líneas o ramas; deja preparada una suite contractual sobre la cual podrá medirse.

## Protección de trabajo existente

La batería histórica de 684 pruebas y la infraestructura de migración actual quedan congeladas como respaldo. Los archivos que ya estuvieran modificados antes de iniciar esta fase no forman parte de sus commits y deben permanecer intactos.

## Criterio de cierre

La fase queda técnicamente lista para revisión sólo cuando:

- todos los archivos de producción del inventario canónico estén clasificados;
- todos los tipos y funciones públicas o internamente relevantes estén asociados a un contrato o a una justificación explícita de no prueba directa;
- cada componente tenga test plan con casos válidos, inválidos y de borde;
- el grafo de dependencias permita ordenar la implementación futura;
- una segunda auditoría no encuentre archivos, símbolos o contratos omitidos;
- no haya filas con estado desconocido o sin tratamiento.

Sólo el usuario puede pausar, cancelar o declarar cerrada la tarea.
