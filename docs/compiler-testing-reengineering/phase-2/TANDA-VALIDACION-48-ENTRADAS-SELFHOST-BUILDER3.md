# Tanda de Validación 48: entradas self-host de Builder 3

Fecha: 10 de octubre de 2026
Estado: CERRADA CON CORRECCIÓN DIRIGIDA

## Defecto demostrado

La ruta self-host sólo comprobaba que `fixture_project` fuera una cadena. El
argumento se convertía mediante `str(...)` y cualquier valor verdadero de
`sandbox_argument` activaba el sandbox. Una ruta absoluta, externa, inexistente
o un tipo incorrecto podían alcanzar la preparación del productor.

## Corrección mínima

`parse_selfhost_inputs` exige:

- objeto con sólo los tres campos declarados;
- proyecto relativo, local, existente y con extensión `.nsproj`;
- argumento textual no vacío;
- sandbox ausente o literal exacto `fresh-case-directory`.

`_run_selfhost` consume esos valores sin conversiones. También se auditaron los
cinco proyectos vivos: sus nombres base son únicos, por lo que no se alteró la
identidad de los workspaces ni el cache.

## Evidencia

- Todas las `192` entradas self-host vivas: aceptadas.
- Cinco productores vivos y cinco nombres base únicos: sin colisiones.
- Rutas absolutas, externas, inexistentes o sin `.nsproj`: rechazadas.
- Argumentos nulos, vacíos, numéricos o contenedores: rechazados.
- Cuatro valores de sandbox ajenos: rechazados.
- Campo desconocido y entrada no objeto: rechazados.
- Pruebas dirigidas: `5/5` PASS.
- Suite Python completa del controlador: `154/154` PASS.

No se preparó ni ejecutó ningún fixture, no se abrió la GUI y no se configuró,
compiló o ejecutó producto. No hubo baseline, despliegue ni escritura en `C:`.
