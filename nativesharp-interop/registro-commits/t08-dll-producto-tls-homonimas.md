# DLL NativeSharp consumidora de DLL TLS homónimas

La prueba crea en un directorio aislado dos DLL C `api.dll` diferentes y dos
bibliotecas NativeSharp que publican plantillas para acceder a sus variables
TLS. Después genera una DLL NativeSharp consumidora, elimina los encabezados
C y llama a sus funciones exportadas desde un proceso externo.

En O0–O3 se comprobó el estado de cada variable en el hilo principal y en dos
hilos adicionales sincronizados, el empaquetado de ambas DLL con alias, la
ejecución desde otro directorio y la restauración completa desde caché. Las
cuatro ejecuciones de `tls-dll-producto/validar.py` terminaron con código 0.

La muestra no tiene importaciones transitivas ambiguas entre las DLL C; ese
problema permanece abierto. Las DLL de control incluyen una importación Win32,
ya que el caso de control sin importaciones sigue bajo investigación.
