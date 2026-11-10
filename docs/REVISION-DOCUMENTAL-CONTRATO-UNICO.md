# Revisión documental: contrato único de NativeSharp

Fecha: 2026-11-06. Revisión fuente inicial: `e10230d45`, rama `development-cpp-clang`.

## Alcance autorizado

El propietario solicitó revisar la documentación y los requerimientos del compilador, retirar las
referencias obsoletas a contratos ABI alternativos y evitar que vuelvan a dirigir el trabajo actual.
Esta tanda cambia sólo documentación e instrucciones de consulta. No modifica fuentes, pruebas,
herramientas de build, metadatos binarios, compilador, bibliotecas ni instalación.
Los archivos ajenos `location` y `rebuildTypes` permanecen intactos.

## Inventario y preservación

Se descubrieron 4,120 documentos propios versionados mediante `git ls-files -z`, considerando `.md`,
`.rst` y `.txt`, sin `CMakeLists.txt` ni documentación de terceros. El manifiesto por archivo contiene
SHA-256 y cada coincidencia con una referencia ABI numerada. Las réplicas generadas por builds e
instalaciones de pruebas no se trataron como documentación canónica.

Inventario: `A:\temp\nsc-documentation-review\inventory.json`.
Procedimiento: `A:\temp\nsc-documentation-review\inventory_docs.py` y `archive_docs.py`.

Se encontraron 20 documentos con referencias numeradas. Dieciocho expedientes del compilador se
preservaron íntegros fuera del árbol activo, en `A:\native-sharp-documentation-history\2026-11-06`.
Sus ubicaciones originales conservan enlaces al contrato vigente y al original. El manifiesto del
archivo conserva rutas, referencias originales y hashes comprobados después de la copia.
Los dos documentos de interfaces de herramientas se aclararon expresamente: no describen otra ABI
del lenguaje. El índice general dejó de presentar la etapa 2.0 como edición vigente y de ordenar
seleccionar documentos o compiladores por el banner de versión.

## Corrección del diagnóstico anterior

El rechazo registrado por la ruta de metadata binaria explícita era real, pero no demostraba que la
instalación funcional o los juegos estuvieran rotos. La recomendación de reconstruir Core no estaba
justificada y se retira. No se reconstruyó Core ni se modificó `C:\native-sharp`.

Comprobaciones con `C:\native-sharp\bin\nsc.exe`, build `4998f7ed435b91d5`:

1. `nsc --print-compatibility-policy` declara compatibilidad por capacidades y contenido; etiquetas
   de versión informativas.
2. `nsc --check` sobre `00D_pipeline_inspector/src/Program.ns`: salida cero.
3. Emisión normal del mismo archivo con `-O0` a
   `A:\temp\nsc-documentation-review\pipeline-check.exe`: salida cero.
4. Ejecución del binario recién generado: salida cero y `Expression: (12 * 4) + 7 = 55`.

Estas comprobaciones acreditan el flujo normal de ese ejemplo. No certifican todos los juegos,
los 118 ejemplos ni todos los modos de consumo binario.

## Regla permanente de consulta

`AGENTS.md` conduce a `docs/CONTRATO-VIGENTE.md`. La instalación congelada es la autoridad operativa;
los expedientes viejos son historia. Antes de proponer migraciones, recompilaciones de bibliotecas o
cambios de instalación se contrasta el comando real con el flujo funcional del propietario.
Los diagnósticos internos se preservan como evidencia; no se borran validadores ni se cambian números
de metadata para ocultarlos.
