# Reproducción de callback TLS C

El control C pasó al asignar e invocar un callback global TLS, con aislamiento
en otro hilo, junto con los demás contratos del fixture. NSC
`7dd91639096aa9fa` rechazó la declaración en la proyección TLS de O0; la
variante DLL permanece pendiente hasta superar la estática.
