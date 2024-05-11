#ifndef MEMORIA_H
#define MEMORIA_H
#include "../../ganSOsLibrary/src/header.h"

const int BEST_ID=0;
const int FIRST_ID=1;
const int WORST_ID=2;

typedef struct{
	t_list* connectedSockets;
	bool *modConectado;
}mCIVariables; //manejarConexionesIniciales Variables

t_dictionary *algoritmos_dictionary;

int socketFS, socketKer, socketCPU;
void* user_space;
sem_t user_space_isFree;
Segmento* segmento_0;
t_list*tabla_inicial; //contiene el segmento_0 y es enviada como copia por unica vez a cada proceso
t_list* huecos_libres; //tabla global con todos los huecos libres

t_dictionary * diccionario_tablas;

typedef struct{
	char * PUERTO_ESCUCHA;
	int TAM_MEMORIA;
	int TAM_SEGMENTO_0;
	int CANT_SEGMENTOS;
	int RETARDO_MEMORIA;
	int RETARDO_COMPACTACION;
	char * ALGORITMO_ASIGNACION;
}memConfig;
memConfig configData;

memConfig cargarConfigData(t_config* config){
	memConfig configData;
	configData.ALGORITMO_ASIGNACION = config_get_string_value(config, "ALGORITMO_ASIGNACION");
	configData.CANT_SEGMENTOS = config_get_int_value(config, "CANT_SEGMENTOS");
	configData.PUERTO_ESCUCHA = config_get_string_value(config, "PUERTO_ESCUCHA");
	configData.RETARDO_COMPACTACION = config_get_int_value(config, "RETARDO_COMPACTACION");
	configData.RETARDO_MEMORIA = config_get_int_value(config, "RETARDO_MEMORIA");
	configData.TAM_MEMORIA = config_get_int_value(config, "TAM_MEMORIA");
	configData.TAM_SEGMENTO_0 = config_get_int_value(config, "TAM_SEGMENTO_0");
	return configData;
}

t_dictionary * cargarDiccionarioAlgoritmos(){
	t_dictionary *algoritmos_dictionary=dictionary_create();
	dictionary_put(algoritmos_dictionary, "BEST", (void*)&BEST_ID);
	dictionary_put(algoritmos_dictionary, "FIRST", (void*)&FIRST_ID);
	dictionary_put(algoritmos_dictionary, "WORST", (void*)&WORST_ID);
	return algoritmos_dictionary;
}

typedef struct{
	int pos;
	int size;
}Hueco;

Hueco*minimum(Hueco* a, Hueco* b){
	return a->size<=b->size?a:b;
}

Hueco*maximum(Hueco*a, Hueco*b){
	return a->size>=b->size?a:b;
}

t_list* iniciar_lista_huecos_libres(){
	t_list* lista_huecos_libres=list_create();
	Hueco* hueco_inicial=(Hueco*)malloc(sizeof(Hueco));
	hueco_inicial->pos=0; hueco_inicial->size=configData.TAM_MEMORIA;
	list_add(lista_huecos_libres, hueco_inicial);
	return lista_huecos_libres;
}

void crear_hueco_libre(int pos, int size){
	Hueco* nuevo_hueco=(Hueco*)malloc(sizeof(Hueco));
	nuevo_hueco->pos=pos; nuevo_hueco->size=size;
	log_warning(logger, "Se creó un hueco en la posición %d, de tamaño %d", pos, size);
	list_add(huecos_libres, nuevo_hueco);
}


