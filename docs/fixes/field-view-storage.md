# Protección de campos que respaldan vistas

Una vista obtenida de un propietario guardado en un campo podía mantenerse viva
mientras se sustituía o movía ese propietario, incluso a través de un alias del
objeto. Se conserva un conjunto conservador de raíces y nombres de campos de
la vista; las sustituciones y los movimientos de campos o elementos consultan
ese conjunto. La misma consulta incluye el receptor `this`, campos implícitos,
alias locales, parámetros y arreglos en las formas probadas. Las ramas de un
condicional y las fuentes de referencias reasignadas se unen conservadoramente.

Después de compilar incrementalmente el compilador aislado, una selección de
ocho casos de `nsc.compiler.readonly-borrowed-array` pasó con los controles
local y atómico: se rechazaron sustituciones y movimientos inseguros; los
controles de otro campo y de movimiento después de terminar el préstamo se
ejecutaron en O0/O2 con salida 42. Evidencia:
`A:/nsc-fixes-evidencia/view-field-regression.log`. También pasaron las tres
pruebas existentes de vida y escape de préstamos y la de movimientos
parciales (`move-array-lifetime-existing.log` y
`move-array-partial-existing.log`). El build usado contenía otros cambios aún
no registrados; no se construyó de forma aislada esta revisión.

El seguimiento aproxima arreglos por su raíz y une fuentes sin distinguir
todos los caminos o momentos; puede rechazar usos seguros. No demuestra la
procedencia de todas las expresiones ni la seguridad de vistas en agregados,
receptores calculados o llamadas que podrían modificar campos. Esas rutas y las
decisiones de diseño consultadas continúan abiertas. Esta corrección parcial
no completa ni publica las familias de punteros compartidos.
