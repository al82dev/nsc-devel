# Tanda correctiva NSC 3.0.5: ABI de objetos y procedencia de allocator

## SCOPE LOCK

### Objetivo exacto autorizado

Corregir la regresion que permite compilar un consumidor con el `nsc` actual contra bibliotecas
precompiladas validas, pero corrompe memoria antes de entrar al constructor del consumidor. La
correccion debe restaurar el layout fisico estable de clases y conservar la procedencia de
allocators personalizados sin depender de bytes nuevos dentro del objeto.

### Naturaleza del cambio

Correccion funcional de ABI interno/publicado y del lowering LLVM de construccion y destruccion.
No es una ampliacion de sintaxis, una migracion de metadata, un refactor general ni una tanda de
optimizacion.

### Evidencia de entrada

- `al82dev-startup-checkpoint.log`: ultimo checkpoint
  `010 program.before-game-constructor`; el cuerpo de `MyGame` no comienza.
- `nsc-runtime-debug-trace.log`: dos inicializaciones equivalentes de diccionarios dejan de
  avanzar en fronteras distintas; la ubicacion aparente del fallo no es determinista.
- `new Game()` reproduce el fallo sin codigo especifico del consumidor.
- El commit `dced1bfbdad61b03101ec5fc8c3234ad60435a93` incremento el encabezado de toda
  clase de 8 a 24 bytes y desplazo todos los campos 16 bytes.
- La metadata ya publica `size`, `alignment`, `headerSize` y offsets, pero el frontend reconstruye
  clases importadas y especializaciones genericas con el layout del compilador consumidor.

### Operacion fallida e invariante violado

Codigo precompilado reserva objetos con el contrato historico de encabezado de un puntero. Codigo
generico materializado por el consumidor actual escribe campos con un encabezado de tres punteros.
La misma entidad generica puede quedar enlazada entre ambos productores; por tanto, la escritura
del consumidor rebasa el bloque reservado por el productor.

Invariante obligatorio:

```text
object[0] == puntero a descriptor de ocho campos
primer campo de usuario == offset 8 en Win64 cuando no existe una base mayor
Allocate(bytes) y Deallocate(pointer) reciben el mismo puntero de objeto
```

### Revision conocida como correcta y rango auditado

- Layout conocido como correcto: padre de `dced1bfbd`.
- Introduccion causal: `dced1bfbd`.
- Seguimiento funcional de procedencia: `7726a11ef`, `1362da8fa`, `8c8f54c11`,
  `b31407edc` y `12c8fa928`.
- HEAD al abrir la tanda: `c2c33a2f1` sobre `development`.
- Rango causal auditado: `dced1bfbd^..c2c33a2f1` para los simbolos de layout,
  construccion, destruccion y deallocacion.

### Solucion permitida

1. Restaurar el encabezado fisico de clase a un solo puntero de descriptor.
2. Mantener sin cambios los ocho campos y posiciones del descriptor publico.
3. Para objetos de allocator personalizado solamente, crear un descriptor lateral por objeto que:
   - copie los ocho campos del descriptor real;
   - sustituya exclusivamente la entrada virtual `destroy-and-free`;
   - conserve fuera del objeto el contexto y la thunk de deallocacion.
4. Hacer que la entrada lateral destruya el objeto, llame la thunk con el puntero original y libere
   el descriptor lateral.
5. En fallo de constructor, liberar el bloque con la misma thunk y liberar el descriptor lateral,
   sin ejecutar dos veces el destructor.
6. Elegir desde el ownership MIR la entrada `destroy-and-free` para almacenamiento owned y la
   entrada object-only para raw placement o almacenamiento promovido al stack.

### Comportamiento que debe permanecer identico

- Sintaxis, semantica, diagnosticos y orden de evaluacion.
- ABI de los ocho campos del descriptor, RTTI, vtables y llamadas virtuales.
- Layout y offsets historicos de clases.
- Destruccion exactamente una vez y orden derivada/campos/base.
- `Allocate` recibe el tamano exacto del objeto.
- `Deallocate` recibe exactamente el puntero devuelto por `Allocate`.
- Heap normal usa `__nativesharp_free`.
- Raw placement y objetos promovidos al stack se destruyen, pero no liberan su almacenamiento.
- Allocator personalizado conserva contexto y thunk a traves de DLLs.
- Fallo de constructor devuelve el bloque al allocator sin ejecutar el destructor del objeto
  incompleto.
- La instrumentacion Full Debug vigente permanece activa y separada del defecto.

