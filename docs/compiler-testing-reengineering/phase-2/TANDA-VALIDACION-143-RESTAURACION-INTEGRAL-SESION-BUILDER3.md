# Validacion 143 - restauracion integral de sesion en Builder 3

## Entrada reproducible

- Ejecucion: `20261010-090333-82a4a13f`.
- Seleccion persistida: `Total`, perfil `Todos`, build `Debug`, optimizacion
  `O0`.
- Casos canónicos seleccionados: `9,587`.
- Variantes ejecutadas con resultado terminal: `5,695`.
- Casos no ejecutables visibles: `3,892`.
- Conteos terminales: `5,140 PASS`, `247 FAIL`,
  `88 INFRASTRUCTURE_ERROR`, `187 BLOCKED` y `33 KNOWN_FAILURE`.

## Causa confirmada

Validacion 142 proyecto correctamente todas las filas de `results.json`, pero
`MainWindow._load_history_path` deduplicaba casos desde esas variantes. Las
filas pendientes no tienen resultado terminal y, por definicion, no aparecen
en `results.json`; por eso la tabla reconstruida tenia `5,695` filas en vez de
las `9,587` de la seleccion original.

Ademas, los resultados reales se conservan en orden de finalizacion. Exigir el
mismo orden del catalogo rechazaba evidencia completa aun cuando las `5,695`
identidades fueran un conjunto exacto sin duplicados, faltantes ni sobrantes.

## Cierre implementado

1. El cargador valida el sobre `selection` persistido y reconstruye la misma
   seleccion contra el catalogo canónico.
2. Se cotejan `case_count`, las variantes seleccionadas y el multiconjunto de
   identidades terminales antes de tocar la GUI.
3. La tabla se repuebla con los `9,587` casos: variantes terminales y filas
   `PENDIENTE` o `FILTRADO`.
4. Los contadores vivos incluyen ahora las filas no ejecutables visibles.
5. Alcance, valor, perfil, build y optimizacion vuelven a los valores de la
   corrida cargada.
6. Estados, duraciones, detalles, evidencia, log completo, reporte condensado
   y fallos exactos se restauran desde disco.
7. La carga no inicia worker, no configura CMake, no compila, no ejecuta y no
   modifica la evidencia historica.

## Correccion de proyeccion visual

La comprobacion del usuario demostro que reconstruir primero filas `LISTO` y
actualizarlas una a una no satisfacia la carga visible. Esa ruta fue retirada.
La tabla materializa ahora directamente cada registro de `results.json` con su
estado, duracion, detalle y evidencia; despues agrega exclusivamente los casos
pendientes o filtrados que no poseen resultado terminal. Los filtros operan
sobre esos estados definitivos desde la primera presentacion de la tabla.
Activar una fila del selector de historial carga la ejecucion inmediatamente;
el boton `Cargar ejecucion` conserva la misma accion. La cabecera cambia a
`Ejecucion CARGADA` para que una mera seleccion no pueda confundirse con una
carga aplicada.

## Validacion

- Sintaxis de `nscbuilder3.py` y su prueba dirigida: correcta.
- Pruebas dirigidas del historial: `10/10`.
- Regresion de resultados en orden de finalizacion: cubierta.
- Lectura de la corrida real: `9,587` casos, `5,695` variantes,
  `3,892` no ejecutables y `5,695` resultados.
- Conteos reales leidos: `5,140 PASS`, `247 FAIL`,
  `88 INFRASTRUCTURE_ERROR`, `187 BLOCKED`, `33 KNOWN_FAILURE`.
- Smoke no interactivo de GUI: correcto.
- Compilacion o ejecucion de NSC: no realizada.
- Pruebas del compilador: no ejecutadas.
- Baseline: no ejecutado.
- Unidad `C:`: no tocada.
