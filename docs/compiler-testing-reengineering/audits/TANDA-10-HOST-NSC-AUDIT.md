# Segunda auditoría independiente — Host.nsc

## Método

Se contrastó el manifiesto con la lectura completa, roots AST, rama preprocesada alternativa, inventarios,
documento reconstructivo y plan contractual.

## Resultado cuantitativo

| Gate | Resultado |
|---|---:|
| Archivos inventariados / leídos / auditados | 1 / 1 / 1 |
| Líneas / bytes | 551 / 24,169 |
| Símbolos físicos | 18 |
| Tipos / funciones-like | 1 / 17 |
| Contratos | 20 |
| Casos diseñados | 160 |
| PR / NIGHTLY | 140 / 20 |
| Contratos sin ocho casos | 0 |
| Símbolos sin contrato | 0 |
| Referencias rotas o IDs duplicados | 0 |

## Gates

- El file ID conserva path, hash, revisión y estado.
- Ambos entry points condicionales están inventariados.
- Cada símbolo posee contrato primario; contratos integradores trazan `runCompiler` explícitamente.
- Logging, tracing, progreso, bundle, Unicode y crash boundary tienen oráculos observables.
- No se crearon pruebas ni se ejecutó producto.
- Los seis archivos preexistentes permanecen protegidos.

## Hallazgos conservados

- La captura completa de stdout/stderr puede crecer con diagnósticos voluminosos; medir pertenece a una fase
  posterior, no autoriza cambiar streaming en esta auditoría.
- La cota de un millón de nombres evita un loop infinito del log automático, pero requiere fixture NIGHTLY
  aislado y no una reproducción sobre el cwd del desarrollador.
- La publicación elimina el destino antes de rename; su semántica Windows exacta debe caracterizarse antes de
  proponer garantías transaccionales más fuertes.

## Dictamen

`Host.nsc` queda `AUDITADO` y `RECONSTRUIBLE`. La Tanda 10 completa sus 80 archivos, pero la Fase 1 continúa con
contratos transversales; no se afirma ejecución ni cobertura.
