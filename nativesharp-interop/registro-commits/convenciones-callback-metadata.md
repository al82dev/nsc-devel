# Convenciones y variadicidad de callback en metadata

La materialización preparada conserva las convenciones extranjeras admitidas
por el parser y admite variádicos C o Win64 con al menos un parámetro fijo.
Los efectos, referencias y combinaciones inválidas siguen rechazados. La matriz
de metadata pasó O0–O3 con un callback vectorcall y uno C variádico. Se observó
en NSMETA la identidad vectorcall y el indicador de variadicidad C. Faltan
controles para las demás convenciones y combinaciones.
