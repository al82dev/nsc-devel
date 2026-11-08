# Tanda 08: seguimiento detallado de interoperabilidad C

Esta vista desglosa los 20 ítems de la [tanda 08](TRACKING.md#08-declaraciones-nombres-y-tipos-c). Los identificadores originales permanecen vigentes. Cada casilla marcada describe **únicamente el caso y la validación indicados**, no la cobertura completa de su categoría. Las casillas vacías son trabajo o validación pendientes; no equivalen necesariamente a código inexistente.

La evidencia histórica y sus revisiones se conserva en [TRACKING.md](TRACKING.md). Un resultado antiguo puede haber sido ampliado después; ante discrepancias debe contrastarse con el código, el registro enlazado y el gate actual. La validación integral de la tanda sigue pendiente. Esta lista no debe interpretarse como un inventario cerrado de todas las variantes de C: las combinaciones aún no inventariadas siguen incluidas en los ítems originales.

## T08-001 — Representación y nombres

- [x] T08-001-01: Auditar la representación y búsqueda de nombres existentes. [Auditoría](TANDA-08-TIPOS-C.md).

## T08-002 — Identidad de declaraciones

- [x] T08-002-01: Comprobar estabilidad de USR de declaraciones C entre sesiones. [Auditoría](T08-002-IDENTIDAD-AUDITORIA.md).
- [x] T08-002-02: Comprobar restauración de USR de records y enums mediante el driver precompilado. [Prueba](registro-commits/t08-002-ruta-driver-c.md).
- [x] T08-002-03: Ejercitar encabezados homónimos de directorios distintos. [Prueba](registro-commits/t08-002-homonimos-c.md).
- [x] T08-002-04: Comprobar casos de typedef C equivalentes y rechazos por incompatibilidad. [Evidencia](registro-commits/t08-h1-typedef-c-equivalentes.md).
- [x] T08-002-05: Comprobar conciliación de records compatibles entre encabezados y metadata. [Evidencia](registro-commits/t08-h5-records-c-precompilado.md).
- [x] T08-002-06: Comprobar muestras de dependencias C en plantillas precompiladas. [Evidencia](registro-commits/t08-h3-identidad-expectativas.md).
- [x] T08-002-07: Recuperar y verificar muestras históricas NSMETA 1.45, 1.47 y 1.48. [1.45](registro-commits/t08-nsmeta-145-fixture-recuperado.md), [1.47](registro-commits/t08-nsmeta-147-fixture-recuperado.md), [1.48](registro-commits/t08-nsmeta-148-fixture-recuperado.md).
- [ ] T08-002-08: Obtener una muestra independiente NSMETA 1.46 y ejecutar la suite de compatibilidad de metadata.
- [ ] T08-002-09: Completar identidad y conciliación para todas las formas de declaraciones C incluidas en el ítem original.
- [ ] T08-002-10: Completar resolución de sombras y ámbitos de dependencias C de plantillas más allá de las muestras verificadas.
- [ ] T08-002-11: Validar integralmente publicación, restauración y conflictos de identidad con y sin encabezado.
- [ ] T08-002-12: Completar las formas de ámbito que aún no cubren los controles de sombras en plantillas.
- [ ] T08-002-13: Comprobar los conflictos de enums anónimos cuando no se demuestra la misma procedencia física.

## T08-003 — Funciones C

- [x] T08-003-01: Probar muestras de llamadas escalares y con punteros. [Detalle](TANDA-08-TIPOS-C.md).
- [x] T08-003-02: Probar funciones `static inline` y el camino ThinLTO indicado en el tracking principal.
- [x] T08-003-03: Probar las muestras de declaraciones sin prototipo y promociones C11 registradas en el tracking principal.
- [ ] T08-003-04: Completar importación y selección de las formas de declaración y redeclaración pendientes.
- [ ] T08-003-05: Validar visibilidad, sobreposición de nombres, enlace y consumo precompilado para las formas restantes.

## T08-004 — Variables globales C

- [x] T08-004-01: Comprobar las muestras de globales externas y `static` registradas en el tracking principal.
- [x] T08-004-02: Comprobar la matriz dirigida de escalares y punteros atómicos no TLS indicada en el tracking principal.
- [x] T08-004-03: Comprobar las muestras dirigidas de TLS estático, atómico y DLL registradas en el tracking principal.
- [ ] T08-004-04: Completar proyección y uso de las variantes de globales, tipos y calificadores no cubiertas.
- [ ] T08-004-05: Volver a ejecutar los controles TLS y de DLL dentro del gate integral de la tanda.
- [ ] T08-004-06: Completar la validación de globales en consumo directo y precompilado, incluyendo colisiones y sombras.

## T08-005 — Typedef

- [x] T08-005-01: Probar las muestras de alias escalares encadenados y punteros registradas en el tracking principal.
- [ ] T08-005-02: Completar alias de punteros, arreglos, funciones, records y enums para los casos aún no cubiertos.
- [ ] T08-005-03: Validar reconciliación y conflictos de alias en consumo directo y precompilado.

## T08-006 — Enteros y `_Bool`

- [x] T08-006-01: Probar la muestra mixta de campo, arreglo y llamada `_Bool` O0–O3.
- [x] T08-006-02: Comparar campos C volátiles con Clang en los caminos directo y precompilado. [Resultado](registro-commits/t08-volatile-campos-revalidacion-alias-pendiente.md).
- [ ] T08-006-03: Completar los tipos enteros, tamaños y signos de las declaraciones C restantes del objetivo.
- [ ] T08-006-04: Validar conversiones, ABI, almacenamiento y operaciones para las combinaciones aún no cubiertas.

## T08-007 — Punto flotante

- [x] T08-007-01: Probar `long double` IEEE binary64 de la muestra directa y precompilada O0–O3.
- [ ] T08-007-02: Completar tipos y formatos flotantes aplicables al objetivo y sus diagnósticos.
- [ ] T08-007-03: Validar campos, globales, llamadas y persistencia de las variantes restantes.

## T08-008 — `void`

- [x] T08-008-01: Probar el retorno `void`, `void*` y prototipo `f(void)` de la muestra C11.
- [ ] T08-008-02: Completar y validar usos restantes de `void` y `void*` en firmas, campos y metadata.

## T08-009 — Enumeraciones

- [x] T08-009-01: Probar las muestras de enum C y conversiones enum/entero registradas en el tracking principal.
- [x] T08-009-02: Probar el caso corregido de comparación `unsigned int` / `int`. [Evidencia](registro-commits/t08-enum-c-uint-int.md).
- [ ] T08-009-03: Completar valores representables, promociones y operaciones de los enums no cubiertos.
- [ ] T08-009-04: Completar identidad, colisiones y persistencia de enums nombrados, anónimos y anidados.
- [ ] T08-009-05: Validar las variantes restantes con encabezado y en biblioteca precompilada.

## T08-010 — Struct C

- [x] T08-010-01: Comparar las muestras de layout natural, empaquetado y anidado contra Clang. [Evidencia](registro-commits/t08-layout-comparacion-clang.md).
- [x] T08-010-02: Probar muestras de miembros anónimos y anidados en los caminos indicados en el tracking principal.
- [x] T08-010-03: Probar muestras de miembro flexible, lectura/escritura y conversión a puntero O0–O3 indicadas en el tracking principal.
- [ ] T08-010-04: Completar miembros y combinaciones de structs C que todavía no están cubiertos.
- [ ] T08-010-05: Resolver el orden de acceso mediante alias a miembro flexible `volatile` en modo Microsoft e ISO.
- [ ] T08-010-06: Validar las variantes restantes en directo, con encabezado y solo con metadata.
- [ ] T08-010-07: Validar los modos Microsoft e ISO con dos referencias al mismo encabezado en la misma compilación.
- [ ] T08-010-08: Verificar el alias volátil reasignado en los cuatro niveles de optimización después de la corrección.
- [ ] T08-010-09: Verificar lectura y escritura `*(data + index)` frente a Clang después de la corrección.

## T08-011 — Union C

- [x] T08-011-01: Probar muestras de miembros superpuestos, campos de bits y unions empaquetadas indicadas en el tracking principal.
- [ ] T08-011-02: Completar las formas restantes de unión, incluidos anidamientos y miembros anónimos.
- [ ] T08-011-03: Validar lectura/escritura, layout y consumo precompilado de las variantes restantes.

## T08-012 — Layout obtenido de Clang

- [x] T08-012-01: Comparar los tamaños, alineaciones de record y offsets de la muestra de layout. [Evidencia](registro-commits/t08-layout-comparacion-clang.md).
- [ ] T08-012-02: Resolver y verificar el contrato de alineación efectiva de cada campo frente a Clang.
- [ ] T08-012-03: Completar la cobertura de campos, empaquetado, bitfields y formas restantes en directo y metadata.

## T08-013 — Tipos incompletos

- [x] T08-013-01: Probar las muestras de punteros a structs y unions opacos en uso directo y precompilado.
- [ ] T08-013-02: Completar representación y reconciliación entre declaración incompleta y definición posterior.
- [ ] T08-013-03: Validar las operaciones permitidas y rechazadas para las combinaciones restantes.

## T08-014 — Punteros

- [x] T08-014-01: Probar la muestra `_Bool*` devuelta, almacenada y seleccionada que documenta el tracking principal.
- [ ] T08-014-02: Completar tipos apuntados, niveles de indirección y conversiones pendientes.
- [ ] T08-014-03: Completar paso, retorno, almacenamiento y consumo precompilado de las variantes restantes.
- [ ] T08-014-04: Verificar alineación, nulabilidad y conservación de calificadores donde correspondan.

## T08-015 — Calificadores

- [x] T08-015-01: Probar las muestras registradas de `const` y `volatile` en globales, campos y arreglos.
- [x] T08-015-02: Comparar los campos volátiles C contra Clang en los casos del fixture. [Resultado](registro-commits/t08-volatile-campos-revalidacion-alias-pendiente.md).
- [ ] T08-015-03: Completar la selección inequívoca del modo volátil de la unidad consumidora.
- [ ] T08-015-04: Preservar el orden de acceso y alineación en alias, reasignaciones y aritmética de punteros C volátiles. [Diagnóstico](registro-commits/t08-volatile-alias-aritmetica-diagnostico.md).
- [ ] T08-015-05: Conservar el contrato a través de parámetros, llamadas, ramas y proyectos precompilados.
- [ ] T08-015-06: Verificar identidad de caché y separación respecto a accesos `volatile` nativos de NativeSharp.
- [ ] T08-015-07: Completar calificadores y niveles de puntero restantes sin perder restricciones de escritura.
- [ ] T08-015-08: Habilitar la generación con modo explícito únicamente después de completar su contrato y retirar la protección temporal con pruebas.
- [ ] T08-015-09: Repetir la comparación de IR de alias directo, reasignado y con aritmética en Microsoft e ISO O0–O3.
- [ ] T08-015-10: Verificar el comportamiento con referencias mixtas y con biblioteca precompilada.

## T08-016 — Arreglos fijos

- [x] T08-016-01: Probar las muestras de arreglos y matrices empaquetadas registradas en el tracking principal.
- [x] T08-016-02: Probar las muestras de arreglos de punteros calificados y records indicadas en el tracking principal.
- [ ] T08-016-03: Completar rangos, dimensiones, tipos de elemento y calificadores restantes.
- [ ] T08-016-04: Validar acceso, layout, dirección y consumo precompilado de las variantes restantes.

## T08-017 — Punteros a función C

- [x] T08-017-01: Probar las muestras registradas de callbacks escalares y callbacks con record o enum.
- [x] T08-017-02: Probar las muestras precompiladas de callbacks con y sin encabezado indicadas en el tracking principal.
- [ ] T08-017-03: Completar representación y llamadas para las firmas y convenciones restantes.
- [ ] T08-017-04: Completar callbacks almacenados, retornados y calificados en campos y globales.
- [ ] T08-017-05: Validar persistencia y consumo de las variantes restantes a través de bibliotecas precompiladas.
- [ ] T08-017-06: Conciliar las notas históricas contradictorias sobre callbacks volátiles almacenados con el comportamiento y las pruebas actuales.
- [ ] T08-017-07: Conciliar el rechazo histórico del método que retorna callback precompilado con la validación posterior del mismo camino.

## T08-018 — Colisiones de nombres

- [x] T08-018-01: Probar la muestra de sombra local de global y enumerador C en O0.
- [ ] T08-018-02: Completar colisiones entre declaraciones C y nombres NativeSharp en los ámbitos restantes.
- [ ] T08-018-03: Validar resolución, ambigüedad y sombras en directo y en consumidores precompilados.

## T08-019 — Diagnósticos de tipos incompletos

- [x] T08-019-01: Probar diagnósticos para aritmética, incremento e índice de records C incompletos. [Evidencia](registro-commits/aritmetica-puntero-incompleto-c.md).
- [x] T08-019-02: Probar el diagnóstico de local incompleto por valor y la muestra de `sizeof`. [Evidencia](registro-commits/diagnostico-local-incompleto-c.md).
- [ ] T08-019-03: Cubrir operaciones inválidas y combinaciones de tipos incompletos restantes.
- [ ] T08-019-04: Verificar que los diagnósticos identifican la declaración y la operación inválida también con metadata.

## T08-020 — Comparación de layout con Clang

- [x] T08-020-01: Comparar la muestra de 55 valores de layout y lecturas/escrituras cruzadas. [Evidencia](registro-commits/t08-layout-comparacion-clang.md).
- [ ] T08-020-02: Completar comparaciones de tamaño, alineación y offsets para las formas pendientes de struct y union.
- [ ] T08-020-03: Comparar las alineaciones y ventanas de acceso relevantes en el IR contra Clang.
- [ ] T08-020-04: Validar los tipos C usados desde metadata sin encabezado en todas las formas pendientes.

## Cierre de la tanda

- [ ] T08-GATE-01: Completar la implementación y las comprobaciones pendientes de T08-002 a T08-020.
- [ ] T08-GATE-02: Resolver o clasificar con evidencia el fallo de la prueba de topología de ondas paralelas.
- [ ] T08-GATE-03: Ejecutar la validación integral agrupada de la tanda 8 y corregir sus fallos hasta que pase.
- [ ] T08-GATE-04: Actualizar estados y evidencias contra la revisión validada, sin cerrar por muestras parciales.
- [ ] T08-GATE-05: Revisar la agrupación de commits relacionados y preservar un respaldo del historial, conforme a lo acordado.