void alojar_nuevo_hueco(int nueva_base, int nuevo_limite){
	bool es_adyacente_inferior(Hueco* a){
		return (a->pos+a->size)==nueva_base;
	}
	bool es_adyacente_superior(Hueco* a){
		return (nueva_base+nuevo_limite)==a->pos;
	}
	Hueco* hueco_adyacente_inferior=list_find(huecos_libres, (void*)es_adyacente_inferior);
	Hueco* hueco_adyacente_superior=list_find(huecos_libres, (void*)es_adyacente_superior);

	if(hueco_adyacente_inferior!=NULL && hueco_adyacente_superior!=NULL){
		//existe un hueco inferior y superior, elimino el superior y el inferior ocupará todo el espacio
		hueco_adyacente_inferior->size+=(nuevo_limite+hueco_adyacente_superior->size);
		list_remove_element(huecos_libres, hueco_adyacente_superior);
		log_warning(logger, "Hueco en %d fusionado con hueco en %d. Nuevo tamaño: %d", hueco_adyacente_inferior->pos, hueco_adyacente_superior->pos,hueco_adyacente_inferior->size);
		free(hueco_adyacente_superior);
	}
	else if(hueco_adyacente_inferior!=NULL && hueco_adyacente_superior==NULL){
		//Solo hay un hueco inferior, no hay hueco superior
		hueco_adyacente_inferior->size+=nuevo_limite;
		log_warning(logger, "El hueco en la posición %d expandió su tamaño a %d", hueco_adyacente_inferior->pos, hueco_adyacente_inferior->size);
	}
	else if(hueco_adyacente_inferior==NULL && hueco_adyacente_superior!=NULL){
		//Solo existe hueco superior, no hay inferior
		log_warning(logger,"El hueco en %d ahora se ubica en %d y expandió su tamaño a %d", hueco_adyacente_superior->pos, nueva_base, hueco_adyacente_superior->size+nuevo_limite);
		hueco_adyacente_superior->pos=nueva_base;
		hueco_adyacente_superior->size+=nuevo_limite;
	}else{
		//no existen huecos adyacentes
		crear_hueco_libre(nueva_base, nuevo_limite);
	}
}

void allocate_segment(t_list* tabla, Segmento* segm, int ID, int pos, int size){
	segm->ID=ID;
	segm->base=pos;
	segm->limite=size;
	list_add(tabla, segm);
	log_info(logger, "Segmento alojado - ID: %d - Base: %d - Tamaño: %d", ID, pos, size);
}

