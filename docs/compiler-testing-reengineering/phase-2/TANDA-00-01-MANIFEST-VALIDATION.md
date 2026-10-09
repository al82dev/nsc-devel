# Tanda 00.01 — Contrato y validación del manifiesto

## Alcance

Esta tanda materializa el primer productor de la Ola 0: un contrato estricto para cada caso nuevo y un cargador
JSONL que lo valida contra `components.csv` y `contracts.csv` de la Fase 1.

## Contrato

Cada fila declara identidad, contrato, componente, clase, ejecutor, capa, entradas, oráculo, dependencias, perfil,
modos de build, optimizaciones, aislamiento, timeout y tags. El cargador rechaza:

- campos ausentes o desconocidos;
- IDs duplicados;
- contratos o componentes inexistentes;
- componente distinto al propietario del contrato;
- dependencias a casos desconocidos o a sí mismo;
- ciclos de dependencias;
- combinaciones inválidas de ejecutor, aislamiento u optimización;
- manifiestos vacíos cuando se exige al menos un caso.

No existe un conteo esperado de pruebas. El universo se descubre desde todas las filas válidas del manifiesto.

## Evidencia enfocada

`tests/compiler-suite/controller/tests/test_manifest.py` cubre un manifiesto válido y rechazos estructurales,
referenciales y de grafo. Son fixtures sintéticos del framework; todavía no implementan contratos del compilador.

## Frontera

Esta tanda no integra CMake, no ejecuta NSC ni NSLink, no crea casos de producto y no modifica la batería histórica.
