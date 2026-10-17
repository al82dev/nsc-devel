# Tanda NSC 3.0.5 L06D - granularidad cold O0

## SCOPE LOCK

- Objetivo exacto: reducir el costo frio de `llvm-object-generation` evitando que `-j` cree mas
  unidades LLVM O0 de las que el backend puede procesar con ganancia real.
- Naturaleza: optimizacion interna del proceso de compilacion.
- Revision correcta de partida: `62343404b47492ef740f7d856b3f7f4839db3796`.
- Evidencia causal: la matriz cold O0 de tres ejecuciones por configuracion, guardada en
  `A:\temp\nsc305-l06-granularity-62343404b`, produjo medianas de 240440 us (j1),
  173341 us (j2), 140062 us (j4), 116180 us (j8) y 118553 us (j16). Entre j8 y j16
  el tiempo de pared no mejora, mientras el trabajo acumulado crece: lowering 103707 -> 132848 us,
  emision 217057 -> 282493 us, verificacion 80060 -> 118833 us y store 27948 -> 43647 us.
- Operacion responsable: `ProjectOopObjectGenerator::generate` usa `workerCount` tanto para
  dimensionar el pool como para decidir cuantas particiones LLVM construir.
- Invariante violado: aumentar concurrencia no debe multiplicar trabajo fijo por modulo cuando ya
  no reduce el camino critico.
- Archivos autorizados:
  - `src/LLVM/ProjectOopObjectGenerator.cpp`;
  - `tests/unit/ProjectOopLoweringTests.cpp`;
  - este expediente;
  - `A:\handover-deepseek\restantes-optimizacion-deepseek.md` al cerrar la sub-tanda.
- Simbolos autorizados:
  - `ProjectOopObjectGenerator::generate`;
  - la prueba focal de generacion particionada en `ProjectOopLoweringTests.cpp`.
- Preservar exactamente: sintaxis, semantica, diagnosticos, ABI, NSMETA, nombres de simbolo,
  contenido de cada funcion, formato COFF, orden determinista, invalidacion y restauracion de cache.
- Cambio permitido: en O0, construir como maximo ocho unidades de codigo y conservar el numero de
  workers solicitado para procesarlas. Los perfiles O1/O2/O3 y LTO conservan su politica actual.
- Prohibido: ocultar el costo cold con hits, modificar el linker, cambiar contratos publicos,
  ejecutar baseline/bateria completa, instalar, desplegar o usar otra unidad para evidencia.
- Pruebas: target focal `nsc_project_oop_lowering_tests`, build focal de `nsc.exe`, matriz O0 cold
  con cache unica por ejecucion y runtime ubicado en A:.
- Criterio de terminacion: la prueba focal pasa, j16 O0 produce como maximo ocho unidades de codigo
  mas el coordinador, el objeto sigue verificando/enlazando en la prueba focal y la mediana cold no
  empeora respecto de la evidencia anterior.

## Resultado

- `ProjectOopObjectGenerator::generate` separa ahora concurrencia de ejecucion y granularidad:
  conserva `workerCount` para el pool, pero usa como maximo ocho particiones en O0.
- El plan persistente de particiones se identifica por la granularidad efectiva, por lo que la
  restauracion no mezcla planes de ocho unidades con planes producidos por otra politica.
- `nsc_project_oop_lowering_tests`: `PASS 101 checks`, incluida la nueva asercion j16 O0 <= 8
  unidades y los enlaces particionados O0/O2.
- `nsc.exe` construido y contrato de runtime estatico del host aprobado.
- Comparacion cold posterior, cinco ejecuciones por configuracion:
  - j8: mediana 114843 us, nueve modulos fisicos (coordinador + ocho unidades);
  - j16: mediana 105845 us, nueve modulos fisicos (coordinador + ocho unidades).
- Contra la mediana j16 previa de 118553 us y doce modulos, j16 baja 10,7 % en el corpus focal.
  La mejora no usa hits: cada ejecucion empleo una raiz de cache nueva.
- Evidencia: `A:\temp\nsc305-l06d-post-62343404b`.

L06D queda cerrada. L06 permanece abierta: el proyecto grande todavia requiere eliminar trabajo
comun repetido dentro de las unidades restantes y validar su log cold real.
