# SCOPE LOCK - K&R NativeSharp

## Objetivo autorizado

Corregir los 53 proyectos de `A:\nsc-examples\KR-nativesharp` para que:

1. construyan limpiamente con `C:\native-sharp\bin\nsc.exe`, `O0` y `NSLink`;
2. conserven la intención didáctica de los ejemplos de Kernighan y Ritchie;
3. utilicen código directo, sencillo y legible;
4. no introduzcan `try`, `catch`, `throw`, `throws`, `unsafe`, clases contenedoras ni defensividad artificial.

## Naturaleza

Corrección funcional, migración de proyectos a los paquetes públicos actuales y reescritura pedagógica localizada.

## Comportamiento protegido

- Un proyecto ejecutable por cada ejemplo existente.
- Nombres, organización por capítulos y separación real de las cuatro unidades multifichero.
- Funciones libres dentro de namespaces.
- Compilación `O0` mediante `NSLink` y la instalación pública `C:\native-sharp`.
- Algoritmo y propósito pedagógico de cada ejemplo.

## Revisión correcta conocida

No existe una revisión limpia actual. La auditoría del 10 de octubre de 2026 obtuvo 48/53 únicamente con
artefactos `2.0.0-dev` conservados en `obj`; la restauración limpia está rota.

## Repositorios y componentes autorizados

- Fuentes, proyectos, manifiestos, locks y README de `A:\nsc-examples\KR-nativesharp`.
- Este documento de alcance.

## Protegido

- `A:\nsc-devel` salvo este documento.
- Código del compilador, NSLink, runtime y bibliotecas instaladas.
- `C:\native-sharp`: sólo lectura y ejecución; queda prohibido instalar o modificar.
- El resto de `A:\nsc-examples`.

## Validación obligatoria

- Auditoría exhaustiva de 53 proyectos y 57 fuentes.
- Restauración o resolución de dependencias sin reutilizar paquetes `2.0.0-dev` locales.
- Construcción limpia de los 53 proyectos con el compilador oficial, `O0` y `NSLink`.
- Segunda búsqueda de APIs que sustituyan el algoritmo enseñado y de construcciones prohibidas.

## Acciones prohibidas

- Modificar el compilador para hacer pasar los ejemplos.
- Instalar, desplegar o escribir en `C:\native-sharp`.
- Eliminar ejemplos, reducir el inventario o convertir fallos en omisiones.
- Sustituir algoritmos por llamadas de biblioteca de mayor nivel.
- Ejecutar baseline o pruebas ajenas a esta colección.

## Criterio de terminación

- 53/53 proyectos construyen limpiamente.
- 57/57 fuentes fueron revisados.
- Cero dependencias `2.0.0-dev`, rutas absolutas obsoletas y residuos de `try`/`throw`/`unsafe`.
- Cada ejemplo conserva el concepto anunciado por su capítulo o queda bloqueado con evidencia concreta.

## Resultado del 10 de octubre de 2026

- Inventario cerrado: 53 proyectos, 57 fuentes, 53 manifiestos y 53 locks.
- Construcción limpia: 53/53 con `C:\native-sharp\bin\nsc.exe`, `O0` y `NSLink`.
- La matriz inicial construyó 52/53; el restante estaba bloqueado por una ejecución antigua del propio ejemplo.
  Después de detener exclusivamente ese proceso y retirar su residuo transaccional, `nsc clean` y `nsc build`
  terminaron con código cero.
- Auditoría posterior: cero etiquetas `<Language>`, referencias `2.0.0-dev`, `NativeSharp.Core.IO`,
  `LibraryReference` duplicadas, excepciones, bloques `unsafe` y atajos de biblioteca inventariados.
- Procesos de los ejemplos todavía activos al cerrar: cero.
- No se modificó ni instaló contenido bajo `C:\native-sharp`.
