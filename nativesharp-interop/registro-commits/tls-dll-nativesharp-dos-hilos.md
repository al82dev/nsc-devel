# Acceso TLS de DLL desde NativeSharp en dos hilos

La DLL de prueba crea un hilo e invoca un callback de NativeSharp. Ese código
lee el global TLS, lo escribe y vuelve a leerlo; la DLL comprueba las mismas
posiciones y el hilo principal conserva su valor independiente. Se ejecutó
en O0–O3 con NSC `8e8795a3d555ae35` y NSLink sin cambios.

El primer enlace de la DLL de prueba solicitó `memset` y soporte de
excepciones del CRT; se añadieron las bibliotecas estáticas correspondientes
al guion y la repetición pasó. Esta prueba cubre `int` C en Windows x64,
no demuestra toda la superficie TLS ni cierra la tanda 8.
