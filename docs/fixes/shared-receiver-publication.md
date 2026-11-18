# Publicación del receptor y punteros compartidos

Los ejemplos históricos guardan `this` en un campo estático, ya sea desde el
constructor o el destructor de `Payload`, crean un `Rc<Payload>` o
`Arc<Payload>` y finalmente expresan una lectura del alias fuera del ámbito del
fuerte. El análisis con Core binario aislado aceptó las cuatro reproducciones
originales y dos controles sin publicación. Solo se ejecutó el análisis
semántico; no se enlazó ni ejecutó ninguno de los programas. Evidencia:
`A:/nsc-fixes-agente-2-evidencia/receiver-publication-binary-20261112/run-20261112T042943047446Z/results.json`.
El registro incluye fuentes exactas, comandos, cwd y hashes estables antes y
después; el compilador y Core proceden de trabajo en curso.

Esto demuestra que el analizador acepta esa forma; no demuestra una carrera
en ejecución ni que las reglas vigentes prohíban publicar `this` en cualquier
clase. `AssignmentAnalyzer.cpp` registra resúmenes de escape de parámetros con
ordinal, mientras que el receptor se representa como `CurrentReceiver`, por lo
que esa vía no registra el escape de la fuente receptora. Esta observación no
demuestra que corregir solo dicha vía cubra llamadas, alias intermedios,
destructores ni constructores binarios anteriores. Se requiere un contrato de
resumen para el receptor y su propagación antes de acreditar la garantía de
vida; no se agregó una prohibición global ni se ejecutó una lectura colgante.

Una comprobación posterior de emisión O0, con el mismo compilador y Core de
trabajo cuya procedencia final aún está pendiente, distinguió las fases. Los
dos ejemplos originales que publican desde el constructor llegaron al backend
y se rechazaron con `shared payload publishes its receiver during construction
or destruction`. Los dos ejemplos originales con publicación en destructor y
dos controles sin publicación no llegaron a esa guarda: fallaron antes por la
accesibilidad del constructor implícito en el consumo binario. Registro:
`A:/nsc-fixes-agente-2-evidencia/receiver-publication-emission-20261112/run-20261112T045936696066Z/results.json`.

Una variante independiente añadió un constructor público vacío a las clases
de los casos de destructor y a los controles, sin modificar los originales.
Con ese cambio las cuatro publicaciones directas (constructor/destructor y
Rc/Arc) se rechazaron con el diagnóstico del backend y ambos controles emitieron
correctamente. No se ejecutó ninguno de los programas; el resultado es solo O0
y no acredita todas las formas de publicación. Registro:
`A:/nsc-fixes-agente-2-evidencia/receiver-publication-explicit-20261112/run-20261112T050034276971Z/results.json`.

La regresión `nsc.sdk.shared-receiver-publication-emission` comprueba la emisión
de las cuatro variantes explícitas y los dos controles en O0, con Core
configurable; no ejecuta los programas. Con Core WIP devolvió 0 y sin Core
omitió con 77. Registros:
`A:/nsc-fixes-agente-2-evidencia/receiver-publication-regression-20261112/permanent-configured.log`
y `A:/nsc-fixes-agente-2-evidencia/receiver-publication-regression-20261112/permanent-missing-core.log`.
