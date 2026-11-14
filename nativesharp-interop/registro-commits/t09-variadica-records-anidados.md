# T09-003: records C anidados en llamadas `sysv_abi` variádicas precompiladas

## Defecto

Sonda `build-claude-t08/tools/sonda-var-anidado.py`, evidencia
`A:/temp/nsc-t09-sonda-var-anidado-4igx4zrv`. Una función C `sysv_abi` variádica
`int t09_v(T09R r, int k, ...)` recibe un record que contiene otro record:

| Record | Sesión viva | Plantilla precompilada sin encabezado |
|---|---|---|
| `int x; int y;` | pasa | pasa |
| `int x; volatile int y;` | pasa | pasa |
| `int x; T09In sub;` | pasa | rechazado |
| `int x; volatile T09In sub;` | pasa | rechazado |

El rechazo es `sysv_abi record call plan is not available for t09_v` cuando el record va solo
como argumento fijo, y `sysv_abi variadic argument type is not yet supported` cuando además
va como argumento variable. Clang compila todos los casos.

Causa en el código (`planPrecompiledCppVariadicCall`, `CppProjectSessions.cpp`). Los
records de los parámetros fijos se reconstruyen con su propio nombre, para que la función
reconstruida conserve el tipo canónico del catálogo. Esa reconstrucción llamaba a
`makeSimpleCRecordAbiReplay` sin resolutor de dependencias: un campo record se escribía como
`struct __nsc_abi_dependency`, un tipo incompleto, y la comprobación posterior
(`matchesSimpleCRecordAbiReplay` sin resolutor) no podía aceptar un campo record. Los records
que van como argumento variable sí resolvían sus dependencias, pero la llamada también pasa
el record fijo y fallaba por él.

## Cambio

- `checkedFixedRecord` reúne las comprobaciones que ya hacía `fixedRecordSpelling` sobre un
  record del catálogo: identidad (USR y nombre), procedencia del catálogo precompilado,
  origen C, módulo, triple, layout igual al importado, nombre válido y replay posible.
- `fixedRecordSpelling` la usa sin cambiar su resultado: el parámetro fijo debe tener
  exactamente la grafía `struct <nombre>` o `union <nombre>`.
- `fixedDependency` aplica las mismas comprobaciones a un record anidado en un record fijo.
  Admite el campo `const` o `volatile` con `cppReplayUnqualifiedSpelling`.
- Cada record fijo se escribe después de sus dependencias (recorrido en profundidad, con
  detección de ciclos y los mismos límites que el replay de argumentos variables), todos con
  su propio nombre. El resolutor `fixedResolver` devuelve ese nombre.
- La verificación compara con Clang cada record fijo y cada dependencia, campo por campo y
  con el resolutor.

No se modificó Clang ni NSLink. El plan se obtiene como antes y el contrato NSMeta vigente
no cambia.

## Evidencia

### Fixture nuevo

`pruebas-tanda-09-precompilado-variadica-anidado/validar.py`, con y sin `--dll`. Para
`t09_v(r, 4)` y `t09_v(r, 4, 9, r)`, de O0 a O3, prueba estos records:

- con un record anidado plano;
- con un record anidado `volatile`;
- con un record anidado `const`;
- con un arreglo de dos records;
- con dos niveles de anidamiento y un `double` (24 bytes, en memoria);
- con una unión anidada.

En cada caso:

- un control C compilado por Clang 23.1.0 da el resultado esperado;
- la plantilla precompilada se consume con y sin encabezado y devuelve el mismo valor;
- los argumentos físicos del IR de NSC (tipo, extensión, `byval` y alineación) son los de la
  llamada de Clang.

El control negativo comprueba que un record anidado con un campo `_Alignas(16)` sigue
rechazado sin encabezado, con los dos diagnósticos anteriores.

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática | pasa (6 records × 2 llamadas × O0–O3) | `A:/temp/nsc-t09-variadica-anidado-h99lklrw` |
| DLL | pasa (mismas celdas) | `A:/temp/nsc-t09-variadica-anidado-n3c9xy_7` |

Los dos controles negativos `_Alignas` pasan en las dos variantes.

### Sonda tras el cambio

`A:/temp/nsc-t09-sonda-var-anidado-2v7_sraj`: los ocho casos de la sonda inicial pasan en
plantilla precompilada sin encabezado.

### Fixture de campos calificados

La sección variádica de `pruebas-tanda-09-precompilado-campos-calificados/validar.py` usaba
solo los records sin anidar, por este límite. Ahora recorre los cinco, también el record
anidado `volatile`, y pasa en estática y DLL (`A:/temp/nsc-t09-campos-calificados-a90f98jw`,
`A:/temp/nsc-t09-campos-calificados-isldt9el`). Los dos fixtures comparan ahora la presencia de
`byval` sin su tipo, que Clang escribe `%struct.T09R` y NSC `[N x i8]`.

## Límites

- `_Alignas` y los records empaquetados siguen fuera de la llamada `sysv_abi` variádica
  precompilada.
- Solo records C con nombre, sin espacio de nombres ni record contenedor; los records C++
  usan otra ruta (`replayPrecompiledCppVariadicCall`) y no cambian.
