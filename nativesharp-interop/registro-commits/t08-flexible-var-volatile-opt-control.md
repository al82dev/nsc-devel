# T08-010/015: comparación del alias local con optimizaciones

El diagnóstico de IR genera y compara ambos modos en O0, O1, O2 y O3. El IR
optimizado se pide explícitamente a NSC; el control conserva el código 1 cuando
alguna comparación difiere y 2 ante un fallo de preparación o interpretación.
Su ejecución está pendiente; no se conecta al gate mientras el defecto siga.
