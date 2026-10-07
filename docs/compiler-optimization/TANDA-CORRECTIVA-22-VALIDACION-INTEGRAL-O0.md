# Tanda correctiva 22: validacion integral O0 posterior

## SCOPE LOCK

### Objetivo exacto autorizado

Demostrar, con una bateria integral O0 no-baseline, que las correcciones de los productores comunes de
method bodies y construction conservan el comportamiento historico de NSC sin adaptar consumidores.

### Naturaleza

Validacion correctiva de una optimizacion y refactor no funcional.

### Comportamiento inmutable

- Semantica, parser, API, ABI, diagnosticos, metadata y artefactos observables.
- Pruebas historicas, fixtures, proyectos, manifiestos, expectativas y datos.
- Comportamiento de compilacion y ejecucion previamente valido.

### Referencia y rango

- Referencia anterior a la secuencia correctiva: `0b3319586`.
- Revision que se validara: `72817be55`.
- Rango: `0b3319586..72817be55`.

### Archivos y salidas permitidos

- Fuentes y documentacion ya modificadas dentro de `A:\nsc-devel`.
- Build del compilador: `A:\nsc-devel\build-clang`.
- Prefijo temporal: `A:\nsc-devel\_o0-validation-prefix`.
- Historial, trabajo y temporales: `A:\nsc-devel\_nscbuilder`.
- NSLink de solo consumo: `A:\nslink-devel\bin\release\nslink.exe`.

### Elementos protegidos

- Todas las pruebas historicas y sus expectativas.
- `A:\nsc-devel\_tmp_strip_saru_exceptions.py`.
- `C:` completo.
- Bibliotecas, runtime, NSLink, debugger y cualquier otro repositorio.
- Features aplazados y cambios de semantica.

### Acciones prohibidas

- Baseline o minibaseline.
- CTest, porque su inventario contiene verificadores multioptimizacion no compatibles con el alcance O0.
- O1, O2 u O3.
- Instalacion o escritura en `C:`.
- Modificar pruebas, fixtures, manifiestos, consumidores o resultados esperados.
- Diagnostico o repeticion con LLD de programas NativeSharp.
- Recursion y agentes paralelos.

### Procedimiento

1. Construir `nsc` desde `development` en `A:\nsc-devel\build-clang`.
2. Instalar el resultado solamente en `A:\nsc-devel\_o0-validation-prefix`.
3. Confirmar que el ejecutable instalado coincide con el construido.
4. Limpiar exclusivamente `A:\nsc-devel\_nscbuilder\work\tests\NSLink`, salida regenerable de fixtures.
5. Ejecutar con NSCBuilder todos los fixtures y todo el triage mediante NSLink y `-O0`.
6. Leer `metadata.json`, `report.json`, `report.md` y los logs completos.
7. Si aparece una regresion de NSC, localizar primero el productor comun dentro del rango correctivo.

### Criterio de terminacion

- El build y la instalacion temporal terminan correctamente.
- El binario usado por las pruebas procede de la revision actual y nunca de `C:`.
- Todos los casos O0 atribuibles a NSC pasan sin modificar evidencia historica.
- Cualquier fallo fuera del alcance queda demostrado con evidencia reproducible; no se parchea su consumidor.
- El resultado se incorpora a este documento mediante un commit editorial separado.

## Estado anterior a la ejecucion

PENDIENTE DE EJECUCION. Este commit fija el alcance antes de construir o probar.
