# Publicación condicional del receptor compartido

## Reproducción

En un constructor o destructor de clase payload, `Alias = Flag ? this : null`
publica el receptor cuando `Flag` es verdadero. El compilador aislado aceptó
semántica y emisión O0 de esa forma para Rc y Arc en ambos momentos; no se
ejecutaron los consumidores. El control directo `Alias = this` se rechazó por
publicación y el valor condicional formado por nulos emitió. Evidencia con
fuentes, comandos y hashes estables:
`A:/nsc-fixes-agente-2-evidencia/receiver-conditional-20261112/run-20261112T064942911197Z/results.json`.

El MIR `A:/nsc-fixes-agente-2-evidencia/receiver-conditional-mir-20261112/run-20261112T065025314121Z/Rc-conditional-constructor-publish.log`
contiene un `store` al estático cuyo valor es `conditional`: condición,
`this`, `null`. El recorrido `origins` en
`src/LLVM/Lowering/OopLlvmConstruction.cpp` no desciende por ese nodo, por lo
que `isReceiver` no detecta el valor proveniente de `this`.

La matriz de baseline ampliada registra ambas posiciones, un condicional
anidado, condición obtenida de `this.IsReady()`, publicación directa y
controles seguros, para Rc/Arc y constructor/destructor. Las 40 formas pasan
semántica; emiten las 16 condicionales publicadoras y los 20 controles
seguros, y las cuatro publicaciones directas se rechazan en emisión:
`A:/nsc-fixes-agente-2-evidencia/receiver-conditional-expanded-20261112/run-20261112T065154392063Z/results.json`.

## Corrección y validación dirigida

La corrección recorre únicamente las dos ramas de resultado de `Conditional`
con tres hijos en `origins`, y deja fuera la condición. La compilación
incremental y la regresión integrada pasaron: 20 casos publicadores rechazados
con el diagnóstico de publicación y 20 controles seguros emitidos. También
pasaron los controles de publicación directa y accesores binarios; la ausencia
de Core devolvió 77 y una ruta configurada inválida devolvió 1. Registro:
`A:/nsc-fixes-agente-2-evidencia/conditional-integrated-validation-20261112/run-20261112T065447591483Z/results.json`.
El registro conserva comandos, salidas y hashes de compilador, Core, metadata
y runtime antes y después; no se ejecutaron consumidores. La validación usó
un árbol con cambios locales y Core binario preexistente, no una revisión
limpia certificada.

El análisis es conservador: considera ambas ramas aunque una condición sea
constante. El recorrido también participa en la identificación de argumentos,
retornos y copias locales, pero los controles descritos solo acreditan las
formas medidas. No acredita otras expresiones, bibliotecas históricas ni todos
los caminos de publicación.
