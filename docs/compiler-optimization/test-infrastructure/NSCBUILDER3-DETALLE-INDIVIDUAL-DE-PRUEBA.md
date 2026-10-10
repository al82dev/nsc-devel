# NSC Builder 3 — detalle individual de una prueba

## Resultado implementado

NSC Builder 3 permite abrir una vista de diagnóstico de sólo lectura mediante
doble clic sobre una prueba en cualquiera de estas proyecciones:

- la tabla de la corrida actual;
- la tabla de fallos exactos;
- una corrida cargada desde el historial.

El doble clic no ejecuta la prueba. La acción explícita de ejecutar o volver a
correr conserva su comportamiento independiente.

## Información presentada

Cada pestaña de detalle contiene cuatro secciones:

1. `Resumen`: identidad del caso, contrato, componente, ola, objetivo,
   propósito, capa, clase del escenario, tags, dependencias, aislamiento,
   timeout, ejecutor, perfil, configuración, estado, duración, diagnóstico y
   ruta de evidencia.
2. `Entradas y oráculo`: proyección JSON de los inputs y del resultado esperado
   declarados por el manifiesto canónico.
3. `Salida completa`: contenido íntegro del archivo de evidencia principal y
   de los demás `.log` ubicados en el mismo directorio del caso.
4. `Manifiesto`: registro canónico completo que dio origen a la prueba.

## Fuente de autoridad

La vista no reconstruye ni interpreta resultados. Utiliza exclusivamente:

- `CaseRecord` y el manifiesto canónico para escenario, inputs y oráculo;
- `RunVariant` para Build y optimización;
- `CaseOutcome`/`results.json` para estado, duración, detalle y evidencia;
- el archivo de evidencia individual ya publicado por la corrida.

Los históricos se leen sin modificarlos y las rutas relativas se resuelven
contra el directorio de la corrida a la que pertenecen.

## Interacción

- Un clic sencillo en la columna `Acción` continúa solicitando la ejecución
  individual.
- La acción se difiere durante el intervalo de doble clic del sistema.
- Si llega el doble clic, la acción sencilla pendiente se cancela y se abre el
  diagnóstico.
- `Abrir evidencia` muestra en el explorador el directorio que conserva los
  archivos originales.
- `Cerrar detalle` elimina únicamente la pestaña visual; no borra evidencia.

## Validación dirigida

- `python -m py_compile nscbuilder3.py`.
- `python -m unittest tests.compiler-suite.controller.tests.test_nscbuilder3_history`.

La prueba dirigida comprueba que el resumen conserva el diagnóstico, que los
inputs y el oráculo proceden del manifiesto y que la salida reúne el log
principal y un log hermano sin truncarlos.

No se construyó NSC, no se ejecutó la batería de pruebas y no se modificó el
compilador.
