# T08: IR del apuntado volatile en globales atómicos

El control no TLS incorpora los verificadores existentes de IR para las
lecturas y escrituras `volatile` directas e indexadas de unsigned int, short,
long long y double. Los verificadores no se han ejecutado todavía en esta
ruta; no sustituyen la regresión TLS ni acreditan todos los accesos.