//si hay lugar, crea un segmento (create) y lo asigna (alloc) en la tabla indicada
int calloc_segment(t_list* tabla, int ID, int size){
	bool segment_fits (Hueco*a){
		return a->size>=size;
	}
	Hueco*sumarSizes(Hueco* a, Hueco* b){
		a->size+=b->size;
		return a;
	}
	Segmento *newSegment=(Segmento*)malloc(sizeof(Segmento));
	t_list* huecos_utiles;
	//int *algoritmoId=dictionary_get(algoritmos_dictionary, configData.ALGORITMO_ASIGNACION);

	if(strcmp("BEST", configData.ALGORITMO_ASIGNACION)==0){
		//BEST
		huecos_utiles=list_filter(huecos_libres, (void*)segment_fits);
		if(list_is_empty(huecos_utiles)){
			//Manejo de NO SE ENCONTRÓ HUECO ÚTIL
			t_list* huecos_libres_copy=list_duplicate(huecos_libres);
			Hueco*huecoAcumuladorSize=(Hueco*)malloc(sizeof(Hueco));
			huecoAcumuladorSize->size=0;
			huecoAcumuladorSize=list_fold(huecos_libres_copy, huecoAcumuladorSize, (void*)sumarSizes);
			//list_destroy_and_destroy_elements(huecos_libres_copy, (void*)hueco_destroyer);
			free(newSegment);
			return huecoAcumuladorSize->size>=size?1:-1; //devuelve 1 si la suma de tamaños de huecos es util. SE necesita compactación
		}
		Hueco *menorHueco=list_get_minimum(huecos_utiles, (void*)minimum);
		//creando (malloc) segmento y guardandolo en la tabla de segmentos
		allocate_segment(tabla, newSegment, ID, menorHueco->pos, size);
		menorHueco->size-= size;
		//actualizar posición del hueco
		if(menorHueco->size==0){
			//nuevo tamaño de hueco es cero ENTONCES elimino el hueco de la lista
			list_remove_element(huecos_libres, menorHueco);
			log_warning(logger, "Se eliminó un hueco en la posición %d. Llenado por completo.", menorHueco->pos);
			free(menorHueco);
		}else{
			int posAnterior=menorHueco->pos;
			menorHueco->pos+=size; //hueco queda desplazado por el tamaño del nuevo segmento
			log_warning(logger, "Hueco desplazado de %d hacia %d y acotado a %d bytes",posAnterior, menorHueco->pos, menorHueco->size);
		}
		list_destroy(huecos_utiles);
	}
	else if(strcmp("FIRST", configData.ALGORITMO_ASIGNACION)==0){
		//FIRST
		Hueco* menorBase(Hueco* a, Hueco* b){
			return a->pos<=b->pos?a:b;
		}
		huecos_utiles=list_filter(huecos_libres, (void*)segment_fits);
		if(list_is_empty(huecos_utiles)){
			//Manejo de NO SE ENCONTRÓ HUECO ÚTIL
			t_list* huecos_libres_copy=list_duplicate(huecos_libres);
			Hueco*huecoAcumuladorSize=(Hueco*)malloc(sizeof(Hueco));
			huecoAcumuladorSize->size=0;
			huecoAcumuladorSize=list_fold(huecos_libres_copy, huecoAcumuladorSize, (void*)sumarSizes);
			//list_destroy_and_destroy_elements(huecos_libres_copy, (void*)hueco_destroyer);
			free(newSegment);
			return huecoAcumuladorSize->size>=size?1:-1; //devuelve 1 si la suma de tamaños de huecos es util. SE necesita compactación
		}
		Hueco *primerHueco=list_get_minimum(huecos_utiles, (void*)menorBase);


		//creando segmento y guardandolo en la tabla de segmentos
		allocate_segment(tabla, newSegment, ID, primerHueco->pos, size);
		primerHueco->size-= size;
		//actualizar posición del hueco
		if(primerHueco->size==0){
			//nuevo tamaño de hueco es cero ENTONCES elimino el hueco de la lista
			list_remove_element(huecos_libres, primerHueco);
			log_warning(logger, "Se eliminó un hueco en la posición %d. Filled.", primerHueco->pos);
			free(primerHueco);
		}else{
			int posAnterior=primerHueco->pos;
			primerHueco->pos+=size; //hueco queda desplazado por el tamaño del nuevo segmento
			log_warning(logger, "Hueco desplazado de %d hacia %d y acotado a %d bytes",posAnterior, primerHueco->pos, primerHueco->size);
		}
		list_destroy(huecos_utiles);
	}
	else if(strcmp("WORST", configData.ALGORITMO_ASIGNACION)==0){
		//WORST
		huecos_utiles=list_filter(huecos_libres, (void*)segment_fits);
		if(list_is_empty(huecos_utiles)){
			//Manejo de NO SE ENCONTRÓ HUECO ÚTIL
			t_list* huecos_libres_copy=list_duplicate(huecos_libres);
			Hueco*huecoAcumuladorSize=(Hueco*)malloc(sizeof(Hueco));
			huecoAcumuladorSize->size=0;
			huecoAcumuladorSize=list_fold(huecos_libres_copy, huecoAcumuladorSize, (void*)sumarSizes);
			//list_destroy_and_destroy_elements(huecos_libres_copy, (void*)hueco_destroyer);
			free(newSegment);
			return huecoAcumuladorSize->size>=size?1:-1; //devuelve 1 si la suma de tamaños de huecos es util. SE necesita compactación
		}
		Hueco *mayorHueco=list_get_maximum(huecos_utiles, (void*)maximum);
		//creando (malloc) segmento y guardandolo en la tabla de segmentos
		allocate_segment(tabla, newSegment, ID, mayorHueco->pos, size);
		mayorHueco->size-= size;
		//actualizar posición del hueco
		if(mayorHueco->size==0){
			//nuevo tamaño de hueco es cero ENTONCES elimino el hueco de la lista
			list_remove_element(huecos_libres, mayorHueco);
			log_warning(logger, "Se eliminó un hueco en la posición %d. Filled.", mayorHueco->pos);
			free(mayorHueco);
		}else{
			int posAnterior=mayorHueco->pos;
			mayorHueco->pos+=size; //hueco queda desplazado por el tamaño del nuevo segmento
			log_warning(logger, "Hueco desplazado de %d hacia %d y acotado a %d bytes",posAnterior, mayorHueco->pos, mayorHueco->size);
		}
		list_destroy(huecos_utiles);
	}else{
		log_error(logger, "rompiste algo flaco");
		exit(1);
	}
	//list_destroy_and_destroy_elements(huecos_utiles, (void*)hueco_destroyer); creo que esto borraria huecos de la lista principal
	return 0;
}


