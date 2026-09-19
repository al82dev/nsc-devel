# Pruebas de self-hosting

- `bootstrap/`: cadena `nsc0 → nsc1 → nsc2 → nsc3` y aislamiento de procesos.
- `differential/`: comparaciones por tokens, sintaxis, semántica, IR, ABI y ejecución.
- `fixtures/`: corpus exclusivo del compilador autoalojado.

Las pruebas normales de NSC siguen viviendo en `tests/`. Este árbol sólo contiene pruebas cuya
razón de existir es el self-hosting.
