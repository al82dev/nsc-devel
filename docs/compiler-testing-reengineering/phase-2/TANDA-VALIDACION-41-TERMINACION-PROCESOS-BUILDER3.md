# Tanda de Validación 41: terminación de procesos de Builder 3

Fecha: 10 de octubre de 2026
Estado: CERRADA CON CORRECCIÓN DIRIGIDA

## Defecto demostrado

En Windows, `_terminate_tree` invocaba `taskkill /T /F` y regresaba sin revisar
su código ni esperar la salida del proceso hijo. Un `taskkill` fallido podía
dejar vivo el proceso propiedad del controlador mientras Builder 3 continuaba.

## Corrección mínima

- un proceso ya terminado no se toca;
- un `taskkill` correcto siempre va seguido de `wait`;
- un `taskkill` fallido aplica `kill` al hijo conocido y espera;
- si la primera espera vence, se fuerza el hijo y se espera una segunda vez.

No se enumeran ni terminan procesos ajenos.

## Evidencia

- Proceso ya terminado: cero acciones.
- `taskkill` correcto: espera confirmada.
- `taskkill` fallido: fallback `kill` confirmado.
- Espera vencida: `kill` y segunda espera confirmados.
- Pruebas dirigidas con dobles: `4/4` PASS.
- Suite Python completa del controlador: `123/123` PASS.

No se crearon o terminaron procesos reales, no se abrió la GUI y no se
configuró, compiló o ejecutó producto. No hubo baseline, despliegue ni escritura
en `C:`.
