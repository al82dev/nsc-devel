# T08: punteros atómicos C de anchos adicionales

Se prepara una matriz global y TLS de punteros atómicos no calificados a
signed char, unsigned char, unsigned short y unsigned long long C. Comprueba
IR, avance por elemento y observación C en O0–O3. La variante TLS se genera
solo en el directorio de salida. El fixture aún no se ha ejecutado y no
acredita soporte; no modifica producción, NSLink ni metadata.
