# Observación del consumidor NativeSharp de TLS en DLL

Con el fixture directo existente y `nsc` de desarrollo `f6b7ed644963e398`,
`observar-nsc.cmd` llega al enlace NSLink; este falla con `NSL1020` para
`_tls_index`. Por tanto, el rechazo semántico descrito anteriormente no
se aplica a la declaración actual `extern __declspec(thread) int` sin atributo
`dllimport`: NSC la proyecta como TLS estático incluso cuando la referencia de
proyecto declara `Runtime="api.dll"`.

El comportamiento debe distinguir el origen estático y DLL antes de emitir
el acceso, sin inferir que el fallo de NSLink sea el único problema. Este
script es de observación y no sustituye el gate original; no declara aprobado
el acceso TLS directo ni cambia NSLink.
