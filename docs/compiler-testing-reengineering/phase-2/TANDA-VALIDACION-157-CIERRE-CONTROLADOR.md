# Tanda de validación 157 — cierre del controlador previo a la batería integral

## Evidencia de entrada

La primera validación posterior al commit `189aac633` ejecutó los `314` casos
del controlador y terminó con seis errores, agrupados en dos causas exactas:

- cinco errores al cargar `selfhost_inventory.py`, porque el cargador dinámico
  ejecutaba el módulo antes de registrarlo en `sys.modules`;
- un error en el oráculo de timeout, porque un doble sintético de prueba no
  exponía el atributo opcional `manifest`.

## Corrección

`ExecutionEngine._load_gate()` aplica ahora el protocolo completo de carga de
un módulo desde archivo: publica la identidad antes de ejecutar el cuerpo y la
retira si la inicialización falla. Esto permite que `dataclasses` resuelva el
módulo durante la evaluación de sus tipos sin modificar el gate ni el producto.

La clasificación de timeout consulta el manifiesto como atributo opcional. Los
casos canónicos siguen exigiendo el marcador validado por esquema; los dobles
sintéticos y los casos sin marcador conservan la clasificación de
infraestructura.

## Límites

- Código de producto NSC modificado: no.
- Oráculos funcionales modificados: no.
- Casos retirados: no.
- Unidad `C:` modificada: no.
- Revalidación: pendiente después del commit.
