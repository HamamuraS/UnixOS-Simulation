#ifndef HEADER_H
#define HEADER_H

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/select.h>
#include<sys/time.h>
#include <sys/types.h>
#include<sys/socket.h>
#include<netdb.h>
#include<unistd.h> //sleep, usleep
#include<signal.h>
#include<pthread.h>
#include<semaphore.h>
#include<assert.h>
#include<readline/readline.h>
#include<commons/log.h>
#include<commons/collections/list.h>
#include<commons/config.h>
#include<commons/string.h>
#include<math.h>

#define _GNU_SOURCE


#define PUERTO "8002"

void saludar();

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
	MOV_IN,
	MOV_OUT,
	SET,
	ERROR,
	COMPACTION
}op_code;

typedef struct
{
	int size;
	void* stream;
} t_buffer;

typedef struct
{
	op_code codigo_operacion;
	t_buffer* buffer;
} t_paquete;

typedef enum
{
	Kernel,
	Consola,
	CPU,
	FileSystem,
	Memoria
}modulo;

typedef struct{
	int socketConn;
	modulo identidad;
}structIdent;

typedef char* Registro;

#define CANTIDAD_REGISTROS_CPU 12

typedef struct{
	int ID;
	int base;
	int limite;
}Segmento;

typedef struct{
	int PID;
	t_list*instrucciones;
	int program_counter;
	char* registros[CANTIDAD_REGISTROS_CPU]; //se sabe 12 registros en CPU
	t_list*parametros;
	t_list* tablaSegmentos;
	t_list* tablaArchivosProcesos;
}execContext;



//el paquete del contexto de ejecución tiene formato:
//codOp - size total - cantINstr - [sizeInstr1 - instr1 - sizeInstr2 - instr2 -...] - programCounter - [sizeAX - AX - sizeBX - ...]

void enviarIdentidad(modulo ident, int sock_comm); //sock_comm es el devuelto por crear_conexion, socket de comunicacion
int recibirIdentidad(int socket_cliente); //devuelve modulo, identidad
t_list* inicializarListaSockets(); //devuelve lista inicializada en NULL
void addConnectionToList(t_list* listaSockets, modulo ident, int socket_comm);
modulo deleteConnectionFromList(t_list* listaSockets, int socket_comm);
void liberarListaSockets(t_list* lista); //OBLIGATORIA despues de inicializarLIstaSockets. Libera la memoria de la lista y sus nodos
modulo identidadDelSocket(t_list *, int);

t_config* iniciar_config(char*);
int socket_create(char* ip, char* puerto);
int iniciar_servidor();
int esperar_cliente(int);
int crear_conexion(char *ip, char* puerto);
void enviar_mensaje(char* mensaje, int socket_cliente);
char* recibir_mensaje(int);

void empaquetar_lista(t_paquete* paquete, t_list* lista);
void empaquetar_registros(t_paquete* paquete, char* registros[]);
int paquete_contexto(int socket_dest, execContext contexto ,int motivo);

//-------------------------------------------------------RECEPCION DE PAQUETES---------------------------------------------------

int recibir_operacion(int);
t_list* recibir_paquete(int); //recibe considerando que lo que viene en el buffer es sucesion de int y char*
void* recibir_buffer(int*, int);
void iterator(char* value);
execContext recibir_paquete_contexto(int socket_cliente);

//-------------------------------------------------------ENVIO DE PAQUETES---------------------------------------------------------
	//NOTA: Estructura ordenada de paquete final: (Int)op_code (Int)t_buffer.size (void*)t_buffer.stream
/*
 +op_code ocupa el tamaño de un int pero en realidad es tipo enum. Dice si el t_buffer.stream contiene mensaje o paquete
 +t_buffer.size es el tamaño en bytes de t_buffer.stream
 +t_buffer.stream internamente es [size.mensaje1, mensaje1, size.mensaje2, mensaje2, ... ] para saber hasta donde leer
 al recibir varios mensajes
  */
void paquete(int conexion); //esta función llama a todas las de abajo y pide mensajes por consola para empaquetar
							//es la que hace todo el laburo
t_paquete* crear_paquete(void); //Reserva memoria para un t_paquete* y le asigna PAQUETE a su op_code
t_paquete* crear_paquete_contexto(int motivoDesalojo);
void crear_buffer(t_paquete* paquete); //Reserva memoria para el puntero a estructura t_buffer*, inicializa size=0
void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio); //recibe un paquete, un stream y su longitud. Actualiza size de t_buffer y t_buffer
int enviar_paquete(t_paquete* paquete, int socket_cliente);
int enviar_paquete_contexto(t_paquete* paquete, int socket_cliente);
void* serializar_paquete(t_paquete* paquete, int bytes); //devuelve un stream con opcode, buffer size y el buffer de forma contigua
void eliminar_paquete(t_paquete* paquete);
int send_newsegment_petition(int socket_dest, int PID, int segID, int size);
//---------------------------------------------------------------------------------------------------------------------

int select_socketCalling(int, fd_set);

t_log* logger;

#endif