### Presupuesto de cambio

| Dimension | Autorizado |
|---|---|
| Repositorio | `A:\\nsc-devel` |
| Rama | `development` |
| Componentes | layout de clases, ownership semantico de placement/allocator, transporte HIR/MIR y lowering LLVM de objetos |
| Archivos | `include/nativesharp/Frontend/ProjectObjectLayoutModel.h`, `include/nativesharp/Frontend/ProjectOopMirModel.h`, `src/Frontend/ProjectObjectLayoutModel.cpp`, `src/Frontend/ProjectMethodBodyModel.cpp`, `src/Frontend/ProjectOopHirModel.cpp`, `src/Frontend/ProjectOopMirModel.cpp`, `src/LLVM/ProjectOopLlvmLowerer.cpp`, este expediente y pruebas focales existentes si requieren ajustar expectativas |
| Simbolos | `ClassObjectLayout`, `ProjectOopMirClassLayout`, `ProjectObjectLayoutModel::buildClassLayout`, `ProjectMethodBodyModel::analyzeExpression` para `PlacementConstructionExpression`, lowering HIR de `ProjectConstructionSiteKind::Allocator`, copia de layouts en `ProjectOopMirModel::build`, generacion de entradas de destruccion, `lowerConstruction`, `lowerPlacementConstruction`, `emitRaiiDestroy`, cleanup de constructor y helpers de procedencia |
| Configuraciones | build local existente en A: |
| Perfiles | O0 y Full Debug focal |

Todo elemento no enumerado permanece protegido.

### Enmienda causal 1: ownership de allocator placement

La primera ejecucion de TL-LANG-492 compilo y enlazo, pero termino con `exit=1`: el destructor o
la deallocacion no alcanzo el conteo esperado. La inspeccion contrafactual demostro que
`ProjectMethodBodyModel` asigna `storageOwnership=Borrowed` indistintamente a raw placement y a
`ProjectConstructionSiteKind::Allocator`; el lowering HIR repite esa clasificacion. Por ello
`emitRaiiDestroy` no puede elegir la entrada de liberacion para un objeto de allocator sin volver
a inspeccionar bytes del header.

Se amplia el presupuesto exclusivamente a esos dos puntos ya enumerados. La regla corregida es:

```text
raw placement       -> object lifetime owned, storage borrowed
allocator placement -> object lifetime owned, storage owned por la referencia de resultado
```

`Owned` significa aqui que el cleanup debe descargar la obligacion de almacenamiento; el sidecar
determina en runtime que la descarga se hace mediante el allocator original. No cambia la
propiedad del allocator ni convierte el bloque en memoria del heap por defecto.

### Subsistemas protegidos y acciones prohibidas

- No modificar parser, sintaxis, keywords, semantica publica ni contratos 3.0.4.
- No reintroducir versionado numerico, fingerprint o rechazo por version de NSMETA.
- No modificar bindings, bibliotecas consumidoras, juegos, NSLink ni NSDBG.
- No recompilar ni instalar dependencias externas para ocultar la incompatibilidad.
- No tocar ni escribir en `C:` o `X:`.
- No usar MSYS, rutas privadas de herramientas ni herramientas fuera del `PATH` nativo.
- No ejecutar baseline, bateria completa, deploy, instalacion, push, pull ni fetch.
- No retirar todavia la instrumentacion Full Debug solicitada por el usuario.

### Pruebas dirigidas inmutables

- Layout de objeto: `ProjectObjectLayoutModelTests`, `MacroAOopTests` y
  `Stage40StructValueAbiTests` deben conservar header de 8 bytes.
- Raw placement/pinning: `Item320ObjectStorageOwnershipTests` y
  `NativeSharp3T06PinningTests`.
- Procedencia personalizada: TL-LANG-492.
- Fallo parcial de constructor: TL-LANG-493.
- Cruce de modulo/DLL: TL-LANG-494.
- La reproduccion externa contra las bibliotecas preexistentes la ejecuta el usuario en la otra
  maquina despues del commit local.

### Criterio exacto de terminacion

La tanda queda lista para transferencia solamente cuando:

1. el layout vuelve a header de 8 bytes y las pruebas unitarias focales pasan;
2. TL-LANG-492, TL-LANG-493 y TL-LANG-494 pasan en O0;
3. raw placement y stack promotion destruyen sin liberar almacenamiento;
4. el LLVM producido supera su verifier;
5. el diff contiene una sola hipotesis causal y su expediente;
6. existe un commit local aislado y el worktree queda limpio.

