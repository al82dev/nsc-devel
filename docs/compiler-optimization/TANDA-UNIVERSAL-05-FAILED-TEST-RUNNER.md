# Tanda universal 05 — ejecutor dirigido mediante `test.run`

## SCOPE LOCK

- Objetivo autorizado: ejecutar exclusivamente los nombres CTest escritos en `test.run` y conservar evidencia completa y condensada.
- Naturaleza: infraestructura de pruebas.
- Archivos modificables: `nscbuilder.py`, `run_failed_tests.py`, `test.run` y este documento.
- Archivos protegidos: compilador, lenguaje, runtime, bibliotecas, fixtures y pruebas.
- Prohibido: compilar NSC, ejecutar baseline, instalar o escribir en `C:`, hardcodear la selección dentro del ejecutor.
- Terminación: selección externa editable, actualización automática tras CTest completo y validación sintáctica sin ejecutar pruebas.

## Cambios

### `run_failed_tests.py`

- Lee una prueba exacta por línea desde `test.run`.
- Ignora comentarios y líneas vacías y deduplica conservando el orden.
- Ejecuta cada nombre con una expresión regular anclada y escapada, por lo que no arrastra pruebas no solicitadas.
- Publica toda la salida en consola mientras la conserva en `full.log` y en un log individual por prueba.
- Genera `summary.md` y `summary.json` con estado, código de salida, duración, diagnósticos extraídos y ruta de evidencia.
- Un manifiesto vacío termina correctamente e informa que no existen CTests fallidas por repetir.
- No construye, configura, instala ni limpia artefactos.

### `test.run`

- Contiene las 47 CTests de la unión de las dos corridas reportadas el 7 de octubre de 2026.
- Es el único origen de la selección; modificar la lista no requiere modificar Python.

### `nscbuilder.py`

- Acumula únicamente los nombres fallidos informados por etapas CTest ordinarias.
- Al cerrar una corrida no cancelada, sobrescribe atómicamente `test.run` sólo si todos los linkers solicitados terminaron CTest.
- Una corrida CTest verde deja el manifiesto sin entradas ejecutables.
- Una cancelación, un fallo anterior a CTest o un diagnóstico auxiliar con LLD no reemplaza el manifiesto vigente.

## Complejidad

- Lectura y deduplicación del manifiesto: `O(n)` esperado mediante lista ordenada y conjunto de pertenencia.
- Ejecución: iterativa y secuencial para mantener logs deterministas, `O(n)` procesos CTest.
- Memoria: `O(s)`, donde `s` es la salida combinada necesaria para extraer diagnósticos y escribir JSON.
- No se introdujo recursión.

## Validación permitida

- Compilación sintáctica de ambos scripts Python.
- Lectura mediante `--list` para demostrar que las 47 entradas se consumen desde `test.run`.
- No se ejecuta ninguna CTest ni baseline en esta tanda.
