# 239 — Diagnóstico de índice COFF corrupto / Corrupt COFF-index diagnostic

## Resultado / Outcome

**ES.** La prueba P6 acepta las dos barreras válidas que pueden detectar el índice manipulado: el
límite de cantidad de símbolos o la comprobación posterior del tamaño de la tabla de offsets. En
ambos casos el archivo se rechaza antes de reservar memoria o leer fuera del miembro COFF.

**EN.** The P6 test accepts either valid barrier that can detect the manipulated index: the symbol
count limit or the later symbol-offset-table size check. Both paths reject the file before memory
allocation or an out-of-bounds COFF-member read.

## Causa / Cause

El fixture escribe `0xffffffff` en el campo de cantidad de símbolos. Desde que el verificador
incorporó un límite explícito de 16 millones, esa corrupción se detecta antes de calcular el tamaño
imposible de la tabla. La prueba seguía exigiendo exclusivamente el diagnóstico de la segunda
barrera, aunque la primera es más temprana y precisa.

The fixture writes `0xffffffff` into the symbol-count field. Since the verifier gained an explicit
16-million-symbol limit, that corruption is detected before computing the impossible offset-table
size. The test still required only the second barrier's diagnostic even though the first is earlier
and more precise.

## Decisión / Decision

No se debilita el verificador ni se fija el test a un orden interno único. La aceptación exige que
el error siga identificando el archivo como COFF y nombre una de las dos garantías específicas; un
fallo genérico continúa siendo rojo.

The verifier is not weakened, and the test is not coupled to one internal validation order. The
accepted error must still identify COFF and name one of the two specific safeguards; a generic
failure remains red.

## Archivos / Files

- `tests/unit/Stage48_2LibraryMetadataTests.cpp`
- `mkdown/239_diagnostico_indice_coff_corrupto.md`

## Verificación dirigida / Targeted verification

Se ejecuta únicamente Stage 48.2, que debe completar sus 227 comprobaciones. Después se reconstruye
el compilador desde cero sin ejecutar baseline.

Only Stage 48.2 is executed and must complete all 227 checks. The compiler is then rebuilt from
scratch without running a baseline.

## Commit de referencia / Reference commit

**Título ES:** Alinea P6 con la validación temprana del índice COFF

**Title EN:** Align P6 with early COFF-index validation

**Descripción ES:** Reconoce el límite seguro de símbolos y la tabla truncada como rechazos
específicos equivalentes del fixture corrupto.

**Description EN:** Recognize the safe symbol limit and truncated table as equivalent specific
rejections of the corrupt fixture.
