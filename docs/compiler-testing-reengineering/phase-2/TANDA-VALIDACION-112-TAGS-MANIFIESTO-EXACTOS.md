# Tanda de Validacion 112: tags exactos en manifiestos crudos

Fecha: 10 de octubre de 2026
Estado: LISTA; VALIDACION FUNCIONAL EXTERNA PENDIENTE

## Proposito y problema

BuildSystem, Packaging y los gates self-host leen `cases.jsonl` como objetos
JSON crudos porque necesitan campos que no proyecta el modelo tipado. Esas
fronteras verificaban identidad y linaje, pero consultaban `tags` directamente
con el operador de pertenencia.

En Python, el texto `"known-product-defect"` satisface esa consulta igual que
el arreglo `["known-product-defect"]`. Por tanto una forma invalida respecto al
contrato canonico podia acreditar la clasificacion de un defecto conocido.

## SCOPE LOCK

- repositorio: `A:\nsc-devel`;
- componente: infraestructura nueva de pruebas de NSC;
- naturaleza: correccion de validacion de manifiesto;
- archivos funcionales: helper comun, linajes BuildSystem y Packaging e
  inventario self-host;
- pruebas: helper exacto y reproduccion con cuatro componentes vivos;
- comportamiento preservado: casos, IDs, disposiciones, oraculos, seleccion y
  ejecucion;
- archivos protegidos: bateria historica, producto y los seis archivos sucios
  registrados en `CURSOR.json`;
- prohibido: modificar manifiestos, compilar o ejecutar producto, baseline,
  despliegue, `C:` y remotos;
- terminacion local: cero aceptacion de tags escalares y regresion Python
  completa aprobada.

## Cadena causal

1. Entrada: una fila viva de defecto conocido con `tags` convertido de arreglo
   a texto.
2. Operacion: pertenencia de `known-product-defect` sobre el valor crudo.
3. Invariante: `tags` debe ser un arreglo de cadenas no vacias y unicas.
4. Simbolo responsable: frontera raw anterior a los clasificadores.
5. Consumidores: BuildSystem, Packaging, SelfHost.Diagnostics y
   SelfHost.Lexer.
6. Correccion: validar el arreglo exacto antes de cualquier clasificacion.

## Implementacion

`required_string_array` rechaza ausencia, escalares, elementos no string,
cadenas vacias y duplicados. BuildSystem y Packaging lo ejecutan al validar su
linaje. `validate_manifest_cases` lo ejecuta una vez para todos los gates
self-host que comparten esa frontera.

La validacion es iterativa, `O(n)` en cantidad de tags y `O(n)` en memoria para
detectar duplicados. No usa recursión ni cambia rutas de ejecucion del producto.

## Evidencia

- cuatro componentes auditados: BuildSystem, Packaging,
  SelfHost.Diagnostics y SelfHost.Lexer;
- casos vivos recorridos: `129`;
- tags vivos de defecto conocido: `9`;
- prueba contrafactual: cada componente recibe el texto
  `"known-product-defect"` y lo rechaza con `field tags must be an array`;
- pruebas dirigidas: `14/14` PASS;
- suite Python completa del controlador: `246/246` PASS;
- manifiestos vivos válidos: BuildSystem `36`, Packaging `12`,
  SelfHost.Diagnostics `28`, SelfHost.Lexer `53`, todos PASS;
- `git diff --check`: PASS;
- no se configuro, compilo ni ejecuto NSC, NSLink o fixtures;
- no se ejecuto baseline ni se escribio en `C:`.

## Frontera de la afirmacion

La tanda demuestra el cierre estatico de esta forma invalida de `tags` y la
ausencia de regresiones en la suite Python. No acredita la matriz funcional de
los casos ejecutables. Esa validacion externa permanece pendiente y la tarea
continua activa.
