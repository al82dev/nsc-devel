# Evidencia de la barrera TLS de DLL

Con `nsc` de desarrollo `deffd2b4770d5278` se construyeron incrementalmente
`nsc` y `nsc_cpp_session_tests`. `validar-guardia-dll.cmd` confirmó el
diagnóstico específico para el proyecto con `Runtime="api.dll"`. Las pruebas
de global estático y escalares TLS de C volvieron a pasar de O0 a O3, incluido
el control C, el IR `volatile` y la escritura `const` rechazada.

Se incorpora la prueba negativa a la batería de tanda sin retirar el caso
positivo que aún falla. No se ejecutó ni aprobó el gate completo de tanda 8;
la barrera se debe sustituir cuando el acceso TLS de DLL esté implementado.
