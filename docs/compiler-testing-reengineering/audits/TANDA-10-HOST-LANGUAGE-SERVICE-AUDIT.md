# Segunda auditoría independiente — Host.LanguageService

## Método

Se contrastó el manifiesto canónico contra los cuatro archivos, la extracción AST, los inventarios resultantes,
el documento reconstructivo y el plan de pruebas. La segunda pasada recalculó conteos y verificó trazabilidad.

## Resultado cuantitativo

| Gate | Resultado |
|---|---:|
| Archivos inventariados / leídos / auditados | 4 / 4 / 4 |
| Líneas / bytes | 1,614 / 63,887 |
| Símbolos físicos | 126 |
| Tipos / funciones-like | 28 / 98 |
| Contratos | 42 |
| Casos diseñados | 336 |
| PR / NIGHTLY | 294 / 42 |
| Contratos sin ocho casos | 0 |
| Símbolos sin contrato | 0 |
| Referencias rotas | 0 |
| Duplicados de ID | 0 |

## Gates

- Cada file ID conserva hash, revisión, path y estado.
- Cada símbolo apunta a archivo, contrato, documento y sección.
- Cada contrato posee por lo menos un símbolo fuente y ocho casos.
- Se cubrieron caminos nominales, inválidos, bordes, determinismo y hostiles.
- No se creó código de prueba ni se ejecutó producto.
- La batería histórica y los seis archivos protegidos permanecieron fuera del diferencial.

## Hallazgos conservados

- La lista de keywords de rename es una copia local y puede divergir del lexer cuando cambie la superficie del
  lenguaje; la futura implementación de tests debe detectar esa divergencia sin corregirla en Fase 1.
- La búsqueda de identifiers durante indexado recorre tokens por símbolo y merece medición posterior, no una
  optimización especulativa.
- El fallback semántico conserva deliberadamente diagnósticos actuales y modelo anterior; ambos deben probarse
  como dimensiones separadas.
- El formatter es una herramienta conservadora por llaves, no un pretty-printer del AST.

## Dictamen

`Host.LanguageService` queda `AUDITADO` y `RECONSTRUIBLE`. No se afirma ejecución, cobertura ni ausencia de
defectos de producto.
