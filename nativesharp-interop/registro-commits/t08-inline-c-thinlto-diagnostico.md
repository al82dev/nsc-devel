# T08-003: diagnóstico de inline C con ThinLTO

Con el build `f2be0335c46cb515`, el control `validar-thinlto.cmd` se detuvo en
la primera compilación directa O0: `generated native objects are not yet
supported in ThinLTO bundles`. El compilador advierte además que en O0 no
optimiza con ThinLTO. O1–O3 y los productores precompilados no llegaron a
ejecutarse; no se declara ningún resultado para ellos.

La ejecución directa de la herramienta informó código 0 pese al fallo; un
llamador CMD con `call` confirmó código 1. El diagnóstico consta en
`A:/nsc-interop-build/tanda08-inline-c-thinlto/direct-O0.log`.

La restricción está en `ProjectOopObjectBundle::appendGeneratedObjects`. Los
objetos C inline y de lifetime implícito se agregan después de generar unidades
ThinLTO. Quitar el rechazo sin más sería insuficiente: hay que considerar la
conservación de símbolos que necesitan los objetos nativos y su transporte al
reemplazar bibliotecas por sidecars ThinLTO. NSLink no se modificó. La corrección
y sus controles quedan pendientes; no se cierra T08-003 ni la tanda.
