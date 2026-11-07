# Identidad exacta del símbolo en control de IR TLS

El verificador distingue `tls_volatile_record` de `tls_volatile_records`
mediante el delimitador del símbolo LLVM, evitando que una coincidencia por
prefijo haga pasar el control del otro global. Pendiente repetir la verificación.
