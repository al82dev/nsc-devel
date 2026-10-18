# Tanda NSC305 GATE-F02E — observabilidad y evidencia de Stage 48.2

## Alcance

Esta hoja modifica exclusivamente el armazón de
`nsc_stage48_2_library_metadata_tests`. No cambia acciones del Driver, compilador, linker,
NSMETA, ABI, fixtures ni condiciones de las 398 comprobaciones.

## Causa

Cada invocación in-process ya capturaba stdout y stderr, pero muchas aserciones construían el
mensaje en la misma llamada que ejecutaba el Driver. El orden de evaluación de esos argumentos no
garantiza que la interpolación de `error` ocurra después de la invocación; por eso fallos reales
aparecían como `()`. Otras aserciones ni siquiera incorporaban las corrientes capturadas.

Además, `main` eliminaba incondicionalmente la raíz temporal antes de devolver el resultado. La
prueba perdía proyectos, artefactos parciales y logs precisamente cuando fallaba, aunque el
soporte compartido ya ofrece `resultKeeping` para conservarlos.

## Diseño

Un adaptador `TestContext` local mantiene intactas todas las expresiones booleanas y el número de
checks. `runDriver` publica una observación después de completar la invocación y capturar sus dos
corrientes. El siguiente `expect` consume esa observación exactamente una vez:

- si la condición pasa, descarta la observación y no cambia la salida;
- si falla, forma entonces el mensaje final con exit code, stdout y stderr;
- una aserción posterior que no ejecutó Driver no puede heredar evidencia obsoleta.

El resultado final usa `resultKeeping(root)`: una corrida verde conserva el cleanup histórico y
una corrida roja imprime la ruta y mantiene la evidencia. La raíz se sigue limpiando al inicio,
por lo que una ejecución nueva no mezcla residuos anteriores.

## Invariantes

- cada `tests.expect` existente sigue contando una sola comprobación;
- no cambia ningún predicado, exit esperado ni texto base;
- la observación se crea sólo después de `Driver::run`;
- exit/stdout/stderr se adjuntan sólo al primer check asociado;
- no se convierte una cascada en PASS ni se silencian fallos;
- F02A/F02B y sus fixtures permanecen intactos.

## Evidencia focal

La validación se ejecutó con `TEMP` y `TMP` en `A:\temp`, PATH reconstruido exclusivamente desde
Machine+User y `NSC_LINKER=nslink.exe`; `where nslink.exe` resolvió exactamente
`A:\nslink-devel\bin\release\nslink.exe`.

- el target `nsc_stage48_2_library_metadata_tests` compiló correctamente;
- la corrida CTest normal recibió el runtime hermético de F02D, ejecutó los mismos 398 checks y
  terminó con 394 PASS y los cuatro fallos históricos exclusivos de F02A/F02B;
- el contrafactual directo sin `NSC_WINDOWS_RUNTIME_LIBRARY` terminó normalmente con exit 1, no
  con una excepción de Windows, y reportó 74 fallos de 382 checks;
- esos fallos produjeron 37 bloques completos y pareados de `driver-exit`, `driver-stdout` y
  `driver-stderr`;
- la salida no contiene `Segmentation`, `0xC0000005` ni `3221225477`;
- la raíz `A:\temp\nativesharp_stage48_2_library_metadata_tests` fue reportada y existe después
  del resultado rojo;
- `validate_source_registration.py` terminó con exit 0.

Logs persistentes:

- `A:\temp\gate-f02e-where-nslink.log`;
- `A:\temp\gate-f02e-build.log`;
- `A:\temp\gate-f02e-ctest-normal.log`;
- `A:\temp\gate-f02e-direct-no-runtime.log`;
- `A:\temp\gate-f02e-direct-no-runtime-exit.log`;
- `A:\temp\gate-f02e-source-registration.log`.

La auditoría diferencial confirma que ningún sitio de llamada, predicado, texto base o número de
check fue modificado. El cambio se limita a capturar la observación una vez terminada la
invocación, enriquecer el primer fallo asociado y conservar la evidencia cuando el resultado es
rojo.
