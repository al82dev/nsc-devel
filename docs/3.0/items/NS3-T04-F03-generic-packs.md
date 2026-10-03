# NS3-T04-F03 — Generic packs

## Estado

`TERMINADA`. NativeSharp 3 materializa packs finales de tipos y valores const, expansión léxica,
indexación comprobada, folds acotados y forwarding a través de source y `.nsmeta`.

## Qué se hizo

- `pack Ts` y `pack const T Ns` se distinguen desde el parser y comparten la regla determinista de
  ser el último parámetro.
- La aridad variable normaliza cada elemento según su categoría. Los value packs reutilizan la
  validación, identidad y serialización segura de const generics.
- `Ts...`/`Ns...` expande argumentos directos o compuestos. Una expansión compuesta se materializa
  por índice y valida longitudes zip antes de clonar AST.
- `packtype<Ts,I>` y `packat(Ns,I)` proporcionan indexación de tipo y valor con índice constante,
  no negativo y dentro de rango.
- `packsum`, `packproduct`, `packall` y `packany` generan folds compile-time con identidades
  definidas también para packs vacíos.
- Los errores de materialización se propagan a la instancia causal y eliminan la entrada
  provisional de caché; no queda una especialización parcial reutilizable.
- El límite de 1024 elementos se aplica antes de expandir y los límites de monomorfización siguen
  protegiendo recursión/proliferación.
- La metadata pública conserva el source template y un consumidor separado vuelve a materializar
  un value pack con el mismo comportamiento.

## Archivos modificados

- `src/Parser/Parser.cpp`: gramática de value packs y forwarding con elipsis.
- `include/nativesharp/Frontend/ProjectGenericModel.h` y
  `src/Frontend/ProjectGenericModel.cpp`: normalización, zip, índices, folds, errores y caché.
- `CMakeLists.txt`: target dirigido exclusivo de F03.
- `docs/3.0/TANDA-04-GENERICOS-3.0.md` y
  `docs/3.0/TANDA-OPERATIVA-11-T04-CONST-GENERICS-Y-PACKS.md`: estado consolidado.

## Archivos nuevos y por qué

- `tests/unit/NativeSharp3T04GenericPackTests.cpp`: cubre pack vacío, zip, nesting, forwarding,
  overloads, índices, folds, errores, presupuesto y frontera `.nsmeta` con O0/NSLink local.
- `docs/3.0/language/generic-packs.md`: contrato bilingüe de usuario.
- Este documento: evidencia atómica de F03.

## Mejora obtenida

Las APIs genéricas pueden transportar conjuntos heterogéneos o secuencias const sin estructuras
runtime. La expansión es explícita, determinista y acotada; los errores se detectan antes del
backend y las especializaciones cruzan paquetes precompilados sin perder semántica.

## Validación dirigida

- `nsc_ns3_t04_generic_pack_tests`: 10/10, incluido PE O0 enlazado con NSLink local y consumidor
  `.lib/.nsmeta` separado.
- Regresiones dirigidas de parser, genéricos base, constraints F01 y const generics F02.
- `c61_ns3_const_generics_packs`: fixture O0/NSLink de compatibilidad.

No se ejecutó baseline, CTest general, O1/O2/O3, instalación ni deploy.
