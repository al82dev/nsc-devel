# T08 H4: enum C anónimo dentro de un campo publicado

La biblioteca rechazaba un record C que contenía un enum anónimo porque el cierre exigía un nombre de tipo. El cierre admite ahora el enum anónimo cuando conserva USR y un tipo subyacente con tamaño y alineación coincidentes. El productor encuentra su declaración C y publica sus enumeradores mediante las dependencias de NSMETA 1.47; detecta discrepancias de tipo o valor entre observaciones de una misma identidad.

La matriz de identidad compiló y ejecutó `CAnon` y `CAnonHeader` y terminó con código 0 (`A:/nsc-interop-build/tanda08-identidad-enum-campo-final.log`). Antes de añadir la comprobación de discrepancias también pasaron los validadores dirigidos de records C y dependencias de plantilla. El caso probado es un campo escalar; no acredita todos los usos de enums anónimos ni cierra la tanda 8.
