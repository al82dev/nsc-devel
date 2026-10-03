# NS3-T04-F07 — Generic resource limits

## Estado

`TERMINADA`. Los límites productivos de monomorfización son configurables por proyecto y CLI sin
cambiar los valores normativos predeterminados.

## Qué se hizo

- Se conservaron los defaults 64 de profundidad y 16384 instancias nuevas.
- Se añadieron `GenericInstantiationDepth` y `GenericInstantiationLimit` al proyecto raíz.
- Se añadieron los overrides `--generic-depth-limit=` y `--generic-instantiation-limit=`.
- Los rangos se validan antes de compilar: 1..4096 y 1..1048576.
- `ProjectCompilation` construye un único presupuesto con la configuración efectiva.
- El presupuesto distingue ciclos, profundidad y cantidad, y conserva un backtrace causal de los
  doce marcos más recientes.
- Los cache hits no consumen otra instancia; los rechazos no contaminan la caché.
- La configuración del proyecto también se aplica a acciones de diagnóstico de `.nsproj`, no sólo
  a la emisión final, por lo que dumps y checks observan el mismo contrato.

## Archivos modificados

- `include/nativesharp/Compilation/ProjectLoader.h` y `src/Compilation/ProjectLoader.cpp`:
  propiedades, rangos y configuración efectiva.
- `include/nativesharp/Frontend/ProjectGenericModel.h` y
  `src/Frontend/ProjectGenericModel.cpp`: constructor explícito del presupuesto.
- `src/Frontend/ProjectCompilation.cpp`: conexión de la configuración al materializador.
- `src/Driver/Driver.cpp`: CLI, precedencia sobre `.nsproj` y ayuda bilingüe.
- `tests/unit/ProjectLoaderTests.cpp`: contrato de carga y rechazo.
- `CMakeLists.txt`: target dirigido F07.
- `docs/3.0/TANDA-04-GENERICOS-3.0.md`: cierre maestro de T04.
- `A:/NATIVESHARP-3.0-TRACKING.md`: estado del ítem.

## Archivos nuevos y por qué

- `tests/unit/NativeSharp3T04GenericResourceLimitTests.cpp`: ciclos directos/indirectos,
  truncamiento causal, reutilización de caché, límite de cantidad, expansión ramificada, override
  CLI, crecimiento de profundidad y diagnóstico reproducible.
- `docs/3.0/language/generic-resource-limits.md`: contrato de usuario ES/EN.
- Este documento: evidencia atómica de F07.

## Mejora obtenida

El compilador puede ajustar el costo genérico a una máquina o producto concreto sin esconder los
límites ni cambiar silenciosamente el perfil común. Una explosión termina de forma reproducible y
explica su cadena causal; el código que reutiliza instancias no es penalizado.

## Validación dirigida

- `nsc_ns3_t04_generic_resource_limit_tests`: 9/9.
- `nsc_project_loader_tests`: 89/89.
- Construcción local de `nsc`: correcta; runtime host estático: correcto.
- Regresiones T04 y parser: ejecutadas individualmente al cierre.

No se ejecutó baseline, CTest general, O1/O2/O3, instalación ni deploy.
