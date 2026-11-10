# Caché de restricciones de Arc

La comprobación de `Send` y `Sync` no debe reutilizar una compilación válida después de cambiar el tipo de carga o sustituir la biblioteca Core por otra que no publica los impl condicionados. La prueba `tests/integration/shared_arc_conditional_cache.py` recibe explícitamente el compilador y las bibliotecas; trabaja con copias y una caché temporal aislada, sin modificar las bibliotecas de origen.

Con `A:/nsc-fixes-evidencia/arc-cache-final.cmd`, la carga válida compiló en frío, después reutilizó el bundle y devolvió 42. Una carga con puntero crudo se rechazó sin hit; al restaurar la fuente válida se recuperó el bundle. Sustituir la dependencia por un Core sin los impl rechazó la carga sin hit, y restaurar el Core original permitió de nuevo el hit y devolvió 42. Se exige tanto la presencia como la ausencia esperadas del hit; los casos rechazados no se ejecutaron. Los comandos, salidas y hashes están en `A:/nsc-fixes-evidencia/arc-cache-final-results/`.

El compilador y los Core son artefactos aislados en A construidos con cambios locales; la prueba no certifica un árbol limpio ni todos los cambios de caché posibles. Se ejecutó directamente porque necesita dos Core concretos; no instala ni sustituye el SDK del sistema. La seguridad de descarga dinámica y los demás pendientes de las familias compartidas se mantienen abiertos.