pthread_t hiloRecepcionista, hiloAtendedor; //recepcionista utiliza recibirConexionesIniciales, atendedor utiliza manejarConexionesIniciales
sem_t faltanConexiones, hayConexion; //para proteger escritura y lectura de listaWaitingSockets, lista compartida
							//utilizo dos semaforos para sincronizar el orden de ejecución. Estrategia productor-consumidor

t_list* listaWaitingSockets; //lista con "los sockets conectados en espera a ser atendidos". Siempre habrá 0 o 1 socket en espera

void recibirConexionesIniciales(void* args){
	int*socket_mem=(int*)args;
	while(1){
		sem_wait(&faltanConexiones);
		int client_socket=esperar_cliente(*socket_mem); //REGION CRITICA. Agregando socket a lista en memoria compartida
		int *socketHeap=(int*)malloc(sizeof(int));
		memcpy(socketHeap, &client_socket, sizeof(int));
		list_add(listaWaitingSockets, socketHeap);
		sem_post(&hayConexion);
	}pthread_exit(NULL); //en realidad nunca llega a esta linea, el otro hilo mata a este hilo cuando se supera el while
}

void manejarConexionesIniciales(void* args){	//recibe array bool de flags, lista enlazada de identidades
	mCIVariables* variables = (mCIVariables*)args; //CASTEO void* de vuelta a mCIVariables, estructura con variables
	while(!variables->modConectado[Kernel] || !variables->modConectado[CPU] || !variables->modConectado[FileSystem]){
		sem_wait(&hayConexion);
		int*client_socket=(int*)list_remove(listaWaitingSockets, 0); //REGION CRITICA, desencolando socket de lista en memoria compartida
		sem_post(&faltanConexiones);
		int cod_op = recibir_operacion(*client_socket); // devuelve -1 si no se puede leer el paquete
		switch (cod_op) {
			case MENSAJE:
				log_error(logger, "Recibido mensaje pero aún no se conectaron todos los módulos");
				break;
			case PAQUETE:
				log_error(logger, "Recibido paquete pero aún no se conectaron todos los módulos");
				break;
			case IDENTIDAD:
				int ident = recibirIdentidad(*client_socket); //ident va del 0 al 4
				addConnectionToList(variables->connectedSockets, ident, *client_socket);
				(variables->modConectado)[ident]=true; //para el check sobre quienes se conectaron
				int resultPositive=1;
				send(*client_socket, &resultPositive, sizeof(int), 0); //envío respuesta positiva 1
				switch(ident){
					case CPU:
						socketCPU=*client_socket;
						free(client_socket);
						log_info(logger, "Se recibió a CPU en el socket %d", socketCPU);
						break;
					case FileSystem:
						socketFS=*client_socket;
						free(client_socket);
						log_info(logger, "Se recibió a FileSystem en el socket %d", socketFS);
						break;
					case Kernel:
						socketKer=*client_socket;
						free(client_socket);
						log_info(logger, "Se recibió a Kernel en el socket %d", socketKer);
						break;
					default:
						log_warning(logger, "Se conectó un modulo no esperado. Enviando -1 y cerrando el socket %d", *client_socket);
						modulo idDesconocido=deleteConnectionFromList(variables->connectedSockets, *client_socket);
						variables->modConectado[idDesconocido]=false;
						int resultError=-1;
						send(*client_socket, &resultError, sizeof(int), 0); //enviando -1 como respuesta negativa de handshake
						close(*client_socket);
						free(client_socket);
						break;
				}
				break;
			case -1:
				log_error(logger, "Mensaje corrupto. Enviando -1 y cerrando conexion del socket %d", *client_socket);
				int resultError=-1;
				send(*client_socket, &resultError, sizeof(int), 0); //envio -1 como respuesta de error
				close(*client_socket);
				break;
			default:
				log_warning(logger,"Operacion desconocida. CodOp no reconocido");
				break;
		}
	}
	pthread_cancel(hiloRecepcionista); //Cierra violentamente al hiloRecepcionista, que estará bloqueado en accept
	pthread_exit(NULL);
}

