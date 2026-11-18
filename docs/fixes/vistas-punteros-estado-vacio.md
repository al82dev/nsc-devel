# Control de homónimos de punteros compartidos

La campaña ya había documentado una corrección que distingue las cuatro
familias reales de tipos homónimos cuyo destructor no admite estado vacío
(`estado-vacio-punteros-identidad.md`). Su evidencia provenía de otra copia.
Este control verifica la reproducción existente con el compilador de vistas;
no modifica los ejemplos que contiene `tests/integration/shared_default_identity.py`.

El HEAD al ejecutar era `2c9281e8a0c0dfd2ebd2b015f9ba7eb2eb62d4fa`, con otros
cambios de trabajo concurrentes. El ejecutable aislado
`A:/nsc-vistas-evidencia/build/bin/nsc.exe` tenía SHA-256
`5d1a19e76028e6e542f9ac4923dc5e3da4b73c62ffdea9ba8e97fa3aba5d4fb9`.
La configuración incremental y temporales corresponden a vistas en A.

Comando: `call A:\nsc-vistas-evidencia\vistas-env.cmd && python
A:\nsc-vistas-evidencia\shared-default-identity-cwd.py
A:\nsc-vistas-evidencia\build\bin\nsc.exe`. La copia del fixture cambia solo
el directorio de trabajo de los subprocesos al repositorio; `fc` confirmó esa
única diferencia. Usa `--no-artifact-cache` y el entorno sitúa los temporales
en A. El resultado completo está en
`A:/nsc-vistas-evidencia/shared-default-identity-vistas.log`.

Las 16 combinaciones devolvieron 42: `Rc`, `WeakRc`, `Arc` y `WeakArc`, cada
una con destructor directamente observable o anidado, en O0 y O2. En estos
casos no se ejecuta un destructor homónimo sobre el campo no inicializado cuando
falla el constructor exterior. El resultado es un control de la selección de
identidad y de esos ejemplos; los tipos son homónimos de prueba y no usan el
SDK binario real. No verifica todas las rutas de excepción, concurrencia ni el
ciclo completo de las familias, y no certifica un commit aislado.
