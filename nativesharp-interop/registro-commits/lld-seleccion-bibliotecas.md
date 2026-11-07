# Selección del enlazador en bibliotecas de NSC

El generador de bibliotecas recibe la selección por instancia para el archivo
estático y el enlace dinámico. La identidad de caché del archivo estático
usa el bibliotecario descubierto desde esa misma selección. Con selección
vacía se conserva el comportamiento anterior. NSLink y el entorno global no
se modifican.

La selección todavía no se transmite desde el comando público ni desde la
preparación de metadata. La construcción y la validación se harán en grupo
después de completar la propagación.
