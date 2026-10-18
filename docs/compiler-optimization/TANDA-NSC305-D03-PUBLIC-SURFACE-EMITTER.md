# D03 - Public-source emitter

## SCOPE LOCK

- Producto unico: `nsc` en la rama `development`.
- Cambio interno y no funcional: extraer la proyeccion de superficie publica fuera de
  `Driver.cpp`.
- Se preservan byte por byte el texto generado, su orden, los diagnosticos y los codigos de
  salida.
- No se agregan ni cambian sintaxis, API, ABI, NSMETA, fingerprints, formatos ni artefactos.
- No se reactiva el versionado de NSMETA como feature independiente.
- No se toca Parser, HIR, MIR, NSLink, NSDBG, deploy, install ni baseline.
- Los recorridos nuevos son iterativos y no comparten estado mutable entre hilos.

## Hipotesis causal unica

`Driver.cpp` mezclaba despacho de acciones con 1,358 lineas de seleccion, ordenamiento y
rendering de metadata publica. Esa concentracion impedia aislar invariantes de orden y hacia que
cualquier cambio del driver arriesgara silenciosamente el texto reparseado por consumidores.

## Implementacion

- `PublicSurfaceEmitter` es el unico propietario de `build` y `verify`.
- `Driver.cpp` conserva dos adaptadores delgados para no alterar sus consumidores existentes.
- `PublicSurfaceEmitterSupport.h` limita el puente a los formateadores de metadata ya existentes;
  no duplica contratos ni introduce otra codificacion de tipos.
- El cierre transitivo de tipos contractuales usa una pila LIFO y conserva el orden DFS previo.
- Los tipos anidados se materializan en postorden mediante frames explicitos y luego se insertan
  en la misma posicion que ocupaban durante el recorrido recursivo.
- Los carriers anonimos usan frames con indice de hijo; apertura, campos y cierre conservan el
  orden anterior sin consumir la pila nativa.
- Las familias delegate, enum, interface/trait y variant tienen renderers separados.

## Contrafactual focal

`nsc_public_surface_emitter_tests` construye metadata minima con una clase publica y un struct
publico anidado. Compara la proyeccion completa contra un golden byte por byte y vuelve a parsear
esa misma proyeccion mediante `verify`.

La prueba queda registrada en CMake como `nsc.public-surface-emitter`.

## Evidencia focal

- Build `nsc` y `nsc_public_surface_emitter_tests`: PASS.
- CTest `^nsc\.public-surface-emitter$`: PASS 1/1.
- El golden compara byte por byte la salida completa de una clase con un struct publico anidado.
- `PublicSurfaceEmitter::verify` vuelve a lexear y parsear exactamente el mismo buffer: PASS.
- Auditoria de cuerpos lambda: ningun helper excede 60 lineas fisicas.
- `git diff --check`: PASS.
- Registro de fuentes: `PublicSurfaceEmitter.cpp` aparece exactamente una vez en `nsc_driver`;
  la suite consume ese target y no mantiene una lista Driver duplicada.
- No se ejecuto baseline, bateria completa, deploy ni install.

## Criterios de rechazo

- Cualquier byte distinto en el golden sin una correccion de defecto autorizada.
- Cambio en el orden de tipos, miembros, accessors, estados, extensiones o `impl` de traits.
- Diagnostico nuevo, eliminado o reescrito.
- Recursion en el cierre contractual, tipos anidados o carriers anonimos.
- Cambio de NSMETA, ABI, sintaxis o comportamiento de consumidor.
