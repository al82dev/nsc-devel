# Proyección TLS de arreglos de punteros

El control C pasó y NSC rechazó los arreglos TLS de punteros y callbacks en
la barrera de proyección. Se permite que arreglos con elemento puntero lleguen
al mapeo C existente, que decide si su tipo, firma y calificadores son
representables. Pendiente la construcción y validación agrupadas; el cambio no
afirma que todas las firmas o formas de puntero se puedan mapear.
