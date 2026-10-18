# Progreso JSON de `nsc build` para el IDE

## Problema cerrado

El ejecutable principal reconocia `--progress=json`, pero el parser de `nsc build` lo rechazaba
como opcion desconocida. Por eso el compilador directo podia emitir progreso estructurado y el
comando de proyecto, que es el contrato usado por el IDE, no podia propagarlo al compilador
anidado.

## Contrato

- `build` y `run` aceptan `--progress`, `--progress=text|json` y
  `--progress-format=text|json`.
- La seleccion se reenvia sin reinterpretarla a la invocacion anidada del compilador.
- Con `--output-format=json --progress=json`, cada evento se publica inmediatamente como una
  linea JSON independiente.
- La salida tecnica que no es progreso permanece dentro de `stdout` en el sobre final del comando
  de proyecto.
- Los eventos de progreso no se duplican dentro de ese sobre.
- Los valores invalidos y las selecciones duplicadas se rechazan como errores de linea de
  comandos.

## Validacion real

Se construyo `nsc` desde `build-failed-tests` con Clang/LLVM y el gate de runtime estatico paso. El
ejecutable resultante compilo un proyecto minimo real mediante:

```text
nsc build Sample.nsproj --no-restore --progress=json --output-format=json --no-banner --no-error-log
```

La ejecucion termino correctamente, produjo 58 eventos con schema
`nativesharp-progress-event`, publico las fases del frontend, LLVM y enlazado, y emitio un unico
sobre final `schemaVersion: 1` con el artefacto PE generado. Los eventos precedieron al sobre y la
salida tecnica quedo retenida en el campo `stdout`.

## Limitacion heredada observada

`nsc.project-commands` termina en Windows con `0xc0000409` tanto en la construccion actual como en
una construccion anterior del 8 de octubre de 2026. El mismo aborto ocurre sin el caso nuevo, por
lo que no se atribuye a este contrato. La validacion de esta tanda se hizo con el ejecutable real y
no se presenta ese binario de pruebas heredado como evidencia verde.
