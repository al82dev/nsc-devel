# Expedientes de self-hosting de NativeSharp 2.0

Este directorio contiene la documentación ejecutable del programa de self-hosting definido en
`../SELF_HOSTING_ROADMAP.md`.

- `TANDA_TEMPLATE.md` es la plantilla normativa.
- `BootstrapProfile-2.0.md` es el contrato humano congelado por SH01.
- `tandas/TANDA-XX-<nombre>.md` contiene el expediente humano de cada tanda.
- Los reportes grandes, manifiestos, logs y resultados JSON se conservan en el directorio de
  evidencia generado por la orquestación y se enlazan desde el expediente con su hash.

Una tanda no está cerrada sólo porque el código compile. Está cerrada cuando su expediente, pruebas,
baseline completo y commit de cierre coinciden entre sí.
