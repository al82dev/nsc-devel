# Tanda correctiva NSC 3.0.5: breadcrumbs de stack overflow en method-bodies

Fecha: 2026-10-16
Rama: `development`
Revisión inicial: `560340af5011b66234d367be876df51b38a03df0`

## SCOPE LOCK

### Objetivo exacto autorizado

Instrumentar temporalmente la fase `method-bodies` del compilador para identificar el callable exacto cuya
evaluación no regresa antes del fallo externo `0xC00000FD (STATUS_STACK_OVERFLOW)` observado al producir una
biblioteca grande. La traza debe sobrevivir al aborto del proceso y escribirse exclusivamente en
`A:\temp\nsc-method-bodies-trace.log`.

### Evidencia disponible

- La compilación alcanza `method-bodies` y el proceso termina con `0xC00000FD`.
- El resultado es idéntico con `-j1`; no depende del paralelismo.
- Los commits L04 sólo modifican `ProjectOopLlvmLowerer.cpp`, ejecutado en `llvm-lowering`, por lo que no son
  causa directa de un proceso que no abandona `method-bodies`.
- El failure bundle ordinario se publica después de que `Driver::run` retorna. Un stack overflow que aborta el
  proceso puede impedir su publicación y no constituye un breadcrumb suficiente.

### Hipótesis causal de esta unidad

Una ruta recursiva dentro del análisis semántico de un callable concreto excede la pila. Todavía no está
demostrado qué callable ni qué función recursiva participa. Esta unidad no corrige por tanteo: produce la
identidad mínima necesaria para formular la siguiente hipótesis.

### Presupuesto de cambio

| Dimensión | Autorizado |
|---|---|
| Repositorio | Exclusivamente `A:\nsc-devel` |
| Componente | `ProjectMethodBodyModel` |
| Archivo funcional | `src/Frontend/ProjectMethodBodyModel.cpp` |
| Símbolos | helper privado de breadcrumbs, `build`, `analyzeCallableIsolated` |
| Salida | `A:\temp\nsc-method-bodies-trace.log` |
| Pruebas | build dirigido de `nsc`; fixture focal de `--check-project-method-bodies` |

### Comportamiento protegido

- Sintaxis, semántica, diagnósticos, ABI, NSMETA, mangling, ownership y layout no cambian.
- No se recompila, reemplaza ni modifica ninguna biblioteca, binding, juego o instalación.
- No se infiere incompatibilidad binaria a partir de fechas de artefactos.
- L04, generics y la instrumentación del ejecutable generado permanecen intactos.

### Acciones prohibidas

- Baseline, batería completa, deploy, instalación, push, pull o fetch.
- Escrituras fuera de `A:`.
- Cambiar NSMETA o añadir rechazo por versión.
- Corregir una función recursiva hasta que el callable y la cadena causal estén identificados.
- Imprimir los breadcrumbs en consola.

### Diseño fijado

La instrumentación sobrescribe el archivo al comenzar cada build de `method-bodies` y abre, escribe, vacía y
cierra el archivo en cada frontera. Registra:

- `BUILD-BEGIN` con cantidades de miembros y funciones de namespace;
- `WORKLIST-READY` con el número de callables iniciales;
- `WAVE-BEGIN` y `WAVE-END`;
- `CALLABLE-BEGIN` y `CALLABLE-END`, con wave, ordinal, nombre, signature key y fuente.
- fronteras de inicializadores, chequeos posteriores, escape summaries y `BUILD-END`.

El archivo usa una exclusión mutua sólo durante cada escritura para no intercalar registros cuando `-j` es
mayor que uno. La reproducción causal recomendada sigue siendo `-j1`: el último `CALLABLE-BEGIN` que carezca
de `CALLABLE-END` identifica el cuerpo en ejecución cuando se agotó la pila.

### Criterio de terminación

- `nsc` compila con código cero.
- Un fixture focal de `method-bodies` termina y deja pares BEGIN/END balanceados.
- La reproducción externa `-j1` deja un último callable inequívoco antes de `0xC00000FD`.
- Código y expediente quedan en un commit local aislado.

Esta unidad sólo cierra la adquisición de evidencia. La regresión se cierra después de convertir la ruta
recursiva demostrada a una forma acotada/iterativa, ejecutar la prueba focal y volver a producir la biblioteca
sin stack overflow.

## Implementación y evidencia dirigida

La instrumentación se implementó exclusivamente en `ProjectMethodBodyModel.cpp`. Usa un mutex sólo mientras
abre, escribe, vacía y cierra cada registro; no conserva un stream abierto ni depende de que el proceso alcance
un destructor después del fallo.

1. `cmake --build build-clang --target nsc --parallel 16`: PASS.
   - Build hash: `a6b04f0483451f2c`.
   - Contrato de host runtime estático: PASS.
2. Fixture focal:
   `--check-project-method-bodies tests/performance/p305/generics-32/generics-32.nsproj -O0 -j 1`: PASS.
3. La traza focal contiene:
   - `BUILD-BEGIN` con 33 miembros, cero funciones de namespace y un worker;
   - un callable `Main()` con `CALLABLE-BEGIN` y `CALLABLE-END` balanceados;
   - `WAVE-END`, fronteras de inicializadores y post-análisis, y `BUILD-END`.
4. No se ejecutó baseline, batería completa, deploy ni instalación.

## Validación externa pendiente

La otra máquina debe reconstruir `nsc`, repetir exclusivamente la producción fallida con `-j1` y conservar
`A:\temp\nsc-method-bodies-trace.log`. Si el último registro es `CALLABLE-BEGIN`, el mismo registro incluye
ordinal, nombre, signature key y fuente. Si el último registro es una frontera posterior, la siguiente tanda
instrumentará únicamente esa subfase. La biblioteca previa y su metadata permanecen intactas hasta demostrar
la causa binaria o semántica correspondiente.
