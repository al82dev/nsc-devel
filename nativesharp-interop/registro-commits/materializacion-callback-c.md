# Materialización inicial de callback C desde metadata

La generación de fuente para contratos precompilados rechazaba `fn:` y por
ello omitía el método observado. Se prepara la reconstrucción de `fn* C` sin
efectos ni variadicidad a partir de su identidad estructural, conservando los
tipos de parámetros y resultado. Otras convenciones y formas permanecen
rechazadas. Falta construir y comprobar el consumidor real de metadata.
