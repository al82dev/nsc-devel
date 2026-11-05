# Tanda 08: sombra de global C con enum homónimo

- Se prepara un proyecto que declara un tipo NativeSharp `HiddenState` y usa un parámetro y un local `hidden_global`; el header C también declara `hidden_global` con un enum homónimo al tipo NativeSharp.
- El programa no utiliza el global C. Su compilación y ejecución deben conservar la precedencia de las variables NativeSharp sin exigir el enum externo. El caso está pendiente de validación agrupada.
