# Tanda universal 39: inventario dinámico de SH40

## Problema confirmado

SH40 congelaba cinco conteos globales y cuatro conteos de proyectos en el código del verificador.
Restaurar una unidad pública válida de Core o Collections hacía fallar el bootstrap aunque todas las
unidades se analizaran sin errores y O0/O1/O2/O3 produjeran el mismo grafo.

Los valores congelados ya estaban desactualizados: el reporte real contiene 102 unidades; Core
analiza 43 y Collections 25.

## Cambio

El verificador ahora:

- descubre los conteos del reporte producido por el compilador;
- exige que todas las métricas requeridas existan y sean positivas;
- exige `source-verified`;
- comprueba que cada proyecto analice todas sus unidades y tenga cero fallos;
- compara de forma exacta reportes, grafos, módulos y checks entre O0/O1/O2/O3;
- deriva el JSON final de la evidencia observada.

Se mantienen las comprobaciones de propietarios obligatorios y la prohibición de entradas binarias
ocultas. No se acepta un conjunto parcial; simplemente se elimina la duplicación manual del número
esperado.

## Archivos

- Reescrito: `selfhost/bootstrap/verify_bootstrap_dependencies.py`.
- Nuevo: este documento.

## Validación requerida

1. Ejecutar el script sobre la evidencia SH40 existente.
2. Ejecutar `nsc.selfhost.bootstrap-dependencies` de forma dirigida.
3. Ejecutar las etapas derivadas sólo después de que SH40 produzca el candidato.
4. Incluir todo en la corrida integral final.

