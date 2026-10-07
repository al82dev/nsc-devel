# Tanda correctiva 23: prefijo O0 autocontenido

## SCOPE LOCK

### Objetivo exacto autorizado

Restaurar la precondicion de la bateria integral O0: el prefijo temporal de `A:` debe contener
`NativeSharpCore.lib` y `NativeSharpCore.nsmeta` producidos por el mismo `nsc.exe` que se valida.

### Naturaleza

Correccion de infraestructura de validacion para una optimizacion y refactor no funcional.

### Comportamiento que debe permanecer identico

- Semantica, parser, API, ABI, diagnosticos, metadata y artefactos observables de NSC.
- Pruebas historicas, fixtures, proyectos, manifiestos, expectativas y datos.
- Fuentes y contrato de Core; esta tanda solo emite sus artefactos en un prefijo temporal.

### Referencia y rango

- Corrida con la precondicion correcta: `20261006-235544-a5ee2c32`.
- Corrida que omitio el artefacto: `20261007-004905-ccdf8cf7`.
- Revision validada: `883c3c9f3`.
- Rango funcional de la secuencia correctiva: `0b3319586..72817be55`.

### Archivos y salidas permitidos

- Este documento dentro de `A:\nsc-devel`.
- Compilador ya construido en `A:\nsc-devel\build-clang`.
- Prefijo temporal `A:\nsc-devel\_o0-validation-prefix`.
- Historial, trabajo y temporales bajo `A:\nsc-devel\_nscbuilder`.
- NSLink de solo consumo en `A:\nslink-devel\bin\release\nslink.exe`.

### Elementos protegidos

- Todas las pruebas y sus expectativas.
- Fuentes de `lib`, runtime y cualquier consumidor.
- `A:\nsc-devel\_tmp_strip_saru_exceptions.py`.
- `C:` completo.
- NSLink, debugger y cualquier otro repositorio.
- Features aplazados y cambios de semantica.

### Acciones prohibidas

- Baseline, minibaseline y CTest.
- O1, O2 u O3.
- Instalacion o escritura en `C:`.
- Copiar bibliotecas desde `C:` o desde otro repositorio.
- Modificar pruebas, fixtures, proyectos, manifiestos, consumidores, Core o expectativas.
- Diagnosticar programas NativeSharp con LLD.
- Recursion y agentes paralelos.

### Cadena causal comprobada

Los nueve fallos de emision de la corrida `20261007-004905-ccdf8cf7` terminan antes de leer
fuentes con el mismo diagnostico: falta
`A:\nsc-devel\_o0-validation-prefix\lib\NativeSharpCore.lib`.

La corrida `20261006-235544-a5ee2c32` preparo ese artefacto en el mismo prefijo mediante el
proyecto fuente `lib/NativeSharp.Core/NativeSharp.Core.nsproj`. Con esa precondicion, 98 de 99
fixtures pasaron. El unico fallo restante, `cc1_c23_views_and_lifetimes`, termino con 8 porque
las fuentes protegidas de Core no lanzan en las tres operaciones fuera de rango. Esa conducta
procede de `9232bcfc0` y no pertenece al rango de method bodies/construction.

### Procedimiento

1. Emitir `NativeSharpCore.lib` y su metadata con el `nsc.exe` instalado en el prefijo temporal,
   `-O0`, doce trabajos y salida dentro del mismo prefijo.
2. Confirmar existencia y hash de ambos artefactos.
3. Ejecutar nuevamente todos los fixtures y todo el triage mediante NSLink y solo `-O0`.
4. Leer el reporte y cada fallo completo.
5. Clasificar `c23` como externo al alcance solo si permanece como el unico fallo y conserva la
   salida 8 ya demostrada.

### Criterio exacto de terminacion

- El prefijo temporal es autocontenido y no consume artefactos de `C:`.
- Los nueve falsos fallos por biblioteca ausente desaparecen.
- No aparece ninguna regresion atribuible a NSC en el rango correctivo.
- La bateria integral O0 conserva 98 de 99 casos correctos y el unico fallo es `c23`, ya
  demostrado como conducta de Core fuera del alcance.
- La evidencia se incorpora mediante un commit editorial separado y los residuos temporales se
  eliminan despues de conservar el historial.

## Estado antes de ejecutar

PENDIENTE. Este commit fija la precondicion y el alcance antes de emitir la biblioteca o repetir
la bateria.
