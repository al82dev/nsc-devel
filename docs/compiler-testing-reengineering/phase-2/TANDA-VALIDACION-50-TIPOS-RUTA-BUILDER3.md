# Tanda de Validación 50: tipos de ruta de Builder 3

Fecha: 10 de octubre de 2026
Estado: CERRADA CON CORRECCIÓN DIRIGIDA

## Defectos demostrados

`BuilderConfig.validated` comprobaba existencia para NSC y NSLink, pero un
directorio existente también pasaba. Las rutas opcionales de archivador, NSC de
comparación, build-id y `LLVM_DIR` no comprobaban su tipo. Las raíces escribibles
podían apuntar a un archivo existente.

## Corrección mínima

Se separaron validadores de archivo y directorio:

- NSC y NSLink deben ser archivos existentes en `A:`;
- archivador, NSC de comparación y build-id, si se proporcionan, deben ser
  archivos existentes;
- `LLVM_DIR`, si se proporciona, debe ser un directorio existente;
- las cuatro raíces escribibles permanecen en `A:` y no pueden ser archivos.

## Evidencia

- Tres raíces existentes como archivo: rechazadas.
- NSC y NSLink configurados como directorio: rechazados.
- Tres herramientas opcionales configuradas como directorio: rechazadas.
- `LLVM_DIR` configurado como archivo: rechazado.
- Pruebas dirigidas de rutas: `8/8` PASS.
- Suite Python completa del controlador: `159/159` PASS.

No se descubrió ni ejecutó ninguna herramienta real, no se abrió la GUI y no se
configuró, compiló o ejecutó producto. No hubo baseline, despliegue ni escritura
en `C:`.
