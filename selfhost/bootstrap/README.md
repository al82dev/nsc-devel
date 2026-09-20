# Infraestructura de bootstrap

SH00 prepara el entorno sin portar todavía código del compilador.

## Archivos normativos

- `bootstrap.lock`: identidades y hashes de la semilla y herramientas externas.
- `generations.json`: identidad y reglas de aislamiento de las cuatro generaciones.
- `dependencies.md`: inventario de dependencias permitidas.
- `bootstrap.config.example.json`: ejemplo de configuración independiente de la máquina.
- `verify_environment.py`: verificador sin dependencias Python externas.
- `bootstrap-profile.json`: contrato ejecutable del Bootstrap Profile 2.0.
- `verify_profile.py`: filtro obligatorio de fuentes y proyectos previo a cada generación.
- `clean_selfhost.py`: limpieza dirigida, seca por omisión y protegida por marcador.
- `recovery.md`: recuperación de la última semilla válida.
- `schemas/`: contratos JSON versionados para lock, reportes y gates.

Las rutas físicas no se guardan en el lock. Se resuelven mediante variables de configuración y la
evidencia registra las rutas absolutas efectivamente utilizadas.

SH01 congela el perfil. El comando mínimo para proteger las fuentes es:

```text
python -B selfhost/bootstrap/verify_profile.py --source-root selfhost/nsc
```

Un resultado 2 significa infracción del perfil; 3 significa contrato o entrada inválidos. Ninguno
permite continuar al bootstrap.

SH07 añade `verify_ir.py`: construye la matriz diferencial de HIR/MIR/CFG/dominancia/SSA, exige
estabilidad entre O0/O1/O2/O3 y materializa bajo `selfhost/out` un caso reducido cuando aparece la
primera divergencia estructural.

SH08 añade `verify_llvm_backend.py`: valida objetos, assembly, anatomía `llvm-readobj` y
ejecutables ya probados por CMake en cuatro perfiles y ambos linkers; publica hashes canónicos.

SH09 añade `verify_project_driver.py`: comprueba dumps de proyecto estables, response files,
objetos por perfil, `nsmeta`, manifiestos y los ocho ejecutables enlazados.

SH10 añade `verify_first_generation.py`: congela cada `.ns`/`.nsproj` del compilador y sus cuatro
dependencias bootstrap, compara dos construcciones limpias de `nsc1`, audita PE/imports y ejecuta
el corpus del compilador y la matriz O0–O3 con ambos linkers.

SH11 añade `verify_self_hosted_generation.py`: observa el árbol de procesos durante `nsc1 → nsc2`,
exige cero hijos, repite en dos rutas/locale/timezone, compara diagnósticos, `nsmeta` y objetos, y
ejecuta O0–O3 con ambos linkers. El reporte conserva `source_derived_codegen=false` hasta que el
backend sustituya la reproducción atestada de la imagen.

SH12 añade `verify_fixed_point.py`: construye `nsc3` tres veces, compara imágenes y objetos sin
normalización y exporta `machine-attestation.json`. La opción `--cross-machine-evidence` valida una
segunda ejecución real; su ausencia queda como problema normativo y no se disfraza con dos rutas.

SH13 añade `verify_robustness.py`: mide startup/compilación/tamaño contra presupuestos separados,
inyecta entradas hostiles y fallos, prueba interrupción y exige limpieza posterior.

SH15 añade `verify_image_replay_retired.py`: inspecciona la implementación, invoca una solicitud
real de generación, exige código 78 y demuestra que ni el productor cambia ni se materializa un
ejecutable de salida. Su aprobación certifica el retiro del atajo, no codegen derivado de fuentes.

SH16 añade `verify_source_derived_backend.py`: ejecuta ocho combinaciones de fuente, optimización y
linker, compara los hashes de objetos y rechaza el antiguo cálculo fijo. Su reporte delimita el
alcance probado para impedir que una rebanada aritmética se confunda con compilador completo.

SH17 añade `verify_source_functions.py`: inspecciona los símbolos de funciones fuente en los cuatro
perfiles, ejecuta los ocho PEs producidos por `lld-link` y NSLink y exige que parámetros y llamada
directa produzcan 42. El reporte declara literalmente `i32-functions-parameters-calls` para no
confundir la ampliación con una generación autoalojada completa.

SH18 añade `verify_source_control_flow.py`: valida anatomía COFF y ejecuta en O0–O3 un programa con
locales, asignaciones, comparación, `while` e `if`, enlazado por LLD y NSLink.

SH19 añade `verify_source_loop_control.py`: exige lowering explícito de `for`, `do`, `break` y
`continue`, inspecciona los objetos y ejecuta la matriz O0–O3 con ambos linkers.

SH20 añade `verify_source_bool_void.py`: inspecciona funciones `bool`/`void` y ejecuta su matriz
tipada O0–O3 con LLD y NSLink.

SH21 añade `verify_source_expressions.py`: exige anatomía fuente, APIs LLVM de resto/bit/shift y
`phi`, y ejecuta O0–O3 con ambos linkers un fixture que detecta violaciones de cortocircuito.

SH22 añade `verify_source_integer_widths.py`: inspecciona las tres formas de conversión entera y
ejecuta una matriz con los diez tipos enteros/bool, operaciones signed/unsigned y llamadas tipadas.

SH23 añade `verify_source_pointers.py`: comprueba símbolos y anatomía COFF, y ejecuta el mismo
programa de punteros y memoria local en O0–O3 con LLD y NSLink.

SH24 añade `verify_source_floating_scalars.py`: exige operaciones y conversiones LLVM reales,
comprueba `_fltused` y ejecuta el fixture `float`/`double` en O0–O3 con LLD y NSLink.
