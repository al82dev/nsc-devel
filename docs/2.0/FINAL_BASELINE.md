# Baseline completo final de NativeSharp 2.0

La promoción de NativeSharp 2.0 exige una sola corrida completa posterior a todos los cambios de
la tanda 16. El contrato instalado `NativeSharp2FinalBaseline.contract` (`NSFINALBASELINE 1`) fija
los criterios que debe satisfacer su reporte:

- construcción Release limpia e instalación verificada en `C:\native-sharp`;
- CTest completo con `lld-link` y NSLink, sin pruebas omitidas;
- todos los fixtures y casos de bug triage en O0 y O2;
- Gate 0 `LANDED`, digest de fuentes estable y hashes de compilador/NSLink estables;
- cero fallos, CTest no interactivo y ningún messagebox.

El contrato describe la puerta; el expediente de cierre de la tanda registra el identificador y la
ruta del reporte que la atravesó.

---

# NativeSharp 2.0 final complete baseline

Promotion requires exactly one complete run after every batch-16 change. The installed
`NativeSharp2FinalBaseline.contract` (`NSFINALBASELINE 1`) requires a clean Release build, verified
installation, complete CTest matrices with lld-link and NSLink, every fixture and bug-triage case
in O0 and O2, a LANDED Gate 0, stable source and tool hashes, zero failures, non-interactive CTest,
and no message boxes. The batch closure records the report that satisfies this gate.
