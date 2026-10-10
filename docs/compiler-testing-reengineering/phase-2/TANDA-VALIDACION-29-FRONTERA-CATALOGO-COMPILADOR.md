# Tanda de Validación 29: frontera exclusiva del catálogo del compilador

Fecha: 10 de octubre de 2026
Estado: CERRADA CON EVIDENCIA ESTÁTICA

## Riesgo cubierto

Los inventarios de Fase 1 todavía documentan bibliotecas, SDK, tooling y
terceros. Builder 3 no puede interpretar esa conservación documental como
autorización para mezclar esos productos con la suite nueva de NSC.

## Prueba permanente

`test_nscbuilder3_catalog_scope.py` deriva los conjuntos vivos y exige:

1. igualdad exacta entre componentes de Builder 3 y
   `implementation-waves.csv`;
2. que cada componente de esas olas pertenezca a `Host.*`, `SelfHost.*`,
   `BuildSystem` o `Packaging`;
3. que todo componente documental fuera de las olas permanezca fuera del
   manifiesto ejecutable;
4. que no exista intersección entre esa superficie externa y los casos que
   Builder 3 puede ejecutar.

## Evidencia

- Componentes del compilador en olas: `44`.
- Componentes documentales externos a las olas: `13`.
- Componentes externos en el manifiesto ejecutable: `0`.
- Pruebas dirigidas: `2/2` PASS.
- Suite Python completa del controlador: `82/82` PASS.
- Cambios funcionales en `nscbuilder3.py`: `0`.
- Inventarios externos eliminados o modificados: `0`.

No se configuró, compiló ni ejecutó producto. No hubo baseline, despliegue ni
escritura en `C:`.
