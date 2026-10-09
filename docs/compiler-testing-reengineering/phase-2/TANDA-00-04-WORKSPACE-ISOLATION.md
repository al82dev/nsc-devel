# Tanda 00.04 — Workspace aislado por caso

## Resultado

`WorkspaceManager` crea un árbol nuevo por `run_id/case_id` bajo una raíz explícita en `A:`. Cada caso recibe
directorios separados de `inputs`, `outputs`, `logs` y `artifacts`, más `workspace.json` con las rutas efectivas.

## Invariantes

- la raíz canónica pertenece a `A:`;
- `run_id` y `case_id` son segmentos simples, nunca paths;
- un workspace existente no se limpia ni reutiliza silenciosamente;
- no se buscan directorios alternativos;
- entradas, salidas, logs y artefactos no comparten directorio;
- la metadata se escribe antes de ejecutar el caso.

La política conserva evidencia por diseño. La limpieza será una operación explícita del orquestador y no forma
parte de `prepare`, evitando perder un fallo por una salida anticipada.

## Evidencia enfocada

`tests/compiler-suite/controller/tests/test_workspace.py` valida estructura, metadata, colisión, rechazo de escape
de paths y frontera de unidad. Sus temporales viven bajo el checkout en `A:` y se eliminan al concluir.
