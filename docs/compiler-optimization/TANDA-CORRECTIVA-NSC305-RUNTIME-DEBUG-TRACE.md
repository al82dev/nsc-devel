# Tanda correctiva NSC 3.0.5: traza Debug del programa generado

Fecha: 2026-10-16
Rama: `development`
Revisión correcta conocida para iniciar esta unidad: `82ab25f3336523935be9b415b4cb113baeaed02d`

## SCOPE LOCK

### Objetivo exacto autorizado

Instrumentar los ejecutables NativeSharp generados en modo Debug para obtener una traza persistente,
buscable y resistente al cierre anormal que identifique la clase, la función y el tipo de callable que
se estaba ejecutando, además de las operaciones de memoria/lifetime directamente representadas en MIR.
La finalidad inmediata es localizar la primera operación causal anterior al fallo externo
`0xC0000374 (STATUS_HEAP_CORRUPTION)` observado al ejecutar `al82dev`.

### Naturaleza del cambio

Infraestructura diagnóstica temporal y correctiva. No es una feature del lenguaje ni una modificación
del contrato de usuario.

### Entrada exacta y fallo observado

- Productor: `nsc.exe` de `development` construyendo el proyecto externo en `O0`.
- Resultado del productor: PASS externo en 115.3 s; el ejecutable fue generado.
- Consumidor: el ejecutable generado.
- Fallo: terminación aproximadamente 1.9 s después de iniciar con código `3221226356` / `0xC0000374`.
- Invariante violado: una operación de heap detectó corrupción de sus estructuras internas.
- Operación exacta responsable: todavía no demostrada; esta tanda produce la evidencia que falta.

### Evidencia externa posterior recibida durante la tanda

La instrumentación manual del consumidor aisló el último checkpoint en
`010 program.before-game-constructor`; no alcanzó `100 constructor.enter` y esa ejecución terminó con
`0xC0000005 (STATUS_ACCESS_VIOLATION)`. Esto acota el intervalo a la asignación del objeto, la invocación
del constructor o el constructor base anterior al cuerpo instrumentado, pero no demuestra por sí solo un
defecto de metadata ni una incompatibilidad entre generaciones de librerías. Las fechas distintas de los
artefactos son correlación, no causa. No se modifica NSMETA ni se ordena recompilar bindings en esta tanda.

### Hipótesis causal de esta unidad

La información actual no permite distinguir entre escritura fuera de límites, doble liberación,
liberación mediante un allocator incorrecto o corrupción previa detectada tardíamente. Una traza del
proceso compilador no puede resolverlo porque `nsc.exe` ya terminó correctamente cuando ocurre el fallo.
La unidad causal autorizada es, por tanto, instrumentar el programa generado sin cambiar su semántica.

### Presupuesto de cambio

| Dimensión | Autorizado |
|---|---|
| Repositorio | Exclusivamente `A:\nsc-devel` |
| Componente | Lowering MIR a LLVM del programa generado |
| Archivo funcional inicial | `src/LLVM/ProjectOopLlvmLowerer.cpp` |
| Símbolos | `OopLlvmImplementation`, definición de funciones MIR y puntos MIR de allocation/deallocation |
| Perfil | Sólo `DebugInformationKind::Full` |
| Salida | `A:\temp\nsc-runtime-debug-trace.log` |
| Pruebas | Build dirigido de `nsc`; fixture O0+Debug focal; inspección de IR/ejecución dirigida |

Si la prueba demuestra que el helper necesita una importación Win32 que no está en la tabla compartida,
se ampliará este mismo documento antes de tocar esa tabla. No se autoriza modificar consumidores externos.

### Comportamiento protegido

- Release, `-g0` y line-tables-only no contienen la instrumentación ni pagan su costo.
- Sintaxis, semántica, ABI, NSMETA, mangling, ownership, allocator provenance y layout permanecen idénticos.
- O0, O1, O2 y O3 conservan su significado; `-O0` no se usa como sinónimo de Debug.
- No se reactivan versiones de NSMETA ni rechazo por versión.
- No se modifica ningún juego, binding, librería ni instalación externa.
- La instrumentación de genéricos preservada bajo `INSTRUMENTATION CODE` sigue desactivada.

### Archivos y subsistemas protegidos

- Parser, lexer, sema, generics, HIR, MIR builder, metadata/NSMETA, linker y runtime público.
- `A:\native-sharp-3.0-baseline`, `C:\native-sharp`, juegos, bindings y otros repositorios.
- Pruebas, manifiestos y expectativas existentes no se adaptan para ocultar un fallo.

### Acciones prohibidas

- Baseline, batería completa, deploy, instalación, push, pull o fetch.
- Escribir fuera de `A:`.
- Usar MSYS/MSYS2/MinGW/Cygwin o rutas absolutas de toolchain.
- Emitir la traza por consola.
- Reservar memoria dinámica dentro del sink de traza generado.
- Corregir el heap por tanteo antes de identificar la primera operación causal.
- Instrumentar LLVM u otro repositorio porque un nombre aparezca en una pila sin demostrar su propiedad.

### Diseño fijado

La instrumentación tiene dos compuertas:

1. **Compuerta de generación:** sólo `DebugInformationKind::Full` emite helpers, strings y llamadas.
2. **Flag/filtro global del módulo Debug:** una máscara permite habilitar funciones y memoria. Se
   inicializa activa durante el diagnóstico y puede quedar en cero al aparcar el código.

