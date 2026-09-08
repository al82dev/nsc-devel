# 143 — Baseline certificado de 1.0-E8

**Fecha:** 2026-09-07  
**Gate:** G00  
**Run:** `20260907-170412-16b5b8cb`  
**Estado:** `LANDED`  
**Certificado:** sí  
**Fallos:** 0

## Resultado

E8 dejó de ser una entrega instalada únicamente en un prefijo de validación. G00 reconstruyó el
Stage 61, lo promovió a `C:\native-sharp`, ejecutó los dos caminos de enlace y certificó que las
fuentes permanecieron inmóviles durante toda la corrida.

El baseline incorpora cuatro contratos de endurecimiento que antes sólo estaban implícitos:

- presupuesto finito de diagnósticos y resumen de los mensajes suprimidos;
- frontera estable para excepciones internas del compilador;
- LLVM 23.x como línea calificada y verificable en ejecución;
- contrato normativo del compilador conectado a implementación y evidencia.

## Evidencia exacta

| Dato | Valor |
|---|---|
| Directorio de corrida | `A:\nsc-devel\_nscbuilder\history\20260907-170412-16b5b8cb` |
| Inicio | `2026-09-07 17:04:12 -06:00` |
| Fin | `2026-09-07 17:15:34 -06:00` |
| Duración | 681.821 s |
| Estado | `LANDED` |
| Certificado | `true` |
| Fallos | 0 |
| Etapas operativas de G00 | 11 |
| Registros de etapa, incluyendo la certificación | 12 |
| Casos | 324 |
| Archivos bajo huella | 1018 |
| Huella S0 | `77a93953271cb2e3ae85fc519153c8163148756ef76ce717ece1071cea9f6a83` |
| Huella S1 | `77a93953271cb2e3ae85fc519153c8163148756ef76ce717ece1071cea9f6a83` |
| SHA-256 construido | `0ed4b3964e12b3ef537b636c9d9e3d331d53ba951f5765bc7572ed4643f07fa0` |
| SHA-256 instalado | `0ed4b3964e12b3ef537b636c9d9e3d331d53ba951f5765bc7572ed4643f07fa0` |

La identidad visible del producto promovido es:

```text
nsc 0.9.0+build.20260907T230413Z.ce3cbc9cd001207a (NativeSharp 0.1, stage 61)
build ce3cbc9cd001207a
LLVM 23.1.0 (supported: 23.x)
SHA-256 0ED4B3964E12B3EF537B636C9D9E3D331D53BA951F5765BC7572ED4643F07FA0
```

## Qué quedó certificado

`S0 == S1` demuestra que los 1018 archivos cubiertos por la huella no cambiaron entre el inicio y
el final. No se mezcló la implementación examinada con una edición posterior.

`built_nsc == installed_nsc` demuestra que el ejecutable probado y el que quedó instalado son el
mismo archivo. La identidad local del prefijo aislado era evidencia previa; la identidad anterior
es la que G00 convirtió en producto vigente.

La lista `problems` quedó vacía y los 324 casos registraron estado satisfactorio. Los dos
enlazadores fueron ejercidos en O0 y O2.

## Recorrido de la corrida

Las once etapas operativas fueron:

1. construir el compilador;
2. instalar el producto;
3. construir CTest para lld-link;
4. construir CTest para NSLink;
5. sincronizar las bibliotecas instaladas;
6. ejecutar CTest con lld-link;
7. ejecutar fixtures con lld-link;
8. ejecutar bug triage con lld-link;
9. ejecutar CTest con NSLink;
10. ejecutar fixtures con NSLink;
11. ejecutar bug triage con NSLink.

El historial contiene un duodécimo registro: `Certificar baseline (Gate 0)`. No es otra matriz de
producto; es la etapa que compara las huellas, comprueba los resultados anteriores y escribe el
estado `LANDED`.

## Relación con la evidencia local

El documento 142 conservó el desarrollo de E8: implementación, corrección del supuesto sobre la
versión de LLVM, limpieza del diff, 95 checks propios y las matrices locales de 299 casos.

G00 añade propiedades que aquellas corridas no podían demostrar por sí solas:

```text
Evidencia local   cambio -> prueba focal -> instalación aislada
G00               fuente inmóvil -> construcción -> instalación viva -> 324 casos
```

## Archivos promovidos por E8

- `include/nativesharp/Driver/CrashBoundary.h`: frontera de fallos internos;
- `include/nativesharp/Diagnostics/DiagnosticEngine.h`: presupuesto y estado de supresión;
- `include/nativesharp/LLVM/LlvmVersion.h`: contrato público LLVM 23.x;
- `src/Diagnostics/DiagnosticEngine.cpp`: aplicación del límite;
- `src/Diagnostics/DiagnosticRenderer.cpp`: resumen observable;
- `src/LLVM/LlvmVersion.cpp`: lectura y validación de la biblioteca cargada;
- `src/LLVM/LlvmModule.cpp` y `src/LLVM/ProjectOopLlvmLowerer.cpp`: gates del backend;
- `src/nsc/main.cpp`: contención en el punto de entrada;
- `tests/unit/Stage61CompilerHardeningTests.cpp`: 95 comprobaciones nuevas;
- `spec/NativeSharp_Compiler_Contract_1_0.md`: ocho cláusulas normativas;
- `spec/NativeSharp_Compiler_1_0_conformance.map`: trazabilidad del compilador;
- `include/nativesharp/Basic/Version.h`: identidad Stage 61.

## Estado de cierre

E8 está implementada, reconstruida, instalada y certificada. El baseline vigente pasa a ser
`20260907-170412-16b5b8cb`; sustituye al baseline E7 como referencia operativa sin borrar su acta
histórica.
