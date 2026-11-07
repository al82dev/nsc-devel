# T08: global C atómico exportado por DLL

`pruebas-tanda-08-atomic-global-dll/validar.cmd` terminó con código 0. La DLL
C exportó `_Atomic(int)` y el control C independiente pasó. NativeSharp leyó y
actualizó el global en O0–O3; la función C observó el resultado. El IR de NSC
contenía carga y actualización atómicas `seq_cst` para el global importado.

El fixture se agregó al gate agrupado, aún no repetido. No acredita TLS
importado de otra DLL, otros tipos ni concurrencia. T08-004 y la tanda siguen
abiertas; NSLink no se modificó.
