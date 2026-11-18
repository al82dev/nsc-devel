# Baseline histórico de NativeSharp

> **Vigencia:** NativeSharp es un lenguaje único y acumulativo; el producto es 1.0. Las referencias históricas a ediciones o versiones de lenguaje y NSMeta que aparezcan a continuación no definen compatibilidad ni deben reintroducirse.

La campaña histórica de la tanda 16 definió una corrida completa posterior a sus cambios. Su
contrato describía los criterios siguientes; esta evidencia no certifica los cambios actuales:

- construcción Release limpia e instalación verificada en `C:\native-sharp`;
- CTest completo con `lld-link` y NSLink, sin pruebas omitidas;
- los fixtures y casos de bug triage en O0 y O2;
- Gate 0 `LANDED`, digest de fuentes estable y hashes de compilador/NSLink estables;
- cero fallos, CTest no interactivo y ningún messagebox.

El contrato generado actualmente se llama `NativeSharpFinalBaseline.contract`; no se ha validado
en esta tarea. Su texto enumera O0 y O2, por lo que no acredita por sí solo los cuatro niveles que
exigen los lineamientos vigentes: O0, O1, O2 y O3 por separado cuando corresponda ejecutar pruebas.

---

# Historical NativeSharp baseline

The historical batch-16 baseline described the criteria listed above. It does not certify current
changes. The current generated contract is named `NativeSharpFinalBaseline.contract`; it has not
been validated in this task. Its text lists O0 and O2 and therefore does not by itself establish
the O0, O1, O2 and O3 coverage required by current guidelines when tests are performed.
