# T08: control concurrente de global C atómico no TLS

Se prepara un control con un hilo C y el hilo NativeSharp que incrementan el
mismo global atómico 10.000 veces cada uno. El resultado se consulta desde
ambos lenguajes en O0–O3. El fixture aún no se ha ejecutado; no demuestra
concurrencia ni modifica producción o NSLink.
