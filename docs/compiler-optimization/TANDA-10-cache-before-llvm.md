# Tanda 10: consulta de caché antes de LLVM

## Objetivo

Hacer que un objeto presente en el caché evite por completo el lowering, la creación del target, el pipeline y la emisión LLVM.

## Cambios realizados

- Se añadió una identidad canónica y versionada del MIR por unidad de código.
- La identidad contiene todos los campos de los nodos y funciones definidas, las firmas de sus dependencias y las tablas, layouts, inicializadores y contratos globales consumidos por el lowerer.
- El módulo coordinador se identifica con todas las firmas, pero no con cuerpos que no define.
- Cada partición se identifica con cuerpos propios y declaraciones realmente usadas, conservando invalidación precisa.
- El caché puede producir una clave de objeto desde MIR, identidad de target, versión LLVM, identidad del toolchain, nivel de optimización y política de depuración.
- Para compilaciones sin debug ni ThinLTO, la restauración y verificación COFF se hacen antes de construir cualquier objeto LLVM.
- Debug y ThinLTO conservan temporalmente la ruta basada en IR porque requieren información posterior al lowering.

## Archivos modificados

- `CMakeLists.txt`
- `include/nativesharp/LLVM/CompilationArtifactCache.h`
- `src/LLVM/CompilationArtifactCache.cpp`
- `src/LLVM/ProjectOopObjectGenerator.cpp`

## Archivos nuevos

- `include/nativesharp/LLVM/ProjectOopMirCacheIdentity.h`
- `src/LLVM/ProjectOopMirCacheIdentity.cpp`
- Este informe.

El componente de identidad se creó separado para no mezclar el contrato de serialización canónica del MIR con la orquestación del generador de objetos. El `.cpp` se añadió explícitamente a `nsc_llvm`.

## Mejora esperada

Una recompilación sin cambios restaura cada `.obj` mediante hash y copia verificada, sin construir contextos, módulos, targets ni pass managers LLVM. Una modificación de cuerpo invalida únicamente la partición que lo define, siempre que no cambie una tabla o layout compartido.

## Verificación de la tanda

- El primer build O0 detectó que el nuevo `.cpp` faltaba en la lista explícita de CMake; se corrigió y el build posterior de `nsc` terminó correctamente.
- Compilación fría dirigida de Stage 46 con `-O0 -j 4`: 0 hits y 4 misses.
- Repetición idéntica: 4 hits y 0 misses, con biblioteca final correcta de 7 miembros.
- No se ejecutaron rutas O1/O2/O3 ni baseline completo.
