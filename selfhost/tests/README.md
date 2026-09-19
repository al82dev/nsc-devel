# Pruebas de self-hosting

`profile/positive` contiene código perteneciente al Bootstrap Profile 2.0 que `nsc0` debe compilar
y ejecutar. `profile/negative` contiene construcciones válidas o reconocibles de NativeSharp 2.0
que el perfil deliberadamente rechaza antes del bootstrap; `cases.json` fija sus códigos `NSBP`.

Las corridas generadas nunca se escriben aquí: viven bajo `selfhost/out` o
`selfhost/evidence/runs`.

- `bootstrap/`: cadena `nsc0 → nsc1 → nsc2 → nsc3` y aislamiento de procesos.
- `differential/`: comparaciones por tokens, sintaxis, semántica, IR, ABI y ejecución.
- `fixtures/`: corpus exclusivo del compilador autoalojado.

Las pruebas normales de NSC siguen viviendo en `tests/`. Este árbol sólo contiene pruebas cuya
razón de existir es el self-hosting.
