# NS3-T04-F06 — Controlled specialization

## Estado

`TERMINADA`. Los métodos genéricos pueden declarar una implementación estrictamente más restringida
con `specialize`; la coherencia se valida sobre la familia completa y la selección usa el orden
parcial probado después de la resolución ordinaria de overloads.

## Qué se hizo

- Se añadió el modificador contextual Edition 3 `specialize` para métodos genéricos ordinarios.
- Cada plantilla registra una familia alfa-estable por propietario, nombre, aridad, forma
  static/instance y patrón de parámetros; los constraints quedan fuera de esa identidad para poder
  compararlos.
- El modelo de constraints conserva sus átomos semánticos normalizados y valida familias aun cuando
  ningún sitio de llamada las materializa.
- Se exige inclusión estricta demostrable, opt-in en cada implementación más específica y un
  fallback real. Constraints equivalentes, ramas incomparables y diamantes son errores.
- Las especializaciones cerradas conservan el ordinal y la identidad de su plantilla hasta la
  resolución de llamadas.
- El desempate por constraints ocurre sólo después de conversiones, defaults, variádicos y packs.
- Cada cuerpo `specialize` recibe un símbolo físico determinista propio; el símbolo del fallback no
  cambia.
- El marcador y las cláusulas sobreviven clonación genérica y transporte NSMETA. Los miembros
  cerrados de una familia controlada no se publican como overloads ordinarios: el consumidor
  rematerializa desde las plantillas abiertas y toma la misma decisión sin colisiones de firma.

## Archivos modificados

- `src/Parser/Parser.cpp`: sintaxis, Edition 3 y rechazo exhaustivo sobre declaraciones no válidas.
- `include/nativesharp/Frontend/ProjectGenericModel.h` y
  `src/Frontend/ProjectGenericModel.cpp`: identidad de familia, opt-in, consulta por sintaxis y
  conservación del marcador durante monomorfización.
- `include/nativesharp/Frontend/ProjectGenericConstraintModel.h` y
  `src/Frontend/ProjectGenericConstraintModel.cpp`: átomos normalizados y verificación global de
  coherencia.
- `include/nativesharp/Frontend/ProjectMemberModel.h` y
  `src/Frontend/ProjectMemberModel.cpp`: enlace entre miembro cerrado y plantilla, y coexistencia
  controlada de firmas cerradas.
- `src/Frontend/ProjectMethodBodyModel.cpp`: selección de la candidata estrictamente dominante.
- `src/Frontend/ProjectOopHirModel.cpp`: identidad física separada del cuerpo especializado.
- `src/Interop/LibraryMetadata.cpp`: publicación de familias controladas mediante sus plantillas
  abiertas, sin filtrar materializaciones internas del productor.
- `CMakeLists.txt`: target dirigido de F06.
- `A:/NATIVESHARP-3.0-TRACKING.md`: estado del ítem.

## Archivos nuevos y por qué

- `tests/unit/NativeSharp3T04SpecializationTests.cpp`: siete cierres de ejecución, fallback,
  estabilidad de orden, diamantes, equivalencia, opt-in, target inválido y build separado.
- `docs/3.0/language/controlled-specialization.md`: contrato bilingüe y límites conservadores.
- Este documento: evidencia atómica del commit F06.

## Mejora obtenida

Una biblioteca puede ofrecer una ruta más precisa para tipos con capacidades conocidas sin dejar la
selección al orden de declaración ni abrir un sistema de instancias incoherente. Importar la
biblioteca conserva la misma semántica y los cuerpos físicamente distintos no colisionan al enlazar.

## Validación dirigida

- `nsc_ns3_t04_specialization_tests`: 7/7, incluido productor/consumidor NSMETA en O0/NSLink.
- `nsc_parser_tests`: 23/23.
- Regresiones T04 F01–F05: 12/12, 14/14, 16/16, 10/10, 6/6 y 6/6.
- Construcción local de `nsc`: correcta; contrato de runtime host estático: correcto.

No se ejecutó baseline, CTest general, O1/O2/O3, instalación ni deploy.
