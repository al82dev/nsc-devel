# Tanda 8: enlace de prueba de cuerpos de métodos

El objetivo `nsc_project_method_body_model_tests` falló al enlazar porque
`ProjectCompilationSnapshots.cpp` referencia `PrecompiledSemanticSnapshotCodec::decode`,
cuya definición pertenece a `nsc_precompiled`. Se agregó esa biblioteca a las
dependencias del objetivo de prueba, sin cambiar el compilador ni NSLink.

La reconstrucción incremental de `nsc`, `nsc_project_method_body_model_tests` y
`nsc_project_oop_lowering_tests` terminó con código 0. El control de modelos y HIR
pasó sus grupos de 78, 96, 40 y 8 comprobaciones. La ejecución de la prueba de
cuerpos falló en una de 118 comprobaciones, sobre topología de ondas paralelas;
la repetición aislada produjo el mismo fallo. Las comprobaciones nuevas de
alineación no reportaron fallo. Esta evidencia no cierra la tanda 8.