El lowerer genera automáticamente eventos para todas las funciones MIR; no se editan manualmente cientos
de clases. Cada registro incluye una etiqueta estable independiente del defecto, evento, clase/tipo,
callable y clase de callable. El sink usa strings estáticos, `CreateFileA`, `WriteFile` y `CloseHandle`;
no usa el heap que está bajo investigación. La ruta se abre en append por evento para que el prefijo ya
escrito sobreviva a una terminación anormal.

```dot
digraph runtime_debug_trace {
  rankdir=LR;
  nsc [label="nsc: MIR -> LLVM"];
  gate [label="DebugInformationKind::Full"];
  calls [label="entry/exit + memory events"];
  sink [label="fixed-buffer Win32 sink\n(no heap)"];
  log [label="A:\\temp\\nsc-runtime-debug-trace.log"];
  nsc -> gate -> calls -> sink -> log;
}
```

La fuente DOT queda embebida para reproducibilidad. `dot` no está disponible en el PATH nativo al iniciar
esta tanda, por lo que no se genera una figura SVG y no se instala ni sustituye Graphviz silenciosamente.

### Criterio exacto de terminación de la instrumentación

- El build focal de `nsc` termina con código cero.
- Un ejecutable `-g` emite registros de entrada/salida con clase y función al archivo de `A:\temp`.
- Un artefacto `-g0` no contiene el helper, la ruta ni las llamadas.
- El sink no realiza asignaciones de heap.
- La traza queda etiquetada como `INSTRUMENTATION CODE` para localizarla y aparcarla después del defecto.
- Se crea un commit local aislado con código, prueba focal y esta evidencia.

Este criterio termina la unidad diagnóstica, no la tarea persistente. El defecto `0xC0000374` sólo se
considerará corregido después de identificar su primera causa, aplicar una corrección independiente,
recompilar y ejecutar de nuevo el consumidor afectado.

## Implementación realizada

La instrumentación reside únicamente en `ProjectOopLlvmLowerer.cpp` y queda marcada mediante comentarios,
símbolos y registros que contienen `INSTRUMENTATION CODE`.

- `__nsc_runtime_debug_trace` abre el archivo fijo en modo append, escribe un registro y cierra el handle.
- `__nsc_runtime_debug_trace_flags` es una máscara privada por módulo: bit 0 para callables y bit 1 para
  memoria. Su valor diagnóstico inicial es `3`.
- Cada función MIR genera `ENTER` al comenzar su bloque de entrada.
- `EXIT` se genera después de los cleanups y de liberar capture cells, inmediatamente antes del retorno
  real. No se etiqueta como salida un `yield`, una suspensión ni una expresión divergente.
- Las rutas del allocator y deallocator estándar quedan envueltas con `ALLOCATE` y `FREE-BEGIN`.
- El sink no reserva memoria del heap: usa strings globales estáticos y almacenamiento de pila para el
  resultado de `WriteFile`.
- Las declaraciones de `CreateFileA`, `WriteFile` y `CloseHandle` se registran en el catálogo existente de
  funciones para reutilizar una declaración compatible y no producir símbolos duplicados.

La traza del juego que el usuario está agregando manualmente es evidencia complementaria y debe conservar
otro archivo o etiqueta. No sustituye ni se mezcla con el prefijo automático
`[NSC RUNTIME DEBUG INSTRUMENTATION CODE]`.

## Evidencia dirigida

Revisión base examinada: `82ab25f3336523935be9b415b4cb113baeaed02d`.

1. `cmake --build build-clang --target nsc --parallel 16`: PASS.
   - Build hash resultante: `e71666a4e3dde2e2`.
   - Contrato de host runtime estático: PASS.
2. Emisión focal del fixture `tests/performance/p305/generics-32/generics-32.nsproj` con
   `-O0 --profile=debug`: PASS.
   - Objeto: `A:\temp\nsc-generics-32-debug-trace.obj`.
   - Máquina: `0x8664`; 12 secciones; debug information `full`.
   - La inspección de strings encontró `ENTER` y `EXIT` para `Program.Main` y `Program` constructor,
     además de `ALLOCATE` y `FREE-BEGIN`.
3. Emisión del mismo fixture con `-O0 -g0`: PASS.
   - Objeto: `A:\temp\nsc-generics-32-no-trace.obj`.
   - Máquina: `0x8664`; 7 secciones; debug information `none`.
   - Coincidencias de etiqueta, ruta y símbolos `__nsc_runtime_debug_*`: cero.
4. Prueba contrafactual de imports mediante un proyecto temporal fuera del repositorio: PASS para
   declaraciones de usuario de `CreateFileA` y `CloseHandle`; no aparecieron sufijos duplicados. El lenguaje
   rechaza una declaración de usuario de `WriteFile` porque ese nombre está reservado por el toolchain, no
   por un defecto introducido en esta tanda.

## Validación pendiente y bloqueo de esta máquina

No se afirma todavía que el archivo físico haya sido escrito por un ejecutable. La construcción focal de un
`.exe` recorrió todas las fases de NSC y la emisión de objeto, pero el paso de enlace quedó bloqueado porque
`nslink.exe` no está disponible en el PATH nativo de esta máquina. Conforme a los lineamientos, no se buscó
en rutas absolutas, no se alteró PATH y no se sustituyó la herramienta.

La validación externa debe usar el compilador reconstruido y compilar el juego con información Debug completa:

```text
--profile=debug
```

o, de forma equivalente para esta compuerta:

```text
-g
```

`-O0` por sí solo no activa la instrumentación. Después de ejecutar el juego desde su GUI, la traza automática
debe quedar en `A:\temp\nsc-runtime-debug-trace.log`. La ausencia de ese archivo sólo es diagnóstica si el
comando de compilación confirma `profile-debug-information: full` o `debug-information: full`.
