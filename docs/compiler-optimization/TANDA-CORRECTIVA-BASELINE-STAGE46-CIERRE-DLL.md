# Tanda correctiva — cierre binario de DLL en Stage 46

**Fecha:** 1 de octubre de 2026  
**Rama:** `development`  
**Plataforma:** Windows x64  
**Validación:** dirigida O0; baseline no ejecutado

## Problema observado

Una DLL `CompositeNative.dll` enlazaba privadamente `NativeMath.dll`. Su NSMETA publicaba también
`DllProducer/NativeMath` dentro de `binaryDependencies`. Al consumir únicamente la DLL superior,
el compilador intentaba resolver `NativeMath.lib + NativeMath.nsmeta` como si `CompositeNative`
fuera todavía un archivo estático sin enlazar.

La resolución fallaba antes del linker y generaba ocho fallos derivados en O0, O2 y el escenario
diamante. El empaquetado dinámico ya tiene un contrato distinto y suficiente: recorrer exactamente
la tabla de imports PE y copiar los runtimes encontrados junto a la DLL propietaria.

El noveno fallo de Stage 46 no era un rechazo ausente. P6 sí rechazaba dos artefactos explícitos
con identidad `StaticProducer`, pero la prueba exigía el texto histórico `duplicate module name`
en lugar del diagnóstico actual, que además identifica ambos artefactos incompatibles.

## Cambios

- `src/Interop/LibraryMetadata.cpp`: `binaryDependencies` se publica solamente cuando el producto
  raíz es `StaticLibrary`. Una DLL ya consumió sus entradas estáticas y sus dependencias dinámicas
  permanecen transitivas mediante imports PE.
- `tests/unit/Stage46NativeLibrariesTests.cpp`: congela que el NSMETA de una DLL compuesta no
  republique entradas privadas como cierre estático y alinea P6 con el diagnóstico causal vigente.

## Mejora

Un consumidor enlaza sólo la import library de la DLL superior. El compilador deja de exigir
sidecars de compilación para una dependencia dinámica privada, mientras conserva intacta la
expansión certificada y relocatable de archivos estáticos NSMETA 1.25.

## Validación realizada

- `nsc` y `nsc_stage46_native_libraries_tests` construidos en el árbol Debug/O0;
- cadena dirigida O0 `NativeMath.dll -> CompositeNative.dll -> CompositeConsumer.exe` emitida
  correctamente desde A:;
- el consumidor enlazó una sola import library, empaquetó `CompositeNative.dll` y
  `NativeMath.dll`, y terminó con código 42;
- negativa P6 O0 terminó con código 1 y publicó el diagnóstico causal actualizado;
- temporal dirigido eliminado; sin O1/O2/O3 y sin baseline.
