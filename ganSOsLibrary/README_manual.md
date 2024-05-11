# Manual de uso
Se especifican abajo funciones agregadas o modificadas respecto a las usadas en el tp0  

Se agregó tipo enum 'modulo'. Kernel es 0, Consola es 1, ...
```
typedef enum
{
	Kernel,
	Consola,
	CPU,
	FileSystem,
	Memoria
}modulo;
```

Agregados codigos de operación, utilizados por Kernel para saber qué función que quiere ejecutar CPU. 
```
typedef enum
{
	MENSAJE,
	PAQUETE,
	IDENTIDAD,
	RUNNING,
	YIELD,
	EXIT,
	I_O,
	F_OPEN,
	F_CLOSE,
	F_SEEK,
	F_READ,
	F_WRITE,
	F_TRUNCATE,
	WAIT,
	SIGNAL,
	CREATE_SEGMENT,
	DELETE_SEGMENT,
}op_code;
```

---

MODIFICADA, ACTUAL int **iniciar_servidor**(char* puertoEscucha);  
Antes no recibía parámetros. AHora recibe el puerto de escucha char* que se debe levantar del config de cada módulo.  

---

MODIFICADA, ACTUAL int **recibir_operacion**(int socket_cliente);  
Antes cerraba la conexión con `close()` en caso de no poder leer el stream y devuelve -1 para switchear.  
Ahora solo devuelve -1 SIN cerrar la conexión. Así pueden manejarse más operaciones antes de cerrar el socket  

---

MODIFICADA, ACTUAL t_config* **iniciar_config**(char* conf);  
Antes no recibia parámetros. Esta es generalizada, recibe ```"./nombreArchivo.config" ```  

---

NUEVA PARA KERNEL Y CPU: int **paquete_contexto**(int socket_dest, execContext contexto ,int motivo);  
Recibe un `contexto` de ejecución cargado, un "motivo" o codOp que corresponde con el motivo del envío, y lo envía al `socket_dest`  
NOTA: Kernel maneja PCBs, no contextos, por lo que debe crear y cargar un Contexto antes de utilizar la función  

---

NUEVA PARA KERNEL Y CPU: execContext **recibir_paquete_contexto**(int socket_cliente);  
Devuelve una estructura Contexto cargada con los datos provenientes del buffer.  
NOTA: Debe haberse aplicado `recibir_operacion` antes, la cual devuelve el codigo de operación.  

---
void **enviarIdentidad**(modulo ident, int sock_comm);  
Para enviar al módulo al que me conecto quién soy. ident debe ser el elemento de tipo enum `modulo` que corresponda.  
Ejemplo de invocación: enviarIdentidad(FileSystem, socket_mem);  
  
  
## FUNCIONES SOLO PARA MEMORIA DEBAJO
int **recibirIdentidad**(int socket_cliente);  
Devuelve la identidad del modulo que se comunicó por _socket_cliente_. 0 es Kernel, por ejemplo.  
El valor devuelto se debería ingresar a la lista devuelta por `inicializarListaSockets` o a un vector que siga las mismas reglas.  
IMPORTANTE: Antes de utilizar se deberia haber invocado a `recibirOperacion` para que lea el cod_op del paquete.  

---

t_list* **inicializarListaSockets**();  
Devuelve una _listaSockets_ (lista de commons) inicializada  

---

void **addConnectionToList**(t_list* listaSockets, ident, socket_comm);  //NO IMPLEMENTADA AUN  
Donde _ident_ es el valor devuelto por `recibirIdentidad`, que es el numero de identidad del modulo conectado por el socket _socketConectado_  

---

void **deleteConnectionFromList**(t_list* listaSockets, socket_comm); //NO IMPLEMENTADA AUN  
Para eliminar a un socket de la lista si se desconecta.  

---

modulo **identidadDelSocket**(t_list * listaSockets, int sock_comm);  //NO USAR,  NO IMPLEMENTADA CORRECTAMENTE AUN  
Devuelve el número de identidad del módulo que se haya conectado a través del socket _sock_comm_.  

---

int **select_socketCalling**(int socket_srv, fd_set current_sockets);  
Devuelve algún socket haciendo alguna petición en ese instante. Se tiene que usar en un bucle (implementación abajo)
- socket_srv es un socket devuelto por iniciar_servidor. Es el socket de escucha.
- current_sockets es una lista que select() usa para guardar los sockets activos y conectados. No nos importa a nosotros, solo la declaramos.  
  
Implementación:  
```
 //configs, iniciar_servidor, todo eso. iniciar_servidor devuelve socket_srv
	fd_set current_sockets;
	while(1){
		int socket_cl = select_socketCalling(socket_srv, current_sockets);
		
    // Hacer cosas con el socket_cl. Ejemplo, recibir_paquete. Todo el desarrollo el programa en estado de escucha va acá
		
		//Si el cliente se desconecta
		close(sock_cli);		
		FD_CLR(socket_cl, &current_sockets); //cerrar y libera el socket_cl de la lista porque ya fue atendido
	}
```
---