void esperarConexiones(bool* modConectado, t_list* connectedSockets, int socket_mem){
	listaWaitingSockets=list_create();
	sem_init(&faltanConexiones, 0, 1); //así arranca hiloRecepcionista
	sem_init(&hayConexion, 0, 0);

	mCIVariables variables ={connectedSockets, modConectado};
	pthread_create(&hiloRecepcionista, NULL, (void*)recibirConexionesIniciales, (void*)&socket_mem);
	pthread_create(&hiloAtendedor, NULL, (void*)manejarConexionesIniciales, (void*)&variables);

	// esperar a que terminen los hilos
	pthread_join(hiloRecepcionista, NULL);
	pthread_join(hiloAtendedor, NULL);

	log_info(logger, "CPU, FileSystem y Kernel conectados correctamente. Iniciando estructuras administrativas");
}

//petición de escribir valor en la direccion addr
// [ CODOP - BUFFSIZE - DIRECCION FISICA - TAMAÑO VALOR - VALOR]
void* recv_movout_petition(int socket_escucha, int *addr, int *valorSize){
	int buffSize;
	int desplazamiento=0;
	void* buffer = recibir_buffer(&buffSize, socket_escucha);

	memcpy((int*)addr, buffer, sizeof(int));
	desplazamiento+=sizeof(int);
	memcpy((int*)valorSize, buffer+desplazamiento, sizeof(int));
	desplazamiento+=sizeof(int);
	void* valor=malloc(*valorSize);
	memcpy(valor, buffer + desplazamiento, *valorSize);
	desplazamiento+=*valorSize;

	free(buffer);
	return valor;
}

// [ CODOP - BUFFSIZE - DIRECCION FISICA - bytes]
void recv_movin_petition(int socket_escucha, int *addr, int *bytes){
	int buffSize;
	int desplazamiento=0;
	void* buffer = recibir_buffer(&buffSize, socket_escucha);

	memcpy(addr, buffer, sizeof(int));
	desplazamiento+=sizeof(int);
	memcpy(bytes, buffer + desplazamiento, sizeof(int));
	desplazamiento+=sizeof(int);

	free(buffer);
}

void memory_delay(int tiempo_milisegundos){
    struct timespec duration;
    duration.tv_sec = tiempo_milisegundos/1000; // segundo
    duration.tv_nsec = tiempo_milisegundos*1000000; // nanosegundos
    nanosleep(&duration, NULL);
}

void esperarCPU(){

	log_info(logger, "Memoria recibiendo peticiones de CPU.");
	while(1){
	//Tengo que recibir paquete y cod_op! No solo uno! All of paquete! jaja!
	int cod_op = recibir_operacion(socketCPU);
	if(cod_op==(-1)){
		log_error(logger, "Comunicación con CPU perturbada");
		exit(1);
	}
	int dir_fisica;
	switch(cod_op){
	case MOV_IN:
		int bytes; //cant bytes a leeer
		recv_movin_petition(socketCPU, &dir_fisica, &bytes);
		log_info(logger, "LEER - Dirección física: <%d> - Tamaño: <%d> - Origen: <CPU>", dir_fisica, bytes);
		void*retVal=malloc(bytes);
		sem_wait(&user_space_isFree);
		memcpy(retVal, user_space+dir_fisica, bytes);
		memory_delay(configData.RETARDO_MEMORIA);
		sem_post(&user_space_isFree);
		send(socketCPU, retVal, bytes, 0);
		free(retVal);
		break;
	case MOV_OUT:
		int valueSize;
		void* value=recv_movout_petition(socketCPU, &dir_fisica, &valueSize);
		sem_wait(&user_space_isFree);
		memcpy(user_space+dir_fisica, value, valueSize);
		memory_delay(configData.RETARDO_MEMORIA);
		sem_post(&user_space_isFree);
		log_info(logger, "ESCRIBIR - Dirección física: <%d> - Tamaño: <%d> - Origen: <CPU>", dir_fisica, valueSize);
		int rta=1;
		send(socketCPU, &rta, sizeof(int), 0);
		free(value);
		break;

	}

	}
}

