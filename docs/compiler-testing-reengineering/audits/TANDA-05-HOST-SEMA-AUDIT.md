# Segunda auditoría independiente — Host.Sema

## Método

La segunda pasada partió del inventario físico, no del documento narrativo: 30 archivos, 22 tipos definidos,
178 definiciones de source y 14 operaciones inline. Reconstruyó productores y consumidores de símbolos, tipos,
conversiones y diagnósticos; después contrastó cada grupo con contratos y matrices de casos.

## Resultado cuantitativo obtenido

| Control | Valor |
|---|---:|
| archivos | 30 |
| líneas | 5,143 |
| bytes | 175,338 |
| tipos definidos en headers | 22 |
| definiciones físicas de source | 178 |
| funciones/métodos inline con conducta | 14 |
| símbolos | 214 |
| contratos | 30 |
| casos diseñados | 321 |
| casos PR | 313 |
| casos NIGHTLY | 8 |
| archivos/símbolos/contratos omitidos | 0 |
| IDs duplicados o referencias inválidas | 0 |

## Gates reproducidos

- 214/214 símbolos tienen archivo, línea, documento, sección y un contrato propietario.
- 30/30 contratos tienen productores y al menos un caso diseñado.
- 321/321 casos forman `TS-SEMA-001..321` y referencian contratos existentes.
- 22/22 tipos y 178/178 definiciones físicas aparecen en el inventario semántico.
- 30/30 archivos conservan huella, tamaño y líneas de la revisión congelada.
- 30/30 filas tienen trazabilidad archivo → símbolos → contratos → secciones.
- CSV y JSON de producción describen el mismo universo de 974 archivos.
- Markdown y CSV contienen los mismos IDs, contratos, objetivos, oráculos y frecuencias para Host.Sema.
- Sólo cambió `docs/compiler-testing-reengineering/**`; los seis cambios preexistentes permanecieron separados.

## Hallazgos conservados

1. `EffectContext::allows`, ambos dumpers y el análisis de fallthrough/break son recursivos sin guard de
   profundidad o ciclos.
2. Los setters de un tipo calificado delegan y mutan el base compartido.
3. El interning de tipos compuestos recorre linealmente todos los tipos propiedad del contexto.
4. El analyzer emite diagnósticos y continúa en varias rutas; el postestado diagnosticado es parte del contrato.
5. La ruta auditada rechaza capacidades que existen en capas de proyecto posteriores.
6. Textos y aliases históricos de edición/unsafe/ownership siguen presentes y contradicen decisiones posteriores.

## Dictamen

`Host.Sema` es `AUDITADO` y `RECONSTRUIBLE` para la revisión congelada. El dictamen significa suficiencia
documental para reconstruir el comportamiento vigente; no afirma implementación o ejecución de los 321 casos,
ni cobertura instrumental.
