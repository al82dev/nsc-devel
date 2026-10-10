# Tanda de Validación 59: persistencia de configuración de Builder 3

Fecha: 10 de octubre de 2026
Estado: CERRADA CON CORRECCIÓN DIRIGIDA

## Defecto demostrado

El parser aceptaba cualquier entero para `jobs`; al cargar `0` o `257`, la GUI
podía recortarlo mediante su control visual sin reportar que el archivo estaba
malformado. `save` escribía directamente sobre `config.json`, por lo que una
interrupción podía reemplazar la última configuración válida con contenido
parcial.

## Corrección mínima

El rango `1..256` se valida durante el parseo. `BuilderConfig.save` valida el
payload y las raíces antes de escribir y usa `write_json_atomic`: archivo
temporal exclusivo en el mismo directorio, flush, `fsync`, reemplazo atómico y
limpieza garantizada. La carga conserva el payload exacto antes de la
autodetección.

## Evidencia

- Límites `1` y `256`: aceptados.
- Cuatro enteros fuera de rango: rechazados antes de autodetección.
- Reemplazo simulado fallido: configuración anterior intacta y cero temporales.
- Guardado exitoso: round trip exacto y cero temporales.
- Pruebas dirigidas de persistencia y esquema: `8/8` PASS.
- Suite Python completa del controlador: `193/193` PASS.

Todas las pruebas usaron configuraciones sintéticas bajo `_nscbuilder3`; no se
leyó ni modificó el `config.json` real. No se abrió la GUI y no se configuró,
compiló o ejecutó producto. No hubo baseline, despliegue ni escritura en `C:`.
