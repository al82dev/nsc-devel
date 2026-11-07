# T08: control de punteros atómicos globales adicionales

Se prepara un control O0–O3 para punteros atómicos a float, long y unsigned
long C, con lectura, asignación, avance y observación C. El verificador exige
carga, escritura y actualización atómicas `seq_cst` por global. No se ha
ejecutado todavía ni acredita soporte o preservación de TLS.
