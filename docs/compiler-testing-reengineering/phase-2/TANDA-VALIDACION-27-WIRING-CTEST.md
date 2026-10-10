# Tanda de Validación 27: wiring CTest de controladores

Fecha: 10 de octubre de 2026
Estado: CERRADA CON EVIDENCIA ESTÁTICA

## Riesgo cubierto

La existencia de un script y de una referencia CMake no garantiza que la
invocación sea válida: el parser puede agregar una opción obligatoria, CTest
puede omitirla o CMake puede conservar una opción retirada.

## Prueba permanente

`test_ctest_wiring.py`:

1. extrae iterativamente cada llamada `add_test(...)`, respetando paréntesis y
   cadenas CMake;
2. identifica el único `run_*.py` de cada bloque;
3. analiza el script mediante AST y deriva todas sus opciones argparse y las
   marcadas `required=True`;
4. rechaza opciones CTest desconocidas y opciones obligatorias ausentes;
5. exige que cada script existente aparezca exactamente una vez.

No existe una tabla manual de argumentos o controladores.

## Evidencia

- Contratos controlador/CTest validados: `116`.
- Omisiones de opciones obligatorias: `0`.
- Opciones desconocidas: `0`.
- Scripts sin conexión o con conexión repetida: `0`.
- Prueba dirigida: `1/1` PASS.
- Suite Python completa: `76/76` PASS.

No se modificaron comandos CTest ni parsers porque la auditoría no demostró
desalineación. No se modificaron casos, oráculos, producto ni batería histórica.
No se configuró, compiló ni ejecutó producto; no hubo baseline ni despliegue.
