# Control actualizado para escritura volatile directa

Tras construir NSC y ejecutar el fixture agrupado, este se detuvo porque un
control anterior exigía literalmente `store volatile i32 61`. El IR generado
contiene `store atomic volatile i32 61 ... release, align 4`, que es el nuevo
contrato comprobado contra Clang. Se actualiza el control sin quitar el caso.
La batería debe repetirse para conocer los demás resultados.
