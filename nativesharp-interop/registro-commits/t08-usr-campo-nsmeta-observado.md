# Tanda 8: USR de campo observado en un NSMETA real

El fixture generado en `A:/nsc-interop-build/tanda08-meta-c/Api.h` contiene `typedef struct CRecord { int value; } CRecord;`. El archivo `A:/nsc-interop-build/tanda08-meta-c/CPublishedProducer.nsmeta`, SHA-256 `d9bd05dcf8606eed856fd9bddacf5a87bd4f54576770753b4676ff9812716b18`, contiene la secuencia de bytes `c:@S@CRecord@FI@value`, observada con `rg -a -o`.

La batería `A:/nsc-interop-build/tanda08-validacion-proyecto-nuevo.log` registró que el consumidor del tipo C precompilado compiló y se ejecutó de O0 a O3. Por separado, la suite de sesión comparó USR de campos reconstruidos a partir de bytes de catálogo con los de Clang vivo; ver `t08-catalogo-usr-campos-sesion.md`.

La búsqueda de bytes acredita presencia en ese archivo concreto, no que el consumidor haya restaurado o usado ese USR concreto: esa comprobación requiere observar el modelo recuperado del mismo NSMETA. Tampoco acredita funciones, variables o aliases. T08-002 permanece abierto.
