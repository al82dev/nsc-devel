# Delegado generico con retorno T

Base de la campana 152654e5a0e2d84d4c2f245ea7590488f0497430. Caso del ejemplo 69,
A:/native-sharp-reference/REPORTE-PARA-WORKER-COMPILADOR-SDK.md.

## Alcance
Corregir la declaracion reportada delegate T Reducer<T>(...). No se modifican A:/nsc-devel,
ejemplos originales, C:/native-sharp, LLVM/Clang, Daikon ni interoperabilidad C/C++; no se
usan sus builds ni se hace merge, rebase, push o despliegue. Builds, temporales y evidencia
quedan en A. Las decisiones de lenguaje o API siguen pendientes; baseline y bateria completa
pertinentes siguen pendientes.

## Causa y cambio
El parser ya admite delegados genericos con retorno concreto, pero comprobo el tipo de retorno
antes de leer la lista generica y todavia no conocia T. Se reconoce el patron donde el retorno
es el primer parametro generico declarado despues del nombre; el resto del procesamiento
sigue usando las fases existentes. No se declara soporte de un parametro posterior ni de formas
mas complejas de retorno generico.

## Pruebas
La reproduccion historica copiada paso de los errores de parser a verificacion LLVM exitosa.
La prueba nsc.compiler.generic-delegate-return instancia e invoca Reducer<int> y el delegado
existente Predicate<int> con retorno concreto; fallo antes y paso despues en O0 y O2.
Evidencia: A:/nsc-fixes-evidencia/baseline-generic-delegate.log,
fixed-generic-delegate-final.log, generic-delegate-o2/ y cases/69-delegate/fixed-check.log.

clang-tidy clang-analyzer-* sobre DeclarationParser.cpp no informo errores y produjo un aviso
de padding en TypeSyntax.h, no modificado. No se ejecuto una linea base comparable. La bateria
general y otras formas de delegado siguen sin verificar.
