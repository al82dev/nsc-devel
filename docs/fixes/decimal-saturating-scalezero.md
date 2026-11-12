# Saturación decimal de sumas y restas enteras fuera de rango

El operador `saturating` en el producto decimal ya devuelve el límite según el
signo. La documentación histórica `docs/3.0.4/items/NS304-T15-F05-checked-saturating.md`
deja suma y resta como trabajo residual; aplicar el mismo extremo a estas
operaciones es una decisión de diseño adoptada para esta corrección.

El sondeo `A:/nsc-fixes-agent1-temp/decimal-saturating-scope-20261112.log`
compiló y ejecutó casos en O0 y O2 con compiler SHA-256
`b7274638afa8a061bc5be6673f73983c04212e5784a5009219087cbdc5fadea4`.
`Max + 1` y `-Max - 1` con escala cero retornaron cero en ambos modos; al usar
`1.0`, retornaron otro patrón que no era ni el extremo esperado ni cero. Los
controles representables `Max + 0.0` y `-Max - 0.0` conservaron los extremos.
Se emplearon valores de representación fabricados por `unsafe` y helpers
`noinline`; esta evidencia no establece qué literales extremos son admitidos.

El candidato limita el cambio a suma/resta `saturating` cuyo resultado no cabe
tras reducciones exactas y llega a escala cero: selecciona el máximo coeficiente
de 96 bits, conserva el signo matemático y devuelve escala cero. Un resultado
representable mantiene la ruta exacta; un resultado que conserva escala positiva
al detenerse conserva la ruta anterior. No define redondeo ni garantiza
corrección de resultados no reducibles exactamente. No afecta multiplicación,
división, `checked`, predeterminado ni otras categorías de tipo.

La prueba dirigida `tests/integration/decimal_saturating_scalezero` comprueba
ambos signos y escalas de entrada 0/1, más valores representables. Tras el
build incremental, el lanzador independiente
`A:/nsc-fixes-agent1-temp/run-validate-saturating-scalezero.cmd` ejecutó 14
casos en O0/O2: saturación, `checked` y predeterminado con trap 145, controles
válidos, signos y orden decimal. Todos obtuvieron el resultado esperado.
El log `A:/nsc-fixes-agent1-temp/validate-saturating-scalezero.log` conserva
comandos y hashes estables de entradas; el compilador tenía SHA-256
`252d48d6ce45a281c26bb39138b9b3a89cdd55c03f757299eda66b79eef6f068`.
El compilador reutiliza objetos previos y tiene trabajo local; no se afirma
validación de una revisión limpia. Redondeo y saturación en escala positiva
no reducible permanecen pendientes.

El runner CMake exacto integrado pasó en O0 y O2 con salida 0; sus artefactos
están en `A:/nsc-fixes-agent1-temp/decimal-saturating-cmake-20261112` y el
log permanece vacío por diseño cuando pasa. No se ejecutó CTest.
