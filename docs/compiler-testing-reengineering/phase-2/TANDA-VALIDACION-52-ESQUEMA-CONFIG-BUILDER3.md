# Tanda de Validación 52: esquema de configuración de Builder 3

Fecha: 10 de octubre de 2026
Estado: CERRADA CON CORRECCIÓN DIRIGIDA

## Defecto demostrado

`BuilderConfig.load` asumía que `config.json` era un objeto, descartaba campos
desconocidos y construía la configuración con tipos no validados. Un `jobs`
textual o una ruta numérica fallaban después en la GUI, en comparaciones o al
construir rutas, lejos del productor real del defecto.

## Corrección mínima

`parse_builder_config_payload` valida antes de construir `BuilderConfig`:

- objeto JSON obligatorio;
- sólo los diez campos persistidos conocidos;
- nueve rutas como cadenas;
- `jobs` como entero no booleano.

Los campos pueden omitirse para conservar los defaults existentes. La
autodetección continúa después del cierre del esquema.

## Evidencia

- Payload parcial válido: preservado.
- No objeto y campo desconocido: rechazados.
- Nueve campos de ruta con tipo numérico: rechazados.
- Cuatro formas no enteras de `jobs`: rechazadas.
- Carga desde archivo sintético usa el parser antes de autodetección.
- Pruebas dirigidas: `4/4` PASS.
- Suite Python completa del controlador: `165/165` PASS.

No se leyó ni modificó `config.json` real, no se abrió la GUI y no se descubrió,
configuró, compiló o ejecutó producto. No hubo baseline, despliegue ni escritura
en `C:`.
