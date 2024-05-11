# tp-2023-1c-ganSOs

Clonar con ```git clone https://github.com/sisoputnfrba/tp-2023-1c-ganSOs```   
  
## Configuración para PROYECTOS
En eclipse, abrir la *carpeta de cada módulo* como un proyecto por separado.  
**Issue 1:** Eclipse no reconoce las carpetas como "C Project" porque los metadatos de eclipse no los subimos al repo
1. Click derecho en la carpeta del proyecto -> ```New``` -> ```Convert to C/C++ Project```
2. Tildar la carpeta del proyecto, seleccionar *C Project*. En Project Type: seleccionar *Executable* y en Toolchains: seleccionar *Linux GCC*.
3. Finish

**Issue 2:** Hay que linkear las commons de la cátedra y la librería readline  
1. Click derecho en la carpeta del proyecto -> Properties -> C/C++ Build -> Settings -> GCC C Linker/Libraries  
2. En el recuadro de `Libraries (-l)` tocar `Add...` y agregar las palabras "commons" y "readline"  

**Issue 3:** Linkear nuestra libreria casera  *con los siguientes dos procedimientos PEDORROS*   
1. Click derecho en la carpeta del proyecto -> Properties -> C/C++ Build -> Settings -> GCC C Compiler/Includes
2. Incluir la ruta "/home/utnso/tp-2023-1c-ganSOs/ganSOsLibrary/src" en `Include paths (-I)`
3. En `Include files (-include)`, agregar las direcciones "/home/utnso/tp-2023-1c-ganSOs/ganSOsLibrary/src/funciones.c" y "/home/utnso/tp-2023-1c-ganSOs/ganSOsLibrary/src/header.h"  

A. Click derecho en la carpeta del proyecto -> Properties -> C/C++ General -> Path and Symbols  
B. En `Lenguages`, seleccionar GNU C
C. Agregar la dirección "tp-2023-1c-ganSOs/ganSOsLibrary/src"  
D. Ahora dentro del codigo fuente ponés este include textual: #include "../../ganSOsLibrary/src/header.h"  

## Configuración para LIBRERIA COMPARTIDA
Porque no es la misma configuración que para los demás proyectos :(  

**Issue 1:** Eclipse no reconoce la carpeta como una librería compartida.  
1. Click derecho en la carpeta de la libreria  -> ```New``` -> ```Convert to C/C++ Project```
2. Tildar la carpeta de la libreria, seleccionar *C Project*. En Proyect Type seleccionar **Shared Library** y en Toolchains seleccionar *Linux GCC*  
  
**Issue 2:** Hay que linkear las commons de la cátedra, la librería readline y FLAGS DE CONFIGURACION
1. Click derecho en la carpeta de la libreria -> Properties -> **C/C++ Build** -> Settings -> GCC C Linker/Libraries  
2. En el recuadro de `Libraries (-l)` tocar `Add...` y agregar las palabras "commons" y "readline"  
3. En **C/C++ Build**, en la carpeta `Shared Library Settings`, tildar el casillero `Shared (-shared)`
4. En **C/C++ Build**, en la carpeta `Miscellaneos`, tildar el casillero `Position Independent Code`

**Issue 3:** Un par de inclusiones más que no se que hacen pero parece que son importantes(?)  
1. Click derecho en la carpeta de la libreria -> Properties -> **C/C++ Build** -> Settings -> GCC C Compiler/Includes
2. Incluir la ruta "/home/utnso/tp-2023-1c-ganSOs/ganSOsLibrary/src" en `Include paths (-I)`
3. Click derecho en la carpeta del proyecto -> Properties -> **C/C++ General** -> Path and Symbols  
4. En `Lenguages`, seleccionar GNU C
5. Agregar la dirección "tp-2023-1c-ganSOs/ganSOsLibrary/src"  
