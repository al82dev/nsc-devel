# Materialización inicial de callback C desde metadata

La generación de fuente para contratos precompilados rechazaba `fn:` y por
ello omitía el método observado. Se prepara la reconstrucción de `fn* C` sin
efectos ni variadicidad a partir de su identidad estructural, conservando los
tipos de parámetros y resultado. Otras convenciones y formas permanecen
rechazadas. NSC se construyó incrementalmente y la matriz de metadata compiló,
enlazó y ejecutó O0–O3, incluido el productor precompilado y consumidor real.
La muestra no demuestra todas las firmas ni el contrato completo de metadata.
