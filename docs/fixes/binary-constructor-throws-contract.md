# Contrato de excepción de constructores binarios

En `57eab24a8`, un constructor NativeSharp publicado como `throws Exception`
aparecía en el MIR de su productor con ese contrato y en su metadata ABI
con la misma excepción, pero el MIR del consumidor lo mostraba como no
lanzador. Como consecuencia, una fábrica compartida aceptaba una base binaria
lanzadora que debía permanecer detrás de la guarda de limpieza parcial.
La matriz previa está en `A:/nsc-fixes-evidencia/shared-imported-base-57eab24a8.log`;
el MIR del productor y consumidor y el ABI están junto a ese log, en la
carpeta homónima. El caso no ejecutó la variante que debía rechazarse.

`PublicSurfaceBuilder::emitConstructorMember` reconstruye ahora el contrato
de excepción ya presente en la metadata, del mismo modo que los métodos.
No se cambia el formato de metadata, el productor, otros miembros ni la
interoperabilidad C/C++.

El build incremental terminó 0; compilador SHA-256
`da0145daa255ab8f13031aa85dba1af05e4c995436ee29f2ba84b3b11a595e1e`.
La matriz `shared_throwing_imported_base.py` pasó: 16 ejecuciones con base
no lanzadora devolvieron 42, y la base declarada lanzadora se rechazó con la
guarda esperada (`A:/nsc-fixes-evidencia/shared-imported-base-throws-fix-20261112.log`).
La nueva prueba `binary_throwing_constructor_contract.py` construyó productor
y consumidor O0/O2, verificó en MIR las firmas lanzadora y no lanzadora, y
ejecutó cuatro combinaciones con captura de excepción y destrucción de un
campo ya inicializado, todas con salida 42. Comandos, hashes y salidas:
`A:/nsc-fixes-evidencia/binary-ctor-throws-final-20261112/`.

Una comprobación exploratoria de llamada sin catch fue aceptada; no se afirma
que el frontend exija capturar toda llamada lanzadora. Este cambio tampoco
acredita constructores con contratos de lifetime, otros tipos de error o
compatibilidad con metadata que omita el contrato. No se ejecutó batería
completa ni pruebas C/C++.
