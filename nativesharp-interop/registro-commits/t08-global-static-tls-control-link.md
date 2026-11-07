# T08-004: enlace del control C TLS estatico

El enlace sin bibliotecas fallo por `_tls_index`. El control se enlaza con
las bibliotecas estaticas y de importacion usadas por el fixture TLS existente;
la referencia y NSC aun no se han ejecutado despues de este ajuste.
