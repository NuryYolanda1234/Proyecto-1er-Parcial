# Carreritas Dino

Juego infantil de dinosaurios con dos versiones:

- `index.html`: versión para jugar directamente en el navegador. No necesita instalar nada ni conectarse a Internet.
- `main.cpp`: versión de escritorio hecha con C++ y SFML.

## Abrir la versión HTML

1. Abre la carpeta `C:\Proyecto` en el Explorador de archivos.
2. Haz doble clic en `index.html`.
3. Pulsa **JUGAR** y elige un dinosaurio.

También puedes abrir `index.html` desde Visual Studio Code y usar **Open in Browser** si tienes esa opción instalada.

La primera vez que juegues, haz clic en la página para habilitar el audio del navegador.

## Controles

- **Espacio**, **W**, **flecha arriba** o toque en la pantalla: saltar.
- **P**: pausar o continuar.
- **Escape**: pausar; en los cuadros de confirmación, volver.
- **M**: activar o silenciar el sonido.
- En los menús, usa las **flechas** y **Enter**, o haz clic.

El juego tiene 10 niveles. Cada nivel dura 12 segundos; evita las cajas para avanzar.

## Audio

Los efectos de salto, choque, avance de nivel y pasos se generan en el navegador con Web Audio. No hacen falta archivos de audio externos.

## Versión C++ (opcional)

Si tienes CMake, un compilador C++ y SFML 3 instalados, desde PowerShell ejecuta:

```powershell
cmake -S C:\Proyecto -B C:\Proyecto\build
cmake --build C:\Proyecto\build
```

Después abre `file:///C:/Proyecto/index.html`.
