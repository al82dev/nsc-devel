# NS304-T16-F06 — Self-host parity

- **Estado**: COMPLETO — port selfhost del sufijo decimal `m`/`M` + oráculo de paridad de token.
- **Rama**: development

## Implementado (2026-10-15)

- `8e41794d` — cierre del hueco auditado (un caso representativo del port + el corpus idéntico):
  - **Port selfhost**: `selfhost/nsc/Lexer/Lexer.ns::LexNumber` acepta el sufijo `m`/`M` como
    `FloatingLiteral` — el mismo contrato de lexema que el host (`1.5m` conserva el sufijo en el
    lexema), de modo que el token dump del selfhost iguala al host.
  - **Oráculo de paridad**: `TL-LANG-511` fija el oráculo host (`--dump-tokens` →
    `FloatingLiteral "1.5m"`) que `verify_frontend.py` compara contra el selfhost.

## Verificado O0

- `TL-LANG-511` (CL-LANG-020, Host.Language, `nsc_component_tests`): PASS — el host mantiene el
  sufijo `m` en el lexema (el oráculo de paridad).

## Contexto

- El hueco era el port selfhost de cada feature + el corpus idéntico host/selfhost. El corpus completo
  de paridad lo ejecuta `selfhost/bootstrap/verify_frontend.py` (token dump + AST dump vs nsc0), que
  compara cada perfil selfhost contra el host; aquí se cerró el port del `decimal` (representativo) y
  su oráculo.
