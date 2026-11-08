# T08-006: integración de campos C volátiles

El control de Claude detectó que NSC omitía acquire/release en accesos a
campos C escalares declarados volatile, alineados y con semántica Microsoft.
Se integró su parche en las rutas de carga, escritura y asignación compuesta.
Los campos desalineados y la volatilidad heredada mantienen el tratamiento
descrito por el control. El formato de metadata y NSLink no cambiaron.

## Evidencia en esta rama

- Compilación incremental de `nsc`: código 0.
- `pruebas-tanda-08-volatile-campos-c/validar.cmd`: código 0; ejecución
  O0–O3 en directo, Shared y Metadata. Los 34 accesos controlados coinciden
  con Clang en los modos comprobados; también se compararon O1–O3 directos.
- Regresiones agrupadas de `_Bool`, layout-clang, layout, record-c y tres
  controles de bitfields volatile: código 0, `VALIDACION_CALL_OK`.
- El control observa sondas adicionales que pueden rechazar compilación; no
  son evidencia de que enum o record volátiles ya se importen. Permanecen
  pendientes, junto con miembros anónimos y el gate completo de la tanda 8.

Salida principal: `A:/nsc-interop-build/tanda08-volatile-integracion-20261107`.
