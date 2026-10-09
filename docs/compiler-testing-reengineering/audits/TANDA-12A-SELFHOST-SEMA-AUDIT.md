# Segunda auditoría independiente — SelfHost.Sema

## Resultado cuantitativo

| Gate | Resultado |
|---|---:|
| Archivos inventariados / leídos / auditados | 7 / 7 / 7 |
| Líneas / bytes | 2049 / 118956 |
| Declaraciones físicas | 163 |
| Contratos | 20 |
| Casos diseñados | 153 |
| PR / NIGHTLY | 130 / 23 |
| Declaraciones sin contrato | 0 |

## Comprobaciones

- Cada archivo conserva ID, path, hash, revisión y trazabilidad documental.
- Cada declaración física pertenece a exactamente un contrato primario; contratos de proyecto/documentación trazan su archivo no ejecutable.
- Cada contrato posee positivos, negativos, bordes y variantes; determinismo, mutación y escala sólo se añaden cuando aplican.
- Se revisaron errores, estados, ownership, dependencias y oracles sin ejecutar producto.
- No se tocó compilador, pruebas, C:, instalación, baseline, coverage ni remotos.

## Dictamen

`SelfHost.Sema` queda `AUDITADO` y `RECONSTRUIBLE` para la revisión congelada. Este dictamen es documental; no afirma que la suite futura exista o haya pasado.
