# Tanda 14: logs automáticos de error

## Objetivo

Conservar automáticamente el diagnóstico completo de cada compilación fallida sin exigir que el usuario prepare de antemano una ruta `--log-on-error`.

## Cambios realizados

- Una ejecución fallida de `nsc` crea `error.log` en el directorio de trabajo.
- Si ese archivo ya existe, se conserva y el siguiente fallo usa `error1.log`, luego `error2.log` y así sucesivamente.
- Cada log mantiene el formato transaccional existente e incluye código de salida, argumentos, stdout y stderr.
- La reserva del ordinal usa un directorio claim creado atómicamente. Dos compiladores concurrentes no pueden publicar sobre el mismo nombre.
- Después de publicar, NSC muestra la ruta absoluta del log para que pueda abrirse inmediatamente.
- `--log-on-error=<ruta>` conserva el destino explícito y evita crear un segundo log automático.
- `--log=<ruta>` también se considera una decisión explícita y evita duplicar la captura fallida.
- `--no-error-log` desactiva la creación automática para CI, pruebas negativas o entornos de sólo lectura.
- `--log-format=json` también se aplica al log automático cuando se solicita.
- Una ejecución exitosa no crea archivos de error.

## Archivos modificados

- `src/nsc/main.cpp`
- `src/Driver/Driver.cpp`

## Archivos nuevos

- `docs/compiler-optimization/TANDA-14-automatic-error-logs.md`: este informe. No se creó un subsistema de logging nuevo; se reutilizó la captura transaccional existente en el ejecutable.

## Por qué se hizo así

La captura ya rodeaba stdout y stderr del driver, por lo que el punto correcto para persistir un fallo es el ejecutable `nsc`, después de que el driver devuelve su código. Esto conserva mensajes de todas las fases sin añadir escrituras o locks al frontend y al backend.

Los nombres no se obtienen mediante una comprobación seguida de una escritura desprotegida. El claim por candidato serializa únicamente la selección del nombre, mientras compilaciones distintas siguen trabajando en paralelo.

## Mejora esperada

- Los fallos de compilaciones largas quedan disponibles aunque la terminal o la interfaz hayan recortado su salida.
- Ningún fallo nuevo destruye el diagnóstico del anterior.
- Las herramientas gráficas y scripts pueden mostrar la ruta anunciada sin implementar su propia captura de stderr.
- El costo en compilaciones exitosas se limita a la captura en memoria que ya usaba el sistema de logs; no se escribe ningún archivo.

## Verificación de la tanda

- Construcción de `nsc` en Debug/O0: correcta.
- Dos fallos controlados consecutivos produjeron `error.log` y `error1.log` con código, argumentos y stderr esperados.
- Un fallo con `--no-error-log` no produjo otro archivo.
- Un fallo con `--log-on-error=custom.log` produjo solamente `custom.log`.
- Dos procesos NSC fallidos lanzados simultáneamente produjeron `error2.log` y `error3.log`, sin colisión ni directorios claim residuales.
- `nsc --version` terminó correctamente sin crear un log.
- No se ejecutó baseline completo.
