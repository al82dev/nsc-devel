# T08: entrada CRT del control de caché

El consumidor declara `Crt.h` en su proyecto. La primera ejecución del control
ajustado falló al no copiar ese header al directorio aislado. Se copia la misma
entrada que ya utiliza el control sin caché. El caso corregido no se ha
ejecutado todavía; el rechazo del productor con caché permanece vigente.
