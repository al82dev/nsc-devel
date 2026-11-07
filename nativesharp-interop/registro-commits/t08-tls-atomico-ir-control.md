# Control de IR asociado al global atómico

La prueba ya no acepta instrucciones atómicas arbitrarias del módulo. Busca la
función consumidora, identifica el global TLS o la dirección obtenida mediante
`llvm.threadlocal.address` y exige carga, escritura y suma `atomicrmw` con
`seq_cst` y alineación 4 sobre esa dirección. El global aún no se proyecta en
NSC; se validará el control con ambos IR al habilitarlo.
