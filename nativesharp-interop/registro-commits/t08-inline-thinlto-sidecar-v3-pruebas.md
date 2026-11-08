# T08-003: controles del codec de sidecar v3

La suite de ThinLTO comprueba la conservación de bytes binarios en bitcode y dos
objetos nativos, la selección de v3, el rechazo de una longitud inválida incluso
con huella recalculada y de objetos vacíos. También comprueba que un sidecar sin
objetos se escriba y lea como v2. Los resultados se registrarán al validar el
conjunto; todavía no se afirma que el productor o el consumidor usen v3.
