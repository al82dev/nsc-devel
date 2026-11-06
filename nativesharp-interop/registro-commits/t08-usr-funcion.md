# T08-002 y T08-003: USR de función importada

La proyección conserva el USR opcional de Clang en el símbolo. El agrupador
reúne identidades conocidas de una misma función y rechaza contradicciones,
sin mezclar automáticamente alias de nombres diferentes. No añade persistencia
ni acredita ABI; faltan pruebas y validación agrupada.
