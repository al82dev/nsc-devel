# Diseño propuesto: descarte de yield condicional cancelado

Estado: propuesta sin cambios de emisión. Revisión de referencia: `9f7bffd8c`.
El descarte debe preservar el contrato de `Cancel()` acordado: la limpieza ocurre
al siguiente avance y no se entrega otro elemento. La evidencia estática de
rama añadida en esa revisión es informativa; no prueba que un resultado deba
destruirse ni que la vida de un alias sea válida.

## Continuación de un finally escalar

La prueba dirigida `tests/integration/async_cancel_finally_continuation.py`
separó cancelar antes de entrar al finally de cancelar después de su primer
yield. Con el compilador aislado SHA-256
`cd8ac03cd68262e604cad9bf6319552c886ae5ce2421a14313103bcf59bb1154`,
ambas fuentes compilaron en O0/O2. La primera devolvió 22: el avance posterior
publicó otro elemento. La segunda devolvió 23: no publicó otro elemento,
pero el log de efectos no llegó al valor esperado 123. El control sin
cancelación devolvió 42 en ambos niveles. El caso de llamada desconocida
devolvió 72 y sirve solo para comparación diferencial. El runner terminó
con salida 1, como corresponde a las expectativas aún incumplidas.

El comando y configuración están en
`A:/nsc-fixes-evidencia/async-yield-scalar-baseline-20261112.cmd`; salidas,
fuentes y ejecutables generados están en
`A:/nsc-fixes-evidencia/async-yield-scalar-regression-20261112.log` y
`A:/nsc-fixes-evidencia/async-yield-scalar-regression-20261112/`. El SHA
identifica el binario usado; no se ha demostrado su correspondencia exacta
con un commit específico. El caso desconocido no valida por sí solo el orden
de limpieza, excepciones, ownership ni abandono.

Un prototipo externo de descarte escalar no se aplicó. La revisión encontró
que omitir la publicación de un yield no identifica dónde reanudar un finally
ya iniciado y podría alcanzar después un yield de procedencia desconocida.
Además, hay que preservar los estados de reanudación y la ruta de `Drop()`.
El diseño alternativo de reingreso se conserva en
`A:/nsc-fixes-evidencia/async-finally-20261111/cancel-finally-reentry-cfg-20261112.md`;
todavía no está implementado ni probado y exige revisar SEH, dominancia,
excepciones, `await`, cleanups anidados y propiedad.

## Evidencia previa y regresión preparada

`A:/nsc-fixes-evidencia/async-yield-mixed-identity-restored.log` registró en
O0/O2 salida 22 para ambas selecciones: se entregó otro elemento. El caso cuya
fábrica lanza terminó con `0xe04e5301` en ambas configuraciones. Esa evidencia
corresponde al compilador restaurado indicado en
`async-yield-mixed-prototype-20261112.md`, no a una ejecución nueva de 9f7bffd8c.
El prototipo documentado allí arregló únicamente las dos ramas sin excepción y
fue retirado; no demuestra una solución general.

`tests/integration/async_yield_mixed_cancellation.py` prepara la reproducción
con fuente y proyecto separados por caso, sin modificar los originales. Comprueba
O0/O2, condición con efecto exactamente una vez, continuidad de `finally`,
identidad del objeto destruido, preservación del alias y propagación de la
excepción. La línea base actual se ejecutó con:

```text
python -B tests\integration\async_yield_mixed_cancellation.py A:\nsc-fixes-evidencia\build\bin\nsc.exe A:\nsc-fixes-evidencia\async-yield-mixed-current-20261112
```

El archivo `A:/nsc-fixes-evidencia/async-yield-mixed-baseline-20261112.cmd`
conserva configuración de temporales, linker y rutas de runtime/importación en
A; el log homónimo contiene comandos y resultados. El compilador aislado tuvo
SHA-256 `cd8ac03cd68262e604cad9bf6319552c886ae5ce2421a14313103bcf59bb1154`.
Las seis compilaciones pasaron. `owned` y `alias` devolvieron 22 en O0/O2:
el avance cancelado produjo otro elemento. `throw` terminó con 3763229441
(`0xe04e5301`) en O0/O2. El runner terminó con salida 1; los casos permanecen
pendientes. En `owned` y `alias`, la salida anticipada 22 impidió observar las
comprobaciones posteriores de identidad, efectos y destrucción; no se deben
presentar como fallidas ni como aprobadas en esta ejecución.

## Representación y emisión candidatos

