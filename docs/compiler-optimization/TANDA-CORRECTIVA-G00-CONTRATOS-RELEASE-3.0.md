# Tanda correctiva G00: contratos de release NativeSharp 3.0

## Problema

Aunque el compilador ya era 3.0, la construccion continuaba generando e instalando siete
artefactos llamados `NativeSharp2*`. Su contenido describia la migracion 1.0.1 a 2.0 y afirmaba
que 2.0 estaba oficialmente promovido. Reutilizar esos artefactos en el staging 3.0 mezclaba dos
generaciones y, peor aun, podia presentar 3.0 como estable antes del baseline final.

## Cambios

- El ledger describe ahora la ruptura controlada 2.0 a 3.0: compilador, NSMETA, Runtime ABI y
  runtime symbol set.
- La guia migra desde 2.0, conserva Language 2 como edicion predeterminada y exige opt-in
  explicito para Language 3.
- El baseline de migracion publica los contratos fisicos actuales y queda
  `pending-final-baseline`.
- El freeze apunta a `3.0.0-rc.1`, NSMETA 1.25, Runtime ABI 1.1, symbol set 3 y MIR 1.21.
- La matriz se marca `pending-final-baseline`; no se presenta como certificada antes de tiempo.
- El contrato final distingue el staging que debe verificarse en
  `A:/native-sharp-3.0-baseline` del destino de promocion `C:/native-sharp`.
- El acta de promocion publica canal `development` y decision `pending-final-baseline`. El cambio
  a `stable`/`officially-promoted` queda reservado para el gate final ejecutado por Alberto.
- CMake, el manifiesto de instalacion y `InstallationVerifier` usan exclusivamente nombres
  `NativeSharp3*` para los siete artefactos.
- Las pruebas dirigidas y sus fixtures de instalacion fueron migrados a los mismos nombres y
  contratos.

No se modifico ni se instalo nada en C:.

## Archivos nuevos

Solo este documento. Los artefactos `NativeSharp3*` son productos generados del build, no archivos
fuente versionados.

## Mejora

El paquete de prueba deja de mezclar contratos 2.0 con el compilador 3.0. La promocion pasa a ser
fail-closed: mientras no exista baseline final aprobado, todos los artefactos declaran de manera
coherente que la promocion sigue pendiente.

## Validacion especifica

- Target `nsc` construido en `A:\nsc-devel\build-clang`.
- La construccion genero los siete artefactos `NativeSharp3*`.
- Verificador de instalacion: 51/51.
- Baseline de migracion: 15/15.
- Ledger: 15/15.
- Guia de migracion: 9/9.
- Freeze RC: 10/10.
- Matriz final: 16/16.
- Contrato de baseline final: 24/24.
- Acta de promocion pendiente: 11/11.
- La evidencia dirigida se mantuvo en A: y fue eliminada al terminar.
- Se eliminaron 14 residuos `NativeSharp2*` unicamente de los dos directorios `generated`
  utilizados; no se toco evidencia `known-good` ni historiales de autoalojado.
- No se ejecuto CTest general ni el baseline.
