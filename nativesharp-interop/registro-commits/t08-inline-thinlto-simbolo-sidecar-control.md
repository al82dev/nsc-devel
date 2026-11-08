# T08-003: referencia a NativeSharp desde objeto restaurado de sidecar

La muestra añade una biblioteca NativeSharp que incluye la función C inline y
define el símbolo `HostCallback` al que esta llama. Un consumidor separado usa
la biblioteca sin referenciar el encabezado C. En O1–O3 debe consumir un
sidecar ThinLTO y conservar el resultado 42; O0 usa el archivo nativo ordinario.
El resultado se registrará al ejecutar el control.
