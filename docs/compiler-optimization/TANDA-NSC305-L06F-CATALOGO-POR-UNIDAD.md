# Tanda NSC 3.0.5 L06F - catálogo LLVM por unidad

## SCOPE LOCK

- Objetivo exacto: dejar de recorrer y materializar en cada módulo LLVM el catálogo completo de
  símbolos precompilados cuando la unidad sólo necesita una clausura pequeña.
- Revisión correcta de partida: `be64afe9d`.
- Evidencia de entrada: un programa mínimo con catálogo precompilado amplio contiene `8.602`
  funciones MIR (`8.505` importadas), `14.702` campos estáticos, `354` layouts de valor y `275`
  tablas de despacho. Sus ocho unidades ordinarias gastan `2,42-2,64 s` cada una en lowering y el
  coordinador `3,67 s`; emisión, verificación y store son milisegundos.
- Hipótesis: la partición limita cuerpos pero no todo el universo de declaraciones auxiliares.
  `declareStaticFields`, inicializadores de tipo, entradas de destrucción, value-drop, metadata de
  despacho y algunos helpers recorren el modelo global en cada unidad.
- Invariante: una unidad conserva toda declaración alcanzada directa o indirectamente por sus
  cuerpos, limpiezas, tablas, campos y helpers; los símbolos no alcanzados permanecen disponibles
  en MIR/metadata pero no se materializan en ese módulo LLVM.
- Algoritmo: construir clausuras iterativas y deterministas por code unit; separar definiciones,
  declaraciones de funciones, campos y metadata auxiliar; el coordinador conserva sólo los
  símbolos necesarios para storage, wrappers, tablas y field initializers que realmente define.
- Protegido: sintaxis, ABI, NSMETA, nombres físicos, formato COFF, diagnósticos, semántica de
  inicialización/destrucción, despacho virtual/interface, excepciones, callbacks y caché.
- Prohibido: ocultar el costo con hits, borrar superficie del modelo, rechazar metadata por versión,
  usar recursión nueva, modificar bibliotecas consumidoras o ejecutar baseline/batería completa.
- Prueba roja: una partición con una función local y catálogo no usado no debe contener firmas,
  accessors, storage, cctors, descriptores ni entradas de destrucción ajenos.
- Validación: prueba estructural roja/verde, verifier, enlace/ejecución focal, pruebas de lowering y
  una matriz cold de cinco ejecuciones sin hits sobre el programa mínimo amplio.

## Resultado

### Causa confirmada

La telemetría ampliada demostró que las unidades ordinarias ya recibían únicamente `1-3`
declaraciones de función, pero cada una seguía tardando `2,37-2,61 s`. El trabajo restante no era
el cuerpo de esas funciones: cada lowerer recorría y materializaba los `14.702` campos estáticos
importados. El coordinador repetía además las `8.602` firmas aun cuando no define cuerpos.

### Corrección

- Se construye una máscara determinista e iterativa de campos por módulo.
- Un módulo monolítico conserva el modelo completo.
- Una unidad ordinaria conserva sólo campos referidos por sus cuerpos, cleanup regions y salidas
  no locales.
- El coordinador conserva todos los campos locales y las dependencias de sus inicializadores; no
  materializa campos importados que no usa.
- El coordinador declara funciones locales alcanzables y callees requeridos por inicializadores
  locales; ya no declara por defecto todo el catálogo importado.
- La traza automática pasó a esquema 3 e informa `definitions`, `declarations` y `static-fields`
  por módulo.
- El dump MIR informa la procedencia `imported` de cada campo.

La corrección no cambia sintaxis, ABI, NSMETA, nombres físicos, metadata publicada ni formato de
objeto. La selección ocurre después de MIR y sólo decide qué símbolos disponibles necesita cada
módulo físico.

### Pruebas focales

- `nsc_project_oop_lowering_tests`: `PASS 108/108`. La nueva regresión construye dos campos
  estáticos y demuestra que la unidad de `Read` contiene el campo usado pero no el ajeno.
- `nsc_cross_library_inlining_tests`: `PASS 16/16`. El fixture incluye un inicializador estático
  local que llama una función precompilada, por lo que también fija la dependencia legítima del
  coordinador; el ejecutable enlaza y termina con exit 0.
- `nsc.exe` reconstruido y contrato de runtime host estático aprobado.
- Ejecutable del programa mínimo: exit 0 tanto cold como warm.
- Repetición warm: `9 hits / 0 misses`, LLVM `185.198 us`, exit 0.

### Matriz cold sin hits

Cinco ejecuciones, `O0`, `-g0`, `-j16`, caché nueva por corrida:

| Corrida | LLVM total (us) | Lowering coordinador (us) | Total (ms) |
|---:|---:|---:|---:|
| 1 | 956.422 | 823.362 | 5.053 |
| 2 | 977.961 | 845.144 | 5.292 |
| 3 | 972.896 | 840.162 | 5.269 |
| 4 | 978.445 | 846.463 | 5.184 |
| 5 | 1.003.852 | 870.605 | 5.212 |

Medianas:

- LLVM: `3.843.405 us -> 977.961 us`, reducción de `74,6 %`;
- total focal: aproximadamente `8.102 ms -> 5.212 ms`, reducción de `35,7 %`;
- unidades ordinarias: aproximadamente `2,4-2,6 s -> 6,6-7,7 ms` de lowering;
- campos estáticos por unidad en este programa: `14.702 -> 0`;
- declaraciones del coordinador: `8.602 -> 12`.

Toda la matriz fue cold y reportó `0 hits / 9 misses`; la mejora no está escondida por caché.

### Evidencia

- conteo previo ampliado: `A:\temp\nsc305-l06-largecatalog\l06f-counts`;
- contrafactual de campos: `A:\temp\nsc305-l06-largecatalog\l06f-static-prune-one`;
- coordinador podado: `A:\temp\nsc305-l06-largecatalog\l06f-coordinator-one`;
- cinco cold y warm: `A:\temp\nsc305-l06-largecatalog\l06f-five-cold`;
- pruebas focales finales: `A:\temp\nsc305-l06f-tests3` y
  `A:\temp\nsc305-l06f-tests4`.

L06F queda cerrada. L06 general permanece abierta hasta contrastar el próximo log del proyecto
real: si el coordinador sigue dominando, el siguiente corte debe podar cctors/descriptores y
layouts auxiliares por tipo; si LLVM ya cae como en esta sonda, el cuello se transfiere a frontend
incremental (`generics.visibility`, `names`) y enlace.
