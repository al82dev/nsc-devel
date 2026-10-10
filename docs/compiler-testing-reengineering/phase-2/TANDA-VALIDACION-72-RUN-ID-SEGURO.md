# Tanda de Validacion 72: run_id seguro

Fecha: 10 de octubre de 2026
Estado: LISTA; VALIDACION FUNCIONAL EXTERNA PENDIENTE

## Causa confirmada

`run_suite.py` construia `events.jsonl` con `work_root / run_id` antes de que la
validacion de segmentos de `WorkspaceManager.prepare` pudiera ejecutarse. Un
identificador anidado o con traversal podia crear evidencia fuera de la raiz
de corrida autorizada y fallar despues.

## Correccion

- `WorkspaceManager.run_root` valida y resuelve el segmento sin escribir;
- `prepare` reutiliza esa autoridad;
- el controlador valida `run_id` antes de abrir `events.jsonl`;
- eventos y reportes consumen la misma ruta resuelta;
- identificadores vacios, ocultos, anidados o con traversal no crean estado.

## Evidencia ejecutada

- pruebas dirigidas de workspace: `5/5` PASS;
- suite Python completa del controlador: `213/213` PASS;
- no se configuro, compilo ni ejecuto NSC;
- no se ejecuto baseline ni se escribio en `C:`;
- los seis archivos protegidos conservaron sus hashes.

## Frontera de la afirmacion

La validacion demuestra el confinamiento previo del identificador de corrida.
No acredita ejecucion funcional de casos. La meta permanece activa hasta cero
errores inesperados en la matriz acordada.