void esperarFileSystem(){

	log_info(logger, "Memoria recibiendo peticiones de FileSystem.");
	while(1){
	//Tengo que recibir paquete y cod_op! No solo uno! All of paquete! jaja!
	int cod_op = recibir_operacion(socketFS);
	if(cod_op==(-1)){
		log_error(logger, "Comunicación con FileSystem perturbada");
		exit(1);
	}
	int dir_fisica;
	switch(cod_op){
	case F_WRITE:
		//cuando filesystem tiene que escribir, memoria lee lo que se va a escribir
		int bytes; //cant bytes a leeer
		recv_movin_petition(socketFS, &dir_fisica, &bytes);
		log_info(logger, "LEER - Dirección física: <%d> - Tamaño: <%d> - Origen: <FS>", dir_fisica, bytes);
		void*retVal=malloc(bytes);
		sem_wait(&user_space_isFree);
		memcpy(retVal, user_space+dir_fisica, bytes);
		memory_delay(configData.RETARDO_MEMORIA);
		sem_post(&user_space_isFree);
		send(socketFS, retVal, bytes, 0);
		break;
	case F_READ:
		//cuando FS tiene que leer, memoria tiene que escribir lo que lee
		int valueSize;
		void* value=recv_movout_petition(socketFS, &dir_fisica, &valueSize);
		sem_wait(&user_space_isFree);
		memcpy(user_space+dir_fisica, value, valueSize);
		memory_delay(configData.RETARDO_MEMORIA);
		sem_post(&user_space_isFree);
		log_info(logger, "ESCRIBIR - Dirección física: <%d> - Tamaño: <%d> - Origen: <FS>", dir_fisica, valueSize);
		int rta=1;
		send(socketFS, &rta, sizeof(int), 0);
		free(value);
		break;

	}

	}
}

//DEBERIA SER INT
void create_segment(char* PID, int segID, int size){
	t_list*tabla=dictionary_get(diccionario_tablas, PID);
	if(tabla==NULL){
		//ES LA PRIMERA VEZ QUE PROCESO SOLICITA CREATE
		t_list*nuevaTablaSegmentos=list_duplicate(tabla_inicial);
		int checkout=send_tabla_segmentos(socketKer, nuevaTablaSegmentos, 0);
		if(checkout>0){
			log_info(logger, "Creación de proceso PID: <%s>", PID);
		}else{
			log_error(logger, "Error enviando la tabla inicial");
		}
		dictionary_put(diccionario_tablas, PID, nuevaTablaSegmentos);
	}else{
		//EL PROCESO QUE SOLICITA CREATE YA SE CONECTÓ ANTES
		log_info(logger, "PID: <%s> - Crear segmento <%d> - Tamaño <%d>", PID, segID, size);
		int callocRetVal= calloc_segment(tabla, segID, size); //intento crear y alojar nuevo segmento
		//callocRetVal es 0 si se pudo crear y alojar nuevo segmento
				// es 1 si no se pudo crear pero el espacio suficiente existe fragmentado
			// es -1 si no se pudo crear y no existe espacio suficiente ni siquiera fragmentado
		int checkout=send_tabla_segmentos(socketKer, tabla, callocRetVal);
		if(checkout<=0){
			log_error(logger, "Error enviando la tabla actualizada");
		}else{
			switch(callocRetVal){
			case 0:
				log_info(logger, "PID: <%s> - Devuelta tabla actualizada - Nuevo Segmento ID: %d", PID, segID);
				break;
			case 1:
				log_warning(logger, "PID: <%s> - Devuelta tabla sin modificar - Compactación requerida", PID);
				break;
			case (-1):
				log_warning(logger, "PID: <%s> - Devuelta tabla sin modificar - Sin espacio disponible", PID);
			}

		}
	}
}

