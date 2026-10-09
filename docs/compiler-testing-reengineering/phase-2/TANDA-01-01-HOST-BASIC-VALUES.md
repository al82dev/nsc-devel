# Tanda 01.01 — Valores fundamentales de Host.Basic

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Componente | `Host.Basic` |
| Contratos | `CB-BAS-001` a `CB-BAS-005` |
| Casos | `TB-BAS-001` a `TB-BAS-019` |
| Código de producto | Sólo lectura |
| Pruebas históricas | Protegidas; no modificar ni ejecutar |
| Configuraciones | Debug y Release del ejecutable consolidado |

## Implementación

`BasicValuesModule.cpp` materializa los 19 casos canónicos de identidad de archivo, ubicación byte, rango
medio abierto, vocabulario de eventos y códigos de salida. Cada caso usa directamente las cabeceras públicas de
producción y conserva su `case_id`, `contract_id`, capa y perfil.

El manifiesto nuevo contiene entradas declarativas separadas para los 19 casos. El gate del controlador valida
el conjunto descubierto, lo ejecuta caso por caso y exige la secuencia exacta de IDs y 19 resultados `PASS`.
El número se comprueba en este gate de migración porque el alcance de la tanda es explícitamente el intervalo
cerrado `001..019`; el runner general continúa descubriendo dinámicamente cualquier manifiesto recibido.

## Exclusiones

- No se cambió ninguna cabecera ni implementación de `Host.Basic`.
- No se materializaron todavía `CB-BAS-006` a `CB-BAS-016`.
- No se ejecutó NSC, NSLink, baseline, batería histórica, despliegue, remoto ni `C:`.

## Evidencia

- Paridad determinista entre inventario canónico, manifiesto y registro C++: 19/19 IDs.
- Debug: configuración y build correctos; 4/4 gates CTest aprobados.
- Release: configuración y build correctos; 4/4 gates CTest aprobados.
- Controlador: 46/46 pruebas Python aprobadas.
- Runtime host: cero imports dinámicos de VCRUNTIME, MSVCP o UCRT en Debug y Release.
