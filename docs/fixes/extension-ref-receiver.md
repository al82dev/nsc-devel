# Receptor `ref` en llamadas de extensión

El ejemplo original `tests/cc1/c69_ns3_extension_methods` fallaba en O0 y O2:
el verificador LLVM recibía el valor de `Point` para un parámetro que requería
una dirección. La selección del hijo HIR calculaba correctamente el contexto
de dirección, pero la configuración posterior lo reemplazaba por contexto de
valor para llamadas de namespace.

La configuración conserva ahora el contexto ya elegido por el marco de llamada,
como ya lo hacía para las llamadas ordinarias. Se añadió una prueba que modifica
un struct local mediante `extension ref` y lo lee mediante una extensión por
valor; la prueba de biblioteca importada existente también cubre el receptor
por referencia entre módulos.

Con el compilador y runtime construidos en `A:/nsc-fixes-evidencia/build`, el
proyecto original compiló con `--no-artifact-cache` y ejecutó con código 42 en
O0 y O2. `nsc.ns3.t05.extension-methods` pasó, incluyendo la nueva regresión y
la prueba de biblioteca importada. Comandos y salidas están en
`A:/nsc-fixes-evidencia/full-suite-run/extension-fixed/`. La batería completa
sigue teniendo otros fallos y no queda certificada por esta prueba dirigida.