1. Modelar el descarte como una operación contextual del resultado del
   operando, no como propiedad fija del binding raíz. Para un condicional bool
   escalar validado, conservar condición y ambas ramas como subárboles y asociar
   a cada ruta su propia evidencia. La condición se evalúa una vez; solo se
   evalúa la rama elegida. La máscara SIMD requiere otra semántica y queda fuera
   de esta regla. Las ramas con `Unknown` no se convierten en owned ni alias por
   eliminación o por los flags de su padre.
2. En una ruta cuya propiedad y operación de liberación estén demostradas, emitir
   la evaluación completa, activar exactamente una limpieza del resultado solo
   después de obtenerlo con éxito y desactivarla al destruirlo. La destrucción
   debe usar el contrato existente del tipo, incluido el despacho dinámico y la
   propiedad de almacenamiento: `owned` en un binding por sí solo no prueba la
   operación concreta ni justifica asumir heap ordinario.
3. En la ruta de campo estático sin ownership comprobado, evaluar la lectura y
   conservar el alias sin invocar destructor ni liberar su almacenamiento. La
   ausencia de ownership es una observación de esa ruta, no una prueba general
   de duración, de falta de efectos o de inocuidad del subárbol.
4. Una estrategia válida es emitir cada rama con su operación de descarte en su
   propio bloque y unir únicamente control, evitando convertir el alias y el
   owner en un valor sin identidad. Si el resultado debe cruzar un punto de
   suspensión, se requiere un estado persistente explícito con tag, valor,
   indicador de inicialización y transferencias exactas: un tag local no basta.
   No utilizar una segunda evaluación de la condición para reconstruir el tag.
5. `FullExpressionCleanup` y `OwnedTemporaryBorrow` son límites de cleanup:
   no eliminarlos ni aplanarlos. Conservar regiones, slots e indicador de
   inicialización, secuencia y límites del árbol original. Antes de admitir una
   ruta donde el resultado pueda referirse a un temporal cerrado por el wrapper,
   demostrar su vida y el orden del descarte; si falta evidencia, `Unknown`.
6. Toda evaluación que pueda lanzar debe usar la ruta de excepción existente;
   una construcción fallida no activa el destructor del resultado. Las regiones
   ya activas se limpian una vez, el alias no se destruye, y la excepción debe
   propagarse al catch previsto. El resultado histórico `0xe04e5301` impide
   considerar demostrada esta parte; aislar causa y regresión antes de integrar.
7. El emisor debe consumir una representación estable y explícita que identifique
   función, yield y ruta. No usar direcciones de nodos como identidad persistente.
   Si una decisión pasa a afectar la salida LLVM, incorporar su contenido y
   versión a la identidad pre-LLVM de la caché de objetos, o invalidarla de forma
   equivalente; comprobar objeto frío/cálido y cambios semánticos. Reconstruir
   los hechos desde bindings al restaurar MIR no invalida por sí mismo un objeto
   ya emitido con otra decisión.

## Casos todavía no autorizados por la evidencia

No se ha demostrado la emisión para `Unknown`, referencias retornadas, ramas
anidadas o invertidas, conversiones, wrappers con alias, destrucción que lanza,
propiedad transferida por llamadas generales, suspensión, abandono ni máscara
SIMD. Mantener esos casos fuera de la nueva ruta hasta contar con evidencia y
pruebas; no inferir permiso de liberar de un `StaticFieldNonOwning` o de
`SemanticFreshOwnedTransfer`. Cualquier política visible para un caso sin
clasificación debe evaluarse explícitamente con el integrador; el diseño no
autoriza introducir un error nuevo ni descartar silenciosamente el valor.

La siguiente validación dirigida debe repetir el runner con NSC/SDK aislados
después de un cambio y distinguir fallos previos de regresiones; añadir
controles con wrappers, selección invertida y anidada, excepción y suspensión
antes de ampliar la ruta admitida. Las comparaciones de caché MIR existentes no
sustituyen controles de caché de objetos una vez que se modifique la emisión.

## Derivación efímera del plan por función

**Propuesta, sin consumidor implementado.** No se necesita una segunda tabla
persistente para representar las ramas. `asyncYieldFacts` conserva la identidad
de yield, la posición del nodo en preorder, el padre, el índice de hijo y la
evidencia semántica de cada rama; el cuerpo MIR conserva nodos, hijos, wrappers
y regiones de cleanup. El futuro consumidor puede derivar un plan local durante
la emisión y descartarlo al terminar la función. No usar direcciones de nodos
como identidad serializada ni duplicar los hechos en una tabla duradera.

Algoritmo candidato por función:

