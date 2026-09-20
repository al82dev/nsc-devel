# NativeSharp 2.0 self-hosting — tanda 41: driver y productos

## Identidad

| Campo | Valor |
|---|---|
| Estado | `DIRECTED-PASSED` — baseline acumulado pendiente |
| Gate | `SH39` |
| Fecha | 2026-09-20 |
| Rama | `development-claude` |
| Commit de entrada | `8e99bcd62` |
| Commit de cierre | este commit (`selfhost(tanda-41): define complete product plans`) |

## Implementación

`ProductPlan.Create` transforma `ProjectModel.OutputType` en un contrato completo y determinista:
`.exe`, `.dll`, `.lib` o `.nsmeta`, más objeto, metadata, response file, perfil, linker y decisión de
enlace. `--emit-product-plan` persiste ese contrato sin depender del directorio actual.

La fixture contiene proyectos EXE, Library, StaticLibrary y Facade sobre la misma fuente. Para cada
perfil, el gate crea los cuatro planes, ejecuta un `@response` real, emite el objeto EXE y lo enlaza
con LLD y NSLink; las ocho ejecuciones devuelven 42.

Esta tanda separa deliberadamente planificación de proceso hijo: ningún archivo se marca como
enlazado por el driver sin haber pasado por un linker. La sustitución del builder y la invocación
autónoma se certifican en SH41.

- [x] planes EXE/DLL/LIB/fachada.
- [x] metadata y response files.
- [x] O0–O3 con LLD y NSLink.
- [ ] baseline acumulado final.