void delete_segment(char* PID, int segID){
	bool closure(Segmento* a){
		return a->ID==segID;
	}

	t_list*process_table=dictionary_get(diccionario_tablas, PID);
	if(process_table==NULL){
		log_error(logger, "No se encontró la tabla del proceso %s", PID);
		return;
	}		//elimino el segmento de la tabla del proceso que hizo la petición
	Segmento* segmento_a_eliminar=list_remove_by_condition(process_table, (void*)closure);
	log_info(logger, "PID: <%s> - Eliminar segmento <%d> - Base <%d> - Tamaño <%d>", PID, segID, segmento_a_eliminar->base, segmento_a_eliminar->limite);
	send_tabla_segmentos(socketKer, process_table, 0);

	//ahora necesito actualizar el cambio en la tabla de huecos vacíos
	int base_nuevo_hueco=segmento_a_eliminar->base;
	int limite_nuevo_hueco=segmento_a_eliminar->limite;

	alojar_nuevo_hueco(base_nuevo_hueco, limite_nuevo_hueco);
	free(segmento_a_eliminar);
}

void compactar_segmentos();

void esperarKernel(){

	log_info(logger, "Memoria recibiendo peticiones de Kernel.");
	while(1){
	int PID, segID, size;
	char* charPID; //token para el diccionario de tablas de segmentos
	int cod_op = recibir_operacion(socketKer);
	if(cod_op==(-1)){
		log_error(logger, "Comunicación con Kernel perturbada");
		exit(1);
	}
	switch(cod_op){
	case CREATE_SEGMENT:
		recv_newsegment_petition(socketKer, &PID, &segID, &size);
		charPID=string_itoa(PID);
		create_segment(charPID, segID, size);
		free(charPID);
		break;
	case DELETE_SEGMENT:
		recv_deletesegment_petition(socketKer, &PID, &segID);
		charPID=string_itoa(PID);
		delete_segment(charPID, segID);
		free(charPID);
		break;
	case EXIT:
		charPID=recibir_mensaje(socketKer);
		log_info(logger, "Eliminando segmentos asociados a pID: %s", charPID);
		t_list*tabla_segmentos=dictionary_remove(diccionario_tablas, charPID);
		int cantSegmentos=list_size(tabla_segmentos);
		for(int i=1; i<cantSegmentos; i++){
			Segmento* segmento_a_eliminar=list_remove(tabla_segmentos, 1);

			//ahora necesito actualizar el cambio en la tabla de huecos vacíos
			int base_nuevo_hueco=segmento_a_eliminar->base;
			int limite_nuevo_hueco=segmento_a_eliminar->limite;
			alojar_nuevo_hueco(base_nuevo_hueco, limite_nuevo_hueco);
			free(segmento_a_eliminar);
		}list_destroy(tabla_segmentos);
		log_info(logger, "PID: %s - ELIMINADA TABLA DE SEGMENTOS", charPID);
		break;
	case COMPACTION:
		log_warning(logger, "Se recibió solicitud de compactación.");
		//realizar compactación de memoria
		compactar_segmentos();
		//int answer=1;
		//send(socketKer, &answer, sizeof(int), 0);
		break;
	default:
		log_error(logger, "Codigo de operación de kernel no reconocido. Finalizando memoria.");
		exit(1);
	}
	}
}

//asume que el segmento y el hueco son adyacentes, y el hueco está a la izquierda
void realloc_segment(Segmento* segmento_a_desplazar, Hueco* hueco_a_utilizar){
	list_remove_element(huecos_libres, hueco_a_utilizar); //lo quito de la lista para realojarlo
	//desplazo segmento
	segmento_a_desplazar->base=hueco_a_utilizar->pos;
	//alojo el nuevo hueco formado por el desplazamiento del segmento
	int pos_nuevo_hueco=segmento_a_desplazar->base+segmento_a_desplazar->limite; //fin del segmento desplazado
	int limite_nuevo_hueco=hueco_a_utilizar->size; //cantidad de posiciones desplazado
	free(hueco_a_utilizar);
	alojar_nuevo_hueco(pos_nuevo_hueco, limite_nuevo_hueco);
	return;
}

