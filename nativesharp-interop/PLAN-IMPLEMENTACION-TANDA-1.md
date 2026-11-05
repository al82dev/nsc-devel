# Plan de implementación: primera tanda de integración con Clang

Estado: **plan propuesto; implementación no iniciada**. Se basa en [el análisis de la sesión Clang](ANALISIS-TANDA-1-SESION-CLANG.md) y en el enfoque acordado del [plan general](PLAN-CXX-NATIVESHARP.md).

El desglose por archivos, clases, interfaces, dependencias y pruebas se encuentra en [Plan detallado de la primera tanda](PLAN-DETALLADO-TANDA-1.md).

## Objetivo y alcance

Preparar NSC para abrir y cerrar una sesión de Clang oficial 23.1.0, de manera opcional y con configuración reproducible. Esta infraestructura permitirá después avanzar hacia el consumo directo de bibliotecas C/C++ desde NativeSharp. La primera tanda no implementa ese consumo completo ni cambia el núcleo de Clang. C++ consumiendo NativeSharp está fuera de alcance; Daikon y vcpkg no forman parte de esta tanda.

Trabajar en la rama de interoperabilidad de NSC, con construcciones, herramientas de prueba y resultados en A:. No desplegar en C: ni alterar la versión que se usa para los juegos. Conservar el comportamiento ordinario de NativeSharp y usar LLVM y Clang 23.1.0 hasta decisión posterior del usuario.

## Orden de trabajo

1. **Comprobar la base de trabajo.** Verificar rama y cambios existentes de NSC, la referencia congelada y que Clang y LLVM correspondan a la versión fijada. Identificar las rutas efectivas de bibliotecas y herramientas antes de construir. Registrar el estado observado sin modificar ramas ni archivos ajenos.
2. **Configurar el build opcional.** Añadir en el build de NSC una opción para habilitar la integración y descubrir los targets oficiales de Clang del mismo conjunto LLVM. Comprobar versión exacta, dependencias transitivas y compatibilidad del runtime del compilador. Con la opción apagada no se debe exigir Clang ni cambiar la ruta normal. La combinación con el host dinámico queda pendiente de comprobación expresa.
3. **Crear el componente interno.** Definir una configuración inmutable y una interfaz que cree, controle y cierre una sesión. Mantener las clases internas de Clang detrás de una implementación privada. Comunicar fallos claros y liberar correctamente los recursos ante errores, cancelación y cierre repetido.
4. **Conectar NSC.** Asociar el servicio a la compilación de proyecto y revisar las rutas de análisis, objeto, ejecutable y bibliotecas. Hacer explícita su propiedad para que ningún modelo use el contexto de Clang después de destruirlo. Conservar el funcionamiento de clientes existentes, incluido el servicio de lenguaje, con la integración desactivada.
5. **Configurar y diagnosticar.** Hacer llegar a la sesión el target, las opciones y rutas de recursos que correspondan. Conectar diagnósticos de Clang con NSC preservando severidad y ubicación cuando pueda hacerse fielmente. No convertir identificadores de archivo entre ambos compiladores sin traducción.
6. **Construir y verificar en A:.** Verificar primero la configuración desactivada y las regresiones NS relevantes con el ejecutable correcto de A:. Después verificar la sesión activada, versión y recursos, errores de configuración, cierre, cancelación y aislamiento entre configuraciones. Inspeccionar flags y dependencias reales del compilador construido. Antes de ejecutar pruebas, revisar sus rutas de compilador, prefijo, temporales y resultados para evitar acceso de escritura a C:.
7. **Documentar la entrega.** Registrar archivos modificados, configuración, comandos y resultados; distinguir las comprobaciones aprobadas de los puntos aún pendientes. Un error o diagnóstico de algo no soportado no cuenta como interoperabilidad.

## Criterio de terminación

La tanda estará terminada cuando la sesión pueda crearse y cerrarse con la configuración fijada, sus errores sean observables y las verificaciones del modo activado y del modo ordinario pasen con evidencia en A:. Si aparece una limitación de las interfaces oficiales de Clang, documentarla con evidencia antes de plantear un cambio excepcional en el fork. Las funciones, clases y demás capacidades de bibliotecas C/C++ se abordan en las siguientes tandas: cerrar esta infraestructura no cierra la interoperabilidad general.
