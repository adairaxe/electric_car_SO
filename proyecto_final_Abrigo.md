# Simulador de Vehículo Eléctrico

Este programa simula el funcionamiento de un vehículo eléctrico de cuatro ruedas.

## Requisitos

- Sistema operativo Unix-like (Linux, macOS)
- Compilador GCC
- Biblioteca POSIX Threads (pthread)

## Compilación

Para compilar el programa, sigue estos pasos:

1. Abre una terminal.
2. Navega al directorio que contiene el archivo fuente (por ejemplo, `vehiculo_electrico.c`).
3. Ejecuta el siguiente comando:

```
gcc vehiculo_electrico -o vehiculo_electrico.c -pthread 
```

Esto generará un ejecutable llamado `vehiculo_electrico`.

## Ejecución

Para ejecutar el programa, usa el siguiente comando:

```
./vehiculo_electrico <aceleracion> <velocidad_crucero>
```

Donde:
- `<aceleracion>` es un valor entero que representa la aceleración del vehículo.
- `<velocidad_crucero>` es un valor entero que representa la velocidad de crucero.

Ejemplo:
```
./vehiculo_electrico 5 100
```

## Notas

- Asegúrate de tener los permisos necesarios para crear y acceder a la memoria compartida.
- El programa utiliza entrada del usuario para controlar el vehículo. Sigue las instrucciones en pantalla.
