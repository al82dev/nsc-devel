# Diseño propuesto: descarte de yield condicional cancelado

Estado: propuesta sin cambios de emisión. Revisión de referencia: `9f7bffd8c`.
El descarte debe preservar el contrato de `Cancel()` acordado: la limpieza ocurre
al siguiente avance y no se entrega otro elemento. La evidencia estática de
rama añadida en esa revisión es informativa; no prueba que un resultado deba
destruirse ni que la vida de un alias sea válida.

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
