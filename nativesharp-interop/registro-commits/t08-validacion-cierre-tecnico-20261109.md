# T08: validación del cierre técnico (2026-11-09)

Compilador: `A:\nsc-interop-build\tanda1-ON`, construido con los commits hasta `29a1895b5` (fixtures de
`project-oop-lowering` y `stage46`). Desde este cierre, ctest mide el compilador de desarrollo de `A:`
(`NSC_TEST_COMPILER`, ver [corrección](t08-cierre-lowering-stage46.md)). No se tocó nada en `C:`.

## Gates

| Gate | Resultado | Registro |
|---|---|---|
| `validar-tanda-08.cmd` (histórico) | código 0 | `claude\logs\integral-historico.log` |
| `validar-tanda-08-complementaria.cmd` | código 0 | `claude\logs\integral-complementario.log` |

Se ejecutaron uno detrás de otro y sin ctest en paralelo (`claude\scripts\integral-cierre.cmd`).

## ctest (compilador de desarrollo)

Se ejecutaron 29 pruebas: interop, metadata, linker, `stage46`/`47`/`48`/`50_9`, `project-oop-lowering` y los
`item340` a `item349`. Los `item340` a `item342` y `item346` a `item349` se construyeron por primera vez en
`tanda1-ON` para este cierre. Registro: `claude\logs\ctest-final3.log`.

**Pasan 26**, entre ellas:

- `nsc.project-oop-lowering` y `nsc.stage46.native-libraries`, corregidas en este cierre;
- `nsc.item342.aggregate-abi-matrix`, `nsc.item340.com-function-pointers` y `nsc.item343` a `nsc.item346`;
- `nsc.item348` y `nsc.item349`.

**Fallan 3.** Ninguna se atribuye a la tanda 8:

1. **`nsc.stage48_2.library-metadata`, 2 de 444.** Las dos comprobaciones abren
   `tests/fixtures/nsmeta/historical-1.46.nsmeta`, que no existe ([investigación](t08-d24-fixture-historico-146.md)).
   - Por decisión del usuario no se reconstruye el escritor histórico, la prueba queda intacta y no se agrega
     compatibilidad entre versiones.
   - **No es una prueba aprobada:** esas dos comprobaciones no se pueden ejecutar.
2. **`nsc.item341.exception-safe-com-thunk`, 5 de 5.** El fixture convierte a un `extern C delegate` un método
   que declara `throws Boom`; el compilador lo rechaza con «no method in the method group exactly matches delegate
   signature».
   - Sondas en `claude\sonda-t09\com341`: el mismo método sin `throws` se convierte. El filtro de calificadores
     de T08-002 no interviene.
   - El rechazo viene de la coincidencia de tipo de error entre método y delegate (`45258d046`, 2026-10-19). El
     `throws` lo agregó el congelamiento 3.0 (`73b03a394`, 2026-10-25) al exigir `throw` chequeado.
   - Con las reglas vigentes, una excepción chequeada no puede llegar a la frontera C por esta vía, que es justo
     lo que prueba el fixture. Es un conflicto de diseño del lenguaje anterior a esta tanda, y decide el usuario.
3. **`nsc.item347.integer-vector-128`: `nsc.exe` termina por desbordamiento de pila (`0xC00000FD`).** Se
   reproduce con un `Main` que solo llama a `Int32x4.Add` (`claude\sonda-t09\v128\i32`). `Int64x2.Multiply` y un
   proyecto sin vectores compilan.
   - El mismo fallo ocurre con el compilador construido en `97911ad25` (`interop-t07-close`, antes de la tanda 8),
     con `claude\scripts\bisect-nsc.cmd`.
   - Es un error del compilador en la ruta SIMD de `Int32x4`, fuera del alcance de interoperabilidad C. No se
     corrigió.

## Pendiente funcional de la tanda 8

**T08-D25, tipos C que la superficie del productor no referencia.** Pertenece a T08-020 y lo encontró la
revisión del tracking general:

- `pruebas-tanda-08-nombres-metadata/validar.cmd` termina con 1;
- un consumidor sin encabezado no puede usar `sizeof(NAlias)` (`typedef`) ni `sizeof(NMood)` (`enum`), porque
  `CNamesProducer.nsmeta` no los contiene;
- las cuatro sondas por posición de `NRecord` (declaración, `sizeof`, cast y valor) sí pasan.

Requiere decidir el contenido de NSMeta: publicar el catálogo C completo del encabezado o exigir el encabezado al
consumidor ([diseño](t08-nombres-metadata-diseno.md)).

## Estado de la tanda 8

**La tanda 8 no está completamente cerrada.** Quedan:

- T08-D25, pendiente de decisión;
- T08-D21 / T08-GATE-05: agrupación del historial, con la [propuesta](t08-d21-propuesta-agrupacion.md) presentada;
- el fixture NSMeta 1.46, sin ejecutar.

Fuera de la interoperabilidad C quedan `item341` (conflicto de diseño) e `item347` (desbordamiento de pila), los
dos anteriores a la tanda.
