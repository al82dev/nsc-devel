# Reproducción de escalares TLS volatile

El control C estático pasó con enteros sin signo y de 64 bits, double, bool y
`const volatile int` TLS. NSC `ab1886395db79d6f` rechazó las cuatro variantes
`volatile` adicionales en la proyección; el `const volatile int` de 32 bits
ya se proyectaba. El fixture DLL con CRT está preparado y pendiente.
