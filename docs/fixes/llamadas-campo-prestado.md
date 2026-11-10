# Llamadas mientras existe una vista de campo

Una vista de `holder.value.Borrow()` podía seguir viva mientras
`holder.Replace()` reemplazaba ese mismo propietario. El caso
`overwrite_field_via_method` del SDK público fue aceptado por el análisis antes
del cambio; `A:/nsc-fixes-evidencia/shared-method-borrow-before.log` conserva el
comando y la salida. No se ejecutó la lectura posterior potencialmente inválida.

Se reutiliza el seguimiento de raíces y alias de vistas para rechazar llamadas
de instancia potencialmente mutadoras sobre un receptor nominal cuando un campo
de ese receptor tiene una vista viva. Los métodos de clase carecen de un
contrato de solo lectura comprobado y se tratan conservadoramente: la regla
puede rechazar un método que no modifica el campo. Se conservan los métodos
readonly de estructura y los receptores independientes o cuyo préstamo terminó.

Las pruebas dirigidas desde fuentes y desde el SDK binario rechazaron el ejemplo
y una variante con alias para `Rc` y `Arc`. Los controles de receptor
independiente y de fin de ámbito ejecutaron 42 en O0 y O2. Los controles
existentes de reemplazo, movimiento y vistas anidadas también pasaron.
Evidencia en `shared-method-borrow-after.log`, `shared-method-borrow-binary.log`
y `shared-method-borrow-controls.log`, en `A:/nsc-fixes-evidencia`.
`shared_field_method_borrow.py` se invoca directamente porque el archivo CMake
compartido contiene cambios concurrentes sin registrar.

El análisis cubre el receptor nominal y los alias que rastrea el modelo actual;
no demuestra seguridad para receptores calculados, llamadas indirectas ni
argumentos que puedan mutar el objeto. El SDK binario usado es el ya generado
en el build aislado; no se reconstruyó para esta prueba. El build incremental
del objetivo `nsc` actualizó 206 pasos a causa del encabezado compartido y,
como dependencia del propio compilador, compiló también
`src/Interop/CHeaderGenerator.cpp` en el build aislado. No se modificó ese
archivo ni se utilizaron carpetas de build de interoperabilidad. La evidencia
no acredita el árbol limpio: coexistieron cambios concurrentes sin registrar.

## Receptor de clase anidado

`shared-nested-method-before.log` muestra que `holder.child.Replace()` se
aceptaba mientras una vista de `holder.child.value` seguía viva. Se amplió la
comprobación de raíces existente a receptores de clase expresados mediante
miembros. Es conservadora: puede rechazar métodos de ramas independientes del
mismo objeto cuando el análisis solo identifica una raíz común.

Un primer ensayo incluyó receptores de estructura y rechazó erróneamente la
lectura `holder.child.value.Get()`; los diagnósticos se conservan en
`shared-nested-method-readonly.log` y `shared-nested-method-binary.log`. El
retorno `ref readonly` no demuestra que cualquier método sea readonly sobre su
receptor, por lo que no se usa como excepción general. El cambio final deja
pendientes los métodos de estructuras en receptores anidados.

`shared-nested-method-final.log` y `shared-nested-method-binary-final.log`
registran el rechazo del reemplazo inseguro y la ejecución de controles con otro
objeto y con `Get()` en O0/O2 para Rc y Arc, desde fuentes y con el SDK binario.
También pasaron los controles del receptor directo y de fin de ámbito. Las
llamadas mediante índices, receptores calculados, métodos de estructuras
anidadas y llamadas indirectas no quedan acreditadas por esta corrección.
