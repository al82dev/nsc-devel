# Tanda de Validacion 76: identidad terminal tipada

Fecha: 10 de octubre de 2026
Estado: LISTA; VALIDACION FUNCIONAL EXTERNA PENDIENTE

## Causa confirmada

`ToolIdentity` y `RunMetadata` eran dataclasses sin validacion runtime. Podian
serializar nombres o rutas vacios, hashes incompletos, dominios desconocidos,
selecciones duplicadas u objetos de herramienta ajenos al contrato.

## Correccion

- herramienta exige nombre y ruta no vacios;
- SHA-256 exige 64 digitos hexadecimales minusculos;
- build mode, optimizacion, perfil y alcance usan dominios canonicos;
- la seleccion es una tupla no vacia de IDs unicos;
- las herramientas son una tupla de `ToolIdentity` con nombres unicos;
- la metadata falla al construirse, antes de abrir eventos.

## Evidencia ejecutada

- pruebas dirigidas de reportes: `5/5` PASS;
- suite Python completa del controlador: `216/216` PASS;
- no se configuro, compilo ni ejecuto NSC;
- no se ejecuto baseline ni se escribio en `C:`;
- los seis archivos protegidos conservaron sus hashes.

## Frontera de la afirmacion

La validacion demuestra la forma de la identidad terminal y sus dominios. No
acredita identidad de herramientas reales ni una corrida funcional. La meta
permanece activa hasta cero errores inesperados en la matriz acordada.