1. Recorrer iterativamente el cuerpo MIR en el mismo preorder usado al generar
   los hechos y construir una correspondencia temporal índice-nodo. Indexar los
   hijos de cada hecho por `parentFact` y `childIndex`. Validar límites,
   unicidad, padre anterior, pertenencia al mismo yield y correspondencia real
   con el hijo MIR; cualquier discrepancia deja el caso sin plan ejecutable.
2. Para cada raíz de yield que vaya a procesarse, derivar una descripción local
   del operando desde sus hechos y el árbol MIR real. No usar únicamente el tipo
   del binding: el control SIMD sintético demostró que el tipo MIR de la
   condición puede diferir. Una selección escalar requiere tres hijos presentes,
   condición MIR y binding `builtin:bool`, y tipos de resultado coherentes.
3. Conservar la condición y el orden verdadero/falso. Consultar la evidencia de
   cada rama por separado; un `Unknown` permanece explícito y no se convierte
   en alias ni owner por la evidencia del hermano. La descripción puede contener
   rutas parcialmente conocidas para diagnóstico, pero la futura emisión no debe
   empezar parcialmente antes de confirmar el contrato de todas las rutas que
   esa emisión necesita manejar.
4. Tratar `FullExpressionCleanup` como frontera que contiene su subárbol, no
   como prueba de transparencia de propiedad. Conservar la asociación con las
   regiones `OwnedTemporaryBorrow`, sus identidades de local y cleanup y el
   orden del árbol. Una operación opaca mantiene su expresión completa; no se
   extraen llamadas o efectos para clasificarlos por separado.
5. Si se procesa una selección anidada, repetir el procedimiento sobre la ruta
   correspondiente sin reevaluar su condición. No confundir evidencia de una
   rama hija con la propiedad de su padre. El diseño no declara soportados los
   nodos anidados: su emisión requiere pruebas específicas.

La indexación cuesta O(n + f) tiempo y espacio temporal para n nodos MIR y f
hechos; la consulta posterior no debe buscar linealmente todo el árbol por cada
rama. La memoria queda limitada a la función y se libera al terminarla. Una
construcción bajo demanda por yield también es válida si evita trabajo cuadrático.
El plan no se persiste ni cambia la caché MIR. Si llega a influir en LLVM, la
identidad de caché de objetos debe incluir una versión del algoritmo y las
entradas semánticas que cambien su decisión, o invalidar equivalentemente: una
comparación de hechos restaurados de MIR no basta para demostrarlo.

Invariantes antes de permitir un consumidor: la condición se evalúa una sola
vez; solo se evalúa la ruta escalar elegida; no se destruye el alias; cada owner
completamente construido se transfiere o destruye exactamente una vez con la
operación apropiada; no se destruye un resultado cuya construcción falló; las
regiones activas se limpian según su alcance; se conserva la excepción y se
reanuda la limpieza pactada. Si hay suspensión, la ruta elegida, la propiedad y
el estado de inicialización deben sobrevivir en el frame: el plan efímero no
sustituye ese estado runtime. Una forma no demostrada conserva `Unknown` y no
autoriza ni una liberación ni una omisión de cleanup.

| Control dirigido | Evidencia requerida antes de consumir el plan |
|---|---|
| Rama owned / alias, clase y namespace | Identidad de rama y estado semántico correctos; condición una vez; destruye solo owner. |
| Binding o hijo ausente, modo desconocido, tipos incompatibles | `Unknown` o plan no ejecutable; ninguna suposición desde el padre. |
| Condición bool frente a máscara SIMD | Rama escalar solo con tipo MIR y binding bool; SIMD no entra en esa ruta. |
| Wrapper y temporal con cleanup | Subárbol, identidad de región y orden de limpieza preservados; alias de temporal no promovido sin prueba de vida. |
| Excepción antes y después de construcción | Catch y cleanup correctos, sin destructor de objeto no construido ni doble liberación. |
| Condicional invertido y anidado | Ruta elegida y efectos exactos; sin repetir condición; `Unknown` de rama conservado. |
| Suspensión, cancelación y abandono | Estado persistente e inicialización exactos, cleanup una vez, sin yield tras cancelación. |
| Caché MIR fría/cálida y trabajo paralelo | Hechos y plan derivado idénticos; binding ausente sigue desconocido. |
| Caché de objetos después de activar emisión | Miss o invalidación ante cambio de algoritmo/entradas; misma conducta fría/cálida. |

Estas son condiciones de aceptación futuras, no resultados de prueba. La línea
base mixta y la evidencia del wrapper ya documentadas siguen pendientes en sus
límites: no acreditan propagación de excepción ni descarte general correcto.
