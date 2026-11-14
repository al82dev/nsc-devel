# T09-003: tamaño oculto en métodos C++ de instancia

En Windows x64, Clang coloca el receptor `this` antes de los parámetros declarados. Para un método con `pass_object_size` seguido de `bool`, la firma observada es `(ptr this, ptr, i64, i1 zeroext)`. La importación de NSC rechazaba ese método. Ahora conserva el atributo, calcula el tamaño sin reevaluar el argumento e inserta el tamaño después del puntero; el atributo del booleano se aplica a su posición física. El índice constante de un subobjeto se conserva en las rutas HIR de llamadas habituales e iterativas.

## Validación

- `validar-metodo-instancia.py`: ocho métodos con `pass_object_size` y `pass_dynamic_object_size`, modos 0–3, receptor `const`, argumento `bool` y subobjeto con índice constante. Controles Clang y NativeSharp pasaron O0–O3 en biblioteca estática (`A:/temp/nsc-t09-instance-object-size-jdtn52xa`) y DLL (`A:/temp/nsc-t09-instance-object-size-6m5uxpiu`). Las declaraciones y llamadas LLVM de la muestra conservan el `zeroext`.
- La prueba de métodos estáticos y consumidores precompilados pasó, incluidos sus tres rechazos (`A:/temp/nsc-t09-size-static-method-_uvuzwbv`). La regresión C directa pasó en O0–O3 con biblioteca estática y DLL (`A:/temp/nsc-t09-c-object-size-kqbptexo`). Cinco controles seleccionados de tipos de valor y ABI pasaron.
- En la primera revisión, la prueba con `--precompilado` falló porque el método de instancia no se reconstruía al consumir una plantilla genérica, con encabezado (`A:/temp/nsc-t09-instance-object-size-5qht0y_s/O0-con-encabezado-build.log`) o sin él (`A:/temp/t09-instance-pre-noheader-987cdfa34d1e40d18138310ef80e9bcb/resultado.log`).

## Límites

La primera muestra directa no certificaba rutas genéricas precompiladas, constructores, métodos virtuales, otras convenciones, todos los tipos de parámetros ni efectos laterales de índices. La actualización inferior acredita únicamente la ruta precompilada descrita allí. La extracción del cálculo compartido preserva el cuerpo usado por las llamadas estáticas; las formas no incluidas en estas pruebas requieren validación propia. T09-003 y la tanda 9 continúan abiertos.

## Reproducción adicional pendiente de captura

La prueba ahora incorpora dos tipos distintos, `Probe` y `Other`, con un método homónimo `size0` que produce resultados diferentes. Ambos funcionan en la ruta directa con biblioteca estática (`A:/temp/nsc-t09-instance-object-size-pvqg29vy`) y DLL (`A:/temp/nsc-t09-instance-object-size-bbadntwu`), O0–O3. La plantilla genérica con ambos falló inicialmente al reconstruir los métodos (`A:/temp/nsc-t09-instance-object-size-vq0o6j6v/O0-con-encabezado-build.log`). La selección de dependencias debe conservar la relación entre receptor y propietario y no asociar métodos únicamente por nombre.

El trabajo posterior dispone de un selector interno que relaciona variables receptoras con inicializadores de fábrica ya vinculados a Clang; se probó con dos propietarios, métodos homónimos, referencias ambiguas y receptores reasignados, escapados o sombreados. La prueba de catálogo también pasó para métodos de instancia, estáticos y enumeradores juntos. Una sesión C++ real comprobó la selección y captura de ambos métodos. La selección y el transporte se verificaron antes de conectar el consumidor; los resultados posteriores están a continuación.

## Resultado posterior: plantillas precompiladas

El productor identifica métodos de instancia usados a través de locales cuya inicialización se enlaza a una única fábrica C++ de cero argumentos y origen verificable. El catálogo vigente conserva propietario, USR, firma y constancia del receptor. Al restaurar el método se cotejan identidad, contrato y huellas de los archivos de biblioteca cuando hay una observación viva.

La prueba dirigida ejecuta llamadas directas y desde una plantilla genérica a dos clases con métodos homónimos y resultados diferentes. Pasa con y sin encabezado, O0–O3, tanto con biblioteca estática (`A:/temp/nsc-t09-instance-object-size-beu506di`) como con DLL (`A:/temp/nsc-t09-instance-object-size-0rz6ri9r`). Los controles rechazan firma distinta, `.lib` distinto, DLL modificada y receptor reasignado. La prueba de identidad incluye Clang real, selecciones con sombra y escape, catálogo combinado y truncamiento; pasó. También pasaron cinco controles CTest seleccionados de valores y ABI, métodos estáticos precompilados (`A:/temp/nsc-t09-size-static-method-6n_bbjvs`) y funciones libres (`A:/temp/nsc-t09-object-size-pre-tz3ncspi`).

La captura inicial fue conservadora: requería un local único sin reasignación ni escape, receptor directo por flecha y una fábrica sin argumentos con vinculación y referencia únicas. Una actualización posterior admite las reasignaciones demostradas en [su prueba específica](t09-receptor-reasignado.md). Otras formas de receptor, fábricas con argumentos, clases no triviales, métodos virtuales y combinaciones ABI distintas no quedan acreditadas. El rechazo inicial del receptor reasignado evitaba atribuirle un método equivocado; la actualización posterior documenta las formas ya admitidas y las que siguen rechazadas. T09-003 y la tanda 9 siguen abiertos.