El defecto de extremo a extremo no se declara cerrado hasta que el usuario compile y ejecute el
consumidor real contra las bibliotecas preexistentes y confirme que supera la construccion base.

## Implementacion aplicada

### Layout publico restaurado

- `ClassObjectLayout` y `ProjectOopMirClassLayout` ya no transportan offsets privados de
  procedencia.
- `ProjectObjectLayoutModel::buildClassLayout` vuelve a iniciar el payload en
  `reference.size` y publica `headerSize=reference.size`.
- En Win64, las clases sin base mayor vuelven a colocar el primer campo de usuario en offset 8.

### Descriptor lateral de allocator

`ProjectOopLlvmLowerer` crea el tipo privado:

```text
allocator_descriptor_sidecar {
    class_descriptor descriptor; // mismos ocho campos, en offset 0
    void* allocator_context;
    void* deallocate_thunk;
}
```

Al construir mediante `new (allocator)`:

1. se conserva exactamente el puntero de objeto devuelto por `Allocate`;
2. se reserva por el runtime un sidecar de 80 bytes;
3. se copian los 64 bytes del descriptor publico;
4. se sustituye en la copia solamente el campo 5, `destroy-and-free`;
5. se guardan contexto y thunk despues del descriptor;
6. `object[0]` recibe la direccion de la copia, sin agregar bytes al objeto.

La entrada privada ejecuta field 7 para destruir el objeto, llama
`thunk(context, object)` y finalmente libera el sidecar. Si falla el constructor, el cleanuppad
omite la destruccion del objeto incompleto, llama la misma thunk y libera el sidecar. Si falla la
reserva interna del sidecar despues de que el allocator entrego el bloque, primero devuelve el
bloque al allocator y despues emite `AllocationFailure`.

### Seleccion de cleanup

- heap ordinario: descriptor estatico, field 5, destructor + `__nativesharp_free`;
- allocator personalizado: descriptor lateral, field 5 sustituido, destructor + thunk original;
- raw placement: descriptor estatico, field 7, destructor sin liberar almacenamiento;
- stack promotion: descriptor estatico, field 7, destructor sin liberar el frame;
- temporal fresh-owned: field 5;
- parametro/field owned y `delete`: conservan dispatch dinamico por field 5.

## Validacion dirigida ejecutada

Build focal del compilador:

```text
target: nsc
resultado: PASS
static host-runtime contract: passed
build hash: a17fe8753e514644
```

Pruebas unitarias de layout y lowering:

```text
ProjectObjectLayoutModelTests       PASS 47/47
MacroAOopTests                      PASS 63/63
Stage40StructValueAbiTests          PASS 61/61
Item320ObjectStorageOwnershipTests  PASS 11/11
Item323StackClassEscapeAnalysis     PASS 26/26
```

Pruebas externas focales, todas con el `nsc.exe` construido en `A:`:

```text
TL-LANG-491  PASS  rollback de constructor: heap/allocator/raw placement
TL-LANG-492  PASS  destructor + Deallocate exactos
TL-LANG-493  PASS  constructor fallido devuelve bloque al allocator
TL-LANG-494  PASS  objeto owned cruzando DLL conserva procedencia
TL-LANG-495  PASS  failure:null
TL-LANG-496  PASS  failure:throws
TL-LANG-497  PASS  failure:panic
TL-LANG-498  PASS  failure:result y drop de payload owned
```

Full Debug focal:

```text
fixture: tests/bug_triage/b5_ctor_owned_field_array
O0 + Full Debug: compilacion y LLVM verifier PASS
ejecucion: exit 42, que es el resultado esperado por la fixture
instrumentacion: permanece activa
```

`NativeSharp3T06PinningTests` ejecuto correctamente el binario de raw placement al fijar linker y
runtime en `A:`. El ejecutable de prueba termina con 7/8 porque su asercion de texto busca
`reference-local=true pinned=true`, mientras el dumper vigente produce correctamente
`reference-local=true reference-mutable=true pinned=true`. Es una expectativa obsoleta del test,
independiente de esta correccion; no se modifico dentro de este commit causal.

## Estado de cierre

La implementacion y las pruebas locales focales requeridas estan cerradas. Permanece una sola
validacion externa: instalar o copiar este `nsc` en la otra maquina, recompilar un consumidor sin
recompilar sus bibliotecas preexistentes y ejecutar al menos `new Game()`. Ese resultado decide el
cierre de extremo a extremo de la reproduccion original.