void enviar_tablas_actualizadas();

void compactar_segmentos(){
	Hueco* menor_base_H(Hueco*a, Hueco*b){
		return a->pos<=b->pos?a:b;
	}
	Hueco* primer_hueco; //se inicializará en cada iteración
	bool base_mayor_a_hueco(Segmento*a){
		return (a->base>primer_hueco->pos);
	}
	Segmento* menor_base_S(Segmento*a, Segmento*b){
		return a->base<=b->base?a:b;
	}
	//crear tabla global de todos los segmentos de todas las tablas
	t_list* lista_de_tablas=dictionary_elements(diccionario_tablas);
	t_list* tabla_global_segmentos=list_flatten(lista_de_tablas);

	//repito realojamiento por una vez por cada hueco
	while(1){
		//el primer hueco es el de menor base
		primer_hueco=list_get_minimum(huecos_libres, (void*)menor_base_H);
		//lista auxiliar de segmentos que están por delante del hueco encontrado
			t_list* segmentos_utiles=list_filter(tabla_global_segmentos, (void*)base_mayor_a_hueco);
		if(list_is_empty(segmentos_utiles)){
			break; //no hay mas segmentos a desplazar
		}
		//finalmente, el primer segmento por delante del hueco.
		Segmento* sgm_a_desplazar=list_get_minimum(segmentos_utiles, (void*)menor_base_S);
		//muevo el contenido del bloque que le corresponde al segmento en espacio de usuario
		memmove(user_space+primer_hueco->pos,user_space+sgm_a_desplazar->base, sgm_a_desplazar->limite);
		memset(user_space+sgm_a_desplazar->base, ' ', sgm_a_desplazar->limite);
		//actualizo estructuras administrativas
		realloc_segment(sgm_a_desplazar, primer_hueco);
	}
	//Recordemos, compactación tiene delay por configuración muchachos jaja
	memory_delay(configData.RETARDO_COMPACTACION);
	//por último, envío las listas en el formato de abajo
	enviar_tablas_actualizadas();
	log_debug(logger, "Fin de compactación");
	//	 [ PAQUETE - SIZEBUFF - CANT.TABLAS - PID SIZE LISTA - PID SIZE LISTA - PID SIZE LISTA ... ]

}

void loggear_tabla_actualizada(t_list* tabla_segmentos, int PID){
	int cant_segmentos=list_size(tabla_segmentos);
	for(int i=0;i<cant_segmentos;i++){
		Segmento*un_segmento=list_get(tabla_segmentos, i);
		log_info(logger, "PID: <%d> - Segmento: <%d> - Base: <%d> - Tamaño <%d>", PID, un_segmento->ID, un_segmento->base, un_segmento->limite);
	}
}

void enviar_tablas_actualizadas(){
	t_list* lista_claves=dictionary_keys(diccionario_tablas);
	int cant_tablas=dictionary_size(diccionario_tablas);
	t_paquete* paquete = crear_paquete();
	paquete->buffer->stream = realloc(paquete->buffer->stream, paquete->buffer->size + sizeof(int));
	memcpy(paquete->buffer->stream+paquete->buffer->size, &cant_tablas, sizeof(int));
	paquete->buffer->size += sizeof(int);
	for(int i=0; i<cant_tablas; i++){
		char* una_clave=(char*)list_get(lista_claves, i);
		int esa_clave_pero_numerica=atoi(una_clave);//es el PID
		t_list* una_tabla=dictionary_get(diccionario_tablas, una_clave);
		loggear_tabla_actualizada(una_tabla, esa_clave_pero_numerica);
		paquete->buffer->stream = realloc(paquete->buffer->stream, paquete->buffer->size + sizeof(int));
		memcpy(paquete->buffer->stream+paquete->buffer->size, &esa_clave_pero_numerica, sizeof(int));
		paquete->buffer->size += sizeof(int);
		empaquetar_lista_segmentos(paquete, una_tabla);
	}
	enviar_paquete(paquete, socketKer);
	eliminar_paquete(paquete);
}






#endif
