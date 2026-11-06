# USR al resolver enum importado

T08-002/T08-009: el tipo enum importado conserva el USR de Clang y la
resolución de firmas no asocia dos identidades conocidas diferentes aunque
compartan nombre y representación. La proyección comprueba también esa
correspondencia. Si falta el dato se conserva el comportamiento previo; la
coincidencia nunca sustituye las comprobaciones de representación existentes.
Las pruebas se agrupan con la tanda.
