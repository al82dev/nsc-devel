# Suma y resta decimal checked sin representación exacta

El contrato histórico `docs/3.0.4/items/NS304-T15-F07-consteval-lowering.md`
describe que suma y resta alinean a la escala máxima y rechazan coeficientes
mayores de 96 bits. Su helper compartido en `src/Language/NumericSemantics.cpp`
conserva esa comprobación, aunque las búsquedas locales encontraron sus usos
solo en pruebas y la documentación histórica reconoce que no estaba conectado
al lowering. La política half-up documentada en F04 corresponde a división;
no se encontró una regla equivalente de redondeo de suma/resta.
El helper histórico también tiene limitaciones de signos y de ancho intermedio;
no es un oráculo general de corrección para todos los operandos decimales.

Se adopta la decisión de conservar exactitud: en `checked` y modo predeterminado
la suma/resta realiza las reducciones exactas de ceros ya implementadas y
produce `DecimalOverflow` si el coeficiente restante no cabe en 96 bits. No
introduce redondeo half-up. La reducción exacta anterior es una extensión
intencional respecto al helper histórico que rechaza a escala máxima; no se
afirma equivalencia general de ambos. `saturating` y la decisión sobre precisión
de sus resultados a escala positiva no reducible quedan fuera de este cambio.

La matriz basal `A:/nsc-fixes-agent1-temp/decimal-inexact-baseline-20261112.log`
evaluó ±Max ±0.1 en suma/resta, con checked explícito y predeterminado, O0/O2:
las 16 ejecuciones continuaron con salida 32 en lugar del trap 145 adoptado.
Dos controles exactos pasaron. El compilador SHA-256
`252d48d6ce45a281c26bb39138b9b3a89cdd55c03f757299eda66b79eef6f068`, runtime
e import libraries conservaron sus hashes. Los extremos se construyen mediante
`unsafe`; los resultados se consumen en funciones `noinline`. Esto no acredita
la aceptación de literales extremos ni todas las operaciones decimales.

El candidato cambia únicamente la guarda checked/predeterminada para exigir
que el coeficiente que queda tras reducciones exactas quepa en 96 bits. Las
pruebas dirigidas incluyen los ocho casos en O0/O2 y controles exactos con
varios ceros y cancelación cuyo intermedio excede 96 bits. Tras el build
incremental, los 16 casos inexactos produjeron 145 y los dos controles exactos
devolvieron 0. Otras 14 ejecuciones de saturación, signos, reducción, orden y
overflow previo también obtuvieron sus resultados esperados. Comandos y hashes:
`A:/nsc-fixes-agent1-temp/decimal-inexact-integrated-20261112.log`. Compilador
SHA-256 `e6b7ea532d92443d1f0e5900b9de1a426e5c5fbb81b367416e789aa824d157f4`;
runtime e import libraries estables. El build contiene trabajo local y reutiliza
objetos anteriores; no acredita una revisión limpia. El runner CMake exacto
integrado pasó 18 ejecuciones en O0/O2, con 16 traps y dos controles válidos;
comando en `A:/nsc-fixes-agent1-temp/run-decimal-inexact-cmake-20261112.cmd`,
artefactos en `A:/nsc-fixes-agent1-temp/decimal-inexact-cmake-20261112` y log
vacío por diseño al pasar. No se ejecutó CTest.
