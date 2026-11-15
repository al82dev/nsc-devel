# Campos con desplazamientos explícitos y vistas

## Problema y alcance

Una vista de `holder.first` impedía reasignar `holder.second` cuando la estructura tenía
campos `at(...)`, incluso si los rangos físicos estaban separados. El análisis de cuerpos
marcaba toda estructura con desplazamientos explícitos como potencialmente solapada.
La reproducción `overwrite_explicit_disjoint_view` falló en el corte `bb603c7ed`.

La corrección está limitada al cálculo de procedencia de una vista de un campo y no cambia
la disposición de los valores ni los contratos de interoperabilidad. Usa el cálculo
existente de `ProjectObjectLayoutModel` en un modelo aislado y sin publicar diagnósticos.
Solo se admite la separación cuando se resuelve un único campo físico y su rango no comparte
bytes con ninguno de los demás. Si falta evidencia, el análisis conserva el comodín previo.
Las estructuras precompiladas sin prueba completa, uniones, campos de tamaño desconocido y
layouts inválidos siguen por la ruta conservadora.

## Verificación

- `overwrite_explicit_disjoint_view` y el caso adyacente pasan con `Rc` y `Arc` en O0/O2.
- Los casos de solapamiento total y parcial y la unión destructible siguen rechazándose.
- Pasan los controles de destrucción de valores por defecto durante una excepción.
- La compilación incremental de `nsc` y 22 pruebas dirigidas de integración pasaron.
- Siete pruebas dirigidas de SDK y ASan pasaron; Core se construyó en la carpeta de evidencia
  con el compilador integrado para la prueba binaria de `Arc`.
- `clang-tidy` con analizadores de núcleo y C++ no detectó avisos nuevos. Un aviso en
  `OwnershipAnalyzer.cpp` también se observó en el corte de origen; permanece sin modificar.

La prueba amplia `shared-public-pointers` no se ejecutó completa después de este cambio;
los casos anteriores se seleccionaron mediante `NSC_SHARED_POINTER_CASES`. Los comandos y
resultados están en `A:\nsc-integracion-main-limitaciones-evidencia`.
