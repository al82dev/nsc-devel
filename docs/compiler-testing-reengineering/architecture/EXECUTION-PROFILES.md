# Perfiles de ejecución de la nueva suite

## 1. Buckets derivados

Los perfiles son frecuencias acumulativas, no suites desconectadas.

| Bucket | Casos asignados | `nsc_component_tests` | Controlador + NSC | Históricos |
|---|---:|---:|---:|---:|
| PR | 7,943 | 5,498 | 2,445 | 0 |
| NIGHTLY | 1,821 | 612 | 1,209 | 0 |
| WEEKLY | 429 | 8 | 1 | 420 |

Por tanto:

- `PR` ejecuta 7,943 casos;
- `NIGHTLY` ejecuta PR + NIGHTLY: 9,764 casos;
- `WEEKLY` ejecuta PR + NIGHTLY + WEEKLY: 10,193 casos.

El conteo describe el plan actual, no una constante codificada en un runner. Se calcula desde el manifiesto válido.

## 2. PR

Objetivo: respuesta de desarrollo sin self-host completo ni batería histórica.

- ejecuta unitarias y contratos de componente host;
- ejecuta programas `.ns` mínimos e integración marcada PR;
- usa el build mode seleccionado;
- usa O0 por defecto;
- enlaza con NSLink sólo cuando el contrato requiere producto ejecutable;
- no ejecuta históricos ni matrices completas de optimización;
- puede filtrarse por componente o cierre de dependencias.

Un filtro local no autoriza afirmar que pasó PR completo. El reporte conserva `scope=focused` y enumera exactamente
los IDs ejecutados.

## 3. NIGHTLY

Objetivo: cerrar integración costosa sin bloquear cada edición.

- incluye todo PR;
- añade self-host, build contracts, empaquetado, determinismo y robustez costosa;
- puede ejecutar Debug y Release;
- ejecuta O0 y únicamente las matrices O1/O2/O3 exigidas por el contrato;
- verifica aislamiento y repetición desde directorios limpios;
- conserva los productores para diagnóstico, pero nunca los reutiliza como fallback en otra corrida.

## 4. WEEKLY

Objetivo: máxima profundidad y compatibilidad histórica.

- incluye PR y NIGHTLY;
- añade los nueve casos nuevos clasificados weekly;
- ejecuta los 420 casos de la batería histórica congelada mediante su propia infraestructura;
- permite matrices completas de build mode y optimización;
- emite un reporte separado para la referencia histórica, de modo que un fallo legado no se disfrace como fallo de
  la suite nueva.

## 5. Vista rápida

`SMOKE` será una vista, no un cuarto bucket. En la Fase 2 se derivará de casos PR explícitamente marcados como
sentinelas: al menos un nominal y un rechazo representativo por componente ejecutable. La selección se guardará en
el manifiesto; no se escogerá “el primer caso” ni se inferirá por nombre.

Hasta que esa selección se audite, no se publica un conteo de smoke.

## 6. Selectores

El controlador y el binario consolidado aceptarán la misma semántica de selección:

- por `case_id`;
- por `contract_id`;
- por componente;
- por capa;
- por bucket o perfil acumulativo;
- por build mode;
- por optimización;
- por tag contractual explícito.

Los filtros se intersectan. Un selector vacío es error de configuración, no éxito con cero pruebas.

## 7. Matriz de configuraciones

| Superficie | Build mode | Optimización | Linker de producto |
|---|---|---|---|
| Host unit/component | Debug o Release | No aplica | No aplica |
| `.ns` compile/dump | Debug o Release de NSC | O0 por defecto; O1-O3 seleccionables | No aplica |
| `.ns` objeto | Debug o Release de NSC | O0-O3 según contrato | No aplica |
| `.ns` ejecutable | Debug o Release de NSC | O0-O3 según contrato | NSLink |
| Self-host | Debug o Release de NSC | O0 por defecto; matriz nocturna | NSLink cuando corresponda |
| Histórica | Contrato congelado | Contrato congelado | Infraestructura congelada |

## 8. Separación de logs

La salida de construir NSC vive en el reporte de build. La salida de configurar la suite vive en el reporte de
configuración. El log de ejecución contiene únicamente selección, comandos de casos, resultados, bloqueos y
evidencia. Esto impide que cientos de líneas de compilación oculten el defecto que se intenta analizar.
