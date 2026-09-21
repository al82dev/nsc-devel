# Commit Tanda 46 / SH44

Estado: `LANDED`.

Título: `selfhost(tanda-46): prove environment-independent deterministic builds`

La matriz local completa es el requisito de cierre; una atestación externa queda como evidencia
opcional y no bloquea el commit.
La matriz local aprobó 4 variantes —incluida `ruta-ñ-日本` con sus globs reales—, O0–O3 y ambos
linkers; una copia de la misma atestación fue rechazada correctamente. SH41 pasó tras regenerar el
runtime y las seis pruebas dirigidas afectadas pasaron 6/6. El paquete portable W también reprodujo
exactamente fuentes, contrato, entradas y artefactos. El primer baseline reveló y conservó una
contaminación SH05/SH42 por copias de SH44 bajo el build; los corpus SH05–SH07 están ahora limitados
por contrato a seis raíces canónicas, la regresión pasó 45/45 y SH05/SH42 aprobaron con ambos
linkers. Una segunda corrida aprobó toda la matriz técnica pero Gate 0 la rechazó correctamente
porque la documentación cambió durante la medición; el baseline final debe ejecutarse con el árbol
congelado. La corrida congelada `20260921-062601-17e90a1d` pasó Gate 0 con 479/479 CTest,
148/148 fixtures y 18/18 triage por linker, cero problemas y `S0 == S1`.
