# Implementaciones de traits para structs

La muestra original `68-struct/struct_impl.ns` se rechazaba porque la
agregación solo admitía clases y structs prestados. Tras la decisión del
usuario se admiten structs como destino de `impl`, manteniendo las demás
reglas de agregación y coherencia. Los diagnósticos de destino mencionan ahora
ambas formas.

La muestra original pasó `--check`. La regresión
`nsc.compiler.struct-trait-impl` ejecuta una llamada directa y otra mediante
un parámetro genérico restringido por el trait, en O0 y O2, con resultado 42.
Además comprueba en MIR la llamada directa sin construcción en heap y verifica
que una conversión a trait dinámico se rechaza, sin introducir boxing. La
regresión pasó; evidencia en `A:/nsc-fixes-evidencia/full-suite-run/struct-tests.log`
y `A:/nsc-fixes-evidencia/struct-impl-directed/`.
