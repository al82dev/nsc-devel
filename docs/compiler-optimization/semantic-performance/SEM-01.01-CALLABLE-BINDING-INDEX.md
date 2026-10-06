# SEM-01.01: indice compuesto reutilizable por callable

**Requisito:** `SEM-PERF-001`

**Tag:** `NSC.SEMANTICS.INDEX`

## Problema

`ProjectMethodBodyModel` conserva bindings en vectores deterministas, pero varias consultas calientes los recorren
completos para encontrar la anotacion correspondiente a un nodo dentro de un callable. Indexar solo por puntero de
AST es incorrecto porque una misma sintaxis puede participar en mas de una materializacion o especializacion.

## Cambio

Se agrega `ProjectCallableBindingIndex`, una estructura lateral que asigna:

```text
(AstNode*, callableOrdinal) -> bindingOrdinal
```

La estructura guarda ordinales, no punteros hacia elementos de vectores realocables. Registrar dos veces la misma
clave reemplaza el ordinal anterior y conserva la semantica de `newest binding wins` que hoy implementan los
recorridos inversos.

El indice es deliberadamente generico: instancias independientes podran respaldar tipos de expresion, llamadas y
nombres sin mezclar sus espacios de ordinales. No cambia todavia ninguna consulta del compilador.

## Complejidad y concurrencia

- insercion y consulta: O(1) promedio;
- memoria: O(K), donde K es el numero de pares distintos de sintaxis y callable;
- construccion: un solo escritor;
- publicacion: lectura concurrente permitida solo cuando la instancia ya no cambia.

No se usa recursion. Un `clear` elimina el indice completo entre compilaciones.

## Archivos

- `include/nativesharp/Frontend/ProjectCallableBindingIndex.h`: contrato y ownership;
- `src/Frontend/ProjectCallableBindingIndex.cpp`: almacenamiento y lookup;
- `tests/unit/ProjectCallableBindingIndexTests.cpp`: claves, reemplazo, ausencia y limpieza;
- `CMakeLists.txt`: fuentes y target unitario dirigido.

## Prueba dirigida

Target: `nsc_project_callable_binding_index_tests`.

La prueba demuestra:

1. el mismo nodo no colisiona entre callables;
2. dos nodos no colisionan dentro del mismo callable;
3. una clave repetida devuelve el ordinal mas reciente;
4. las claves nulas no entran al indice;
5. `clear` retira todas las entradas.

No se ejecuta baseline, CTest general ni otra optimizacion distinta de O0.

## Evidencia

- build dirigido del target: correcto;
- ejecucion directa: `PASS: 11 checks`;
- `clang-tidy`: la pasada alcanzo los archivos nuevos, pero no pudo resolver `<string_view>` con el entorno MSVC
  descrito por la base de compilacion; el reporte nativo se conserva en
  `_nscbuilder/semantic-performance/SEM-01.01/clang-tidy.txt`;
- el unico warning propio emitido antes de ese error, parametros enteros intercambiables, se elimino separando los
  ordinales mediante el parametro de sintaxis.
