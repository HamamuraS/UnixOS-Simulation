#include "../../ganSOsLibrary/src/header.h"
//#include <pthread.h>

void gestionadorRunning(void); //Planificador corto plazo
void gestionarNuevasConexiones(void* socket); //Hilos receptores, agregan a NEW
void gestionadorReady(void); //Planificador largo plazo

typedef struct{

	char* IP_MEMORIA;
	char* PUERTO_MEMORIA;
	char* IP_FILESYSTEM;
	char* PUERTO_FILESYSTEM;
	char* IP_CPU;
	char* PUERTO_CPU;
	char* PUERTO_ESCUCHA;
	char* ALGORITMO_PLANIFICACION;
	long ESTIMACION_INICIAL;
	double HRRN_ALFA;
	int GRADO_MAX_MULTIPROGRAMACION;
	char  (**RECURSOS);
	char (**INSTANCIAS_RECURSOS);

}kerConfig;


t_list* GDT;

 typedef struct{

 	char* file_name;
 	int pointer;

}Descriptor;


sem_t configSem, mutexLogger;
kerConfig kerConfigData;

kerConfig liftConfig(t_config* config){

	kerConfig rev;

	rev.ALGORITMO_PLANIFICACION = config_get_string_value(config,"ALGORITMO_PLANIFICACION");
	rev.IP_CPU = config_get_string_value(config,"IP_CPU");
	rev.IP_FILESYSTEM = config_get_string_value(config,"IP_FILESYSTEM");
	rev.IP_MEMORIA = config_get_string_value(config,"IP_MEMORIA");
	rev.ESTIMACION_INICIAL =  config_get_long_value(config,"ESTIMACION_INICIAL");
	rev.GRADO_MAX_MULTIPROGRAMACION = config_get_int_value(config,"GRADO_MAX_MULTIPROGRAMACION");
	rev.PUERTO_CPU = config_get_string_value(config,"PUERTO_CPU");
	rev.PUERTO_ESCUCHA = config_get_string_value(config,"PUERTO_ESCUCHA");
	rev.PUERTO_FILESYSTEM = config_get_string_value(config,"PUERTO_FILESYSTEM");
	rev.PUERTO_MEMORIA = config_get_string_value(config,"PUERTO_MEMORIA");
	rev.HRRN_ALFA = config_get_double_value(config,"HRRN_ALFA");

	rev.RECURSOS = config_get_array_value(config,"RECURSOS");
	rev.INSTANCIAS_RECURSOS = config_get_array_value(config,"INSTANCIAS_RECURSOS");

	return rev;
}

typedef struct{
	int PID; //que sea el socket de conexion
	t_list* instrucciones;
	int program_counter;
	char* registros[CANTIDAD_REGISTROS_CPU];
	t_list* tablaSegmentos;
	double est_prox_rafaga;
	struct timespec tiempo_Llegada_Ready; //timestamp en segundos y en nanosegundo
	t_list* tablaArchivos;
}PCB;

typedef struct{
	PCB* un_pcb;
	t_list* parametros;
	int cod_op;
	bool impide_compactación;
}BlockedNode; //estructura para la cola de bloqueados en espera de utilizar file system

int socket_fs, socket_memoria, socket_cpu;
sem_t mutexMemoria;

sem_t mutexConsolasNew, mutexConsolasReady, mutexConsolasExit, mutexConsolasBlocked;
sem_t hayEspacioReady, hayConsolasNew, hayConsolasReady, hayConsolasExit, senialInicio, hayConsolasBlocked;

t_list* global_PCB_list;
t_list* consolasNew; //contiene PCBs de cantidad indeterminada de consolas conectadas
t_list* consolasReady; //contiene PCBs de cantidad de consolas igual o menor al grado maximo de multiprogramacion
t_list* consolasExit; //contiene PCBs de cantidad de ........ exit
t_list* consolasBlocked; //PCBs bloqueados. Falta inicializar, borrar esto cuando se inicializa.
t_dictionary* recursosDisponibles;
t_dictionary* recursosEnUso;
t_dictionary* recursos_por_proceso;
////////////////////////////////////////////////////////////////////////////////////////////////////



//inicializa lista de recursos y diccionario con colas de bloqueados de CADA RECURSO
void init_count_recursos(){//Toma config y forma lista con struct recursosDisponibles.


		//iterador de vectores para ambas.
	for(int k=0; k < string_array_size(kerConfigData.RECURSOS) ; k++){

			char* nombreRecurso =malloc(sizeof(kerConfigData.RECURSOS[k]));
			int* auxInstancia = malloc(sizeof(int));


			strcpy(nombreRecurso,kerConfigData.RECURSOS[k]);
			int aux= atoi(kerConfigData.INSTANCIAS_RECURSOS[k]);
			*auxInstancia = aux;
			dictionary_put(recursosDisponibles,nombreRecurso,auxInstancia);

	}


}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////

int send_handshake(int senderModule, int socketRecipiente){
	int handshake =1;
	send(socketRecipiente, &handshake, sizeof(int), 0);
	int respuesta;
	recv(socketRecipiente, &respuesta, sizeof(int), MSG_WAITALL);
	if(respuesta==(-1)){
		log_error(logger, "Comunicación con el módulo %d alterada. Abortando kernel", senderModule);
		close(socketRecipiente);
		exit(1);
	}
	return 1;
}

void recieve_handshake(int socket_cliente){

	int handshake;
	int resultOk = 1;
	int resultError = -1;

	recv(socket_cliente, &handshake, sizeof(int), MSG_WAITALL);
	if(handshake == 1){
	   send(socket_cliente, &resultOk, sizeof(int), 0);}
	else{
	   send(socket_cliente, &resultError, sizeof(int), 0);}

}
void string_list_cleaner(t_list* lista){
	void liberador(void* elem){
		free(elem);
	}
	list_clean_and_destroy_elements(lista, (void*)liberador);
	free(lista);
}
void recibir_tablas_compactadas(); //despues de solicitar compactación, recibe y actualiza todos los PCB

//Gestionador de new
void gestionarNuevasConexiones(void* socket){
	int* socket_consola=(int*) socket;
	recieve_handshake(*socket_consola);
	int cod_op = recibir_operacion(*socket_consola); // devuelve -1 si no se puede leer el paquete
	switch (cod_op) {
		case MENSAJE:
			log_error(logger, "Mensaje inesperado recibido");
			close(*socket_consola);
			break;
		case PAQUETE:
			PCB*newPCB=(PCB*)malloc(sizeof(PCB));
			t_list* paqueteConsola = recibir_paquete(*socket_consola); //lista donde cada char* esta en el heap
			newPCB->PID=*socket_consola;
			sem_wait(&configSem);
				newPCB->est_prox_rafaga=kerConfigData.ESTIMACION_INICIAL;
			sem_post(&configSem);
			newPCB->instrucciones=paqueteConsola;
			newPCB->program_counter=0;
			for(int i=0; i<CANTIDAD_REGISTROS_CPU; i++){
				newPCB->registros[i]=string_from_format("0000");
			}
			//aca van las inicializaciones faltantes de PCB: tabla de archivos abiertos, tabla de segmentos
			newPCB->tablaArchivos=list_create();
			sem_wait(&mutexConsolasNew);
				list_add(consolasNew, newPCB);
			sem_post(&mutexConsolasNew);
			sem_wait(&mutexLogger);
			log_info(logger, "Se crea al proceso <%d> en NEW.", *socket_consola);
			sem_post(&mutexLogger);
			sem_post(&hayConsolasNew);//habilito al gestionadorREady
			break;
		case -1:
			log_error(logger, "Mensaje corrupto. Cerrando conexion del proceso %d ", *socket_consola);
			int resultError=-1;
			send(*socket_consola, &resultError, sizeof(int), 0); //envio -1 como respuesta de error
			close(*socket_consola);
			break;
		default:
			log_warning(logger,"Operacion desconocida, codop %d", cod_op);
			break;
	}
}

char* actualizarStringColaReady(){
	int size = list_size(consolasReady);
	char* cadena=string_new();
	string_append(&cadena, "[");
	for(int i=0; i<size; i++){
		PCB* pcb = list_get(consolasReady, i);
		char* newCadena=string_from_format(" %d ", pcb->PID);
		string_append(&cadena, newCadena);
		free(newCadena);
	}
	string_append(&cadena, "]");
	return cadena;
}

t_list* pedirTablaSegmentosInicial(PCB* readyPCB){
	t_list* newTabla;					//no importa el ultimo parametro en este caso
	sem_wait(&mutexMemoria);
	send_newsegment_petition(socket_memoria, readyPCB->PID, 0, 0);
	sem_post(&mutexMemoria);
	int rVal; //return val no interesa en este caso
	sem_wait(&mutexMemoria);
	newTabla=recv_tabla_segmentos(socket_memoria, &rVal);
	sem_post(&mutexMemoria);
	return newTabla;
}

void gestionadorReady(void){ //planificador largo
	while(1){
		sem_wait(&hayConsolasNew);
		sem_wait(&hayEspacioReady);//se necesita esperar a que haya espacio en ready
			sem_wait(&mutexConsolasNew);
				PCB *readyPCB=(PCB*)list_remove(consolasNew, 0);
			sem_post(&mutexConsolasNew);
			clock_gettime(CLOCK_REALTIME, &(readyPCB->tiempo_Llegada_Ready));//carga el struct con tiempo en segundos y nanosegundos
			t_list* tablaInicial = pedirTablaSegmentosInicial(readyPCB);
			readyPCB->tablaSegmentos=tablaInicial;
			log_debug(logger, "Se recibio tabla inicial del PID: %d", readyPCB->PID);
			sem_wait(&mutexConsolasReady);
				list_add(consolasReady, readyPCB); //consolasReady es la lista con los PCB readies.
			sem_post(&mutexConsolasReady);
			list_add(global_PCB_list, readyPCB); //cola global con todos los pcbs en todos los estados
			char* stringReadyPID=actualizarStringColaReady();
			sem_wait(&mutexLogger);
			log_info(logger, "Cola Ready <%s>: %s", kerConfigData.ALGORITMO_PLANIFICACION, stringReadyPID);
			free(stringReadyPID);
			sem_post(&mutexLogger);
			sem_post(&hayConsolasReady); //habilito al gestionadorRunning
		//sem_post(&hayEspacioReady) se hará en gestionadorExit
	}
	pthread_exit(NULL);
}

struct timespec start;
void enviar_PCB(PCB *runningPCB){ //serializar
	//los recursos asignados aquí a contexto se liberan dentro de paquete_contexto
	execContext contexto;
	contexto.PID=runningPCB->PID;
	contexto.instrucciones=list_create();
	contexto.instrucciones= list_duplicate(runningPCB->instrucciones);
	contexto.program_counter=runningPCB->program_counter;
	for(int i=0;i<CANTIDAD_REGISTROS_CPU;i++){
		contexto.registros[i]=string_new();
		string_append(&contexto.registros[i], runningPCB->registros[i]);
	}
	contexto.parametros=list_create();
	contexto.tablaSegmentos=list_duplicate(runningPCB->tablaSegmentos);
	//AGREGAR OTROS ELEMENTOS DEL CONTEXTO, tabla archivos abiertos? tabla segmentos?
	int checkVal=paquete_contexto(socket_cpu, contexto ,RUNNING);
	clock_gettime(CLOCK_REALTIME, &start);

	if(checkVal==-1){
		perror("Error al enviar el contexto de ejecución:");
		exit(1);
	}
}
struct timespec end;
int recibir_PCB(PCB *runningPCB, t_list* parametros){ //actualiza PCB en ejecucion.

	void liberador(void* elem){
		free(elem);
	} //liberador para liberar cada malloc de la lista de instrucciones, no es necesario guardarlas. Estan en PCB
	execContext contextoDevuelto;
	int cod_op = recibir_operacion(socket_cpu);
	contextoDevuelto=recibir_paquete_contexto(socket_cpu);
	clock_gettime(CLOCK_REALTIME, &end);

	for(int i=0; i<list_size(contextoDevuelto.parametros); i++){
		list_add(parametros, list_get(contextoDevuelto.parametros, i));
	}
	runningPCB->program_counter=contextoDevuelto.program_counter; //actualizando PC de PCB
	for(int i=0;i<CANTIDAD_REGISTROS_CPU;i++){ //actualizando los registros de CPU del PCB
		strcpy(runningPCB->registros[i], contextoDevuelto.registros[i]);
		free(contextoDevuelto.registros[i]);
	}
    list_destroy_and_destroy_elements(runningPCB->tablaSegmentos, (void*)liberador);
	runningPCB->tablaSegmentos=contextoDevuelto.tablaSegmentos;
	return cod_op;
}

void registrarRecursosDelProceso(char* recurso, PCB* PCBrunning){
	char* pIDchar=string_itoa(PCBrunning->PID);
	//busco la lista de recursos asignados del proceso particular
	t_list* recursos_del_proceso=dictionary_get(recursos_por_proceso, pIDchar);
	if(recursos_del_proceso==NULL){
		recursos_del_proceso=list_create();
	}
	char* backupRecurso=(char*)malloc(sizeof(recurso)+1);
	memcpy(backupRecurso, recurso, sizeof(recurso)+1);
	list_add(recursos_del_proceso, backupRecurso);
	//cuidado, memory leak al hacer esto en caso de que ya existiese la lista ->
	dictionary_put(recursos_por_proceso, pIDchar, (void*)recursos_del_proceso);
}


bool replanificar; //flag que se setea en true cuando es necesario replanificar despues de syscall

void wait_recurso_lista(char* recurso, PCB* PCBrunning){

	if(dictionary_has_key(recursosDisponibles,recurso)){//el key existe!

		//en cualquiera de los casos, el proceso PCBrunning accedera al recurso en algún momento.
		registrarRecursosDelProceso(recurso, PCBrunning);

		int* instancias = dictionary_get(recursosDisponibles, recurso);

		if(*instancias >= 1){//hay suficiente instancias

			(*instancias)--;
			//dictionary_put(recursosDisponibles,recurso,instancias);
			log_info(logger, "PID: <%d> - Wait: <%s> - Instancias: <%d>", PCBrunning->PID, recurso, *instancias);
			sem_wait(&mutexConsolasReady);
			//RECORDAR: WAIT NO DEBE REPLANIFICAR COLA READY, VUELVE A EJECUTAR MISMO PROCESO
			list_add_in_index(consolasReady, 0, PCBrunning); //agrego adelante para que vuelva a ejecutar
			sem_post(&mutexConsolasReady);
			sem_post(&hayConsolasReady);

		}else{//no hay suficiente instancias

			if(dictionary_has_key(recursosEnUso,recurso)){//colocar el PCB en el diccionario de los bloqueados.

						t_list* listAux = dictionary_get(recursosEnUso, recurso);
						list_add(listAux,PCBrunning);
						log_warning(logger, "PID: <%d> - Bloqueado por: <%s>", PCBrunning->PID, recurso);
						//dictionary_put(recursosEnUso, recurso, listAux); creo que esta linea no va

					}else{

						t_list* listaRecursos = list_create();
						list_add(listaRecursos,PCBrunning);
						log_warning(logger, "PID: <%d> - Bloqueado por: <%s>", PCBrunning->PID, recurso);
						dictionary_put(recursosEnUso, recurso, listaRecursos);
					}

		}



	}else{//KEY NO EXISTE
		char* mensaje_error=string_from_format("WAIT: %s inexistente", recurso);
		log_warning(logger, "Finaliza el proceso <%d> - Motivo: <%s>", PCBrunning->PID, mensaje_error);
		//Enviando codigo de error a consola
		int mensajeFin=ERROR;
		send(PCBrunning->PID, &mensajeFin, sizeof(int), 0);
		//Enviando mensaje de error a imprimir en modulo Consola
		enviar_mensaje(mensaje_error, PCBrunning->PID);
		free(mensaje_error);
		//Liberar recursos del proceso PCBrunning
		sem_wait(&mutexConsolasExit);
			list_add(consolasExit, PCBrunning);
		sem_post(&mutexConsolasExit);
		sem_post(&hayConsolasExit);
	}

}

void quitarRecursoDelProceso(char* recurso, PCB* PCBrunning){
	bool es_el_recurso(char* a){
		return strcmp(a, recurso)==0?true:false;
	}
	char* pIDchar=string_itoa(PCBrunning->PID);
	//busco la lista de recursos asignados del proceso particular
	t_list* recursos_del_proceso=dictionary_get(recursos_por_proceso, pIDchar);
	//y le quito el recurso
	char*instancia_del_recurso=list_find(recursos_del_proceso, (void*)es_el_recurso);
	list_remove_element(recursos_del_proceso, instancia_del_recurso);
	free(instancia_del_recurso);
}

void liberar_recurso_y_actualizar_ready(char* recurso, int runningPID){
	if(dictionary_has_key(recursosEnUso,recurso)){//si hay otros PCBs que requiere el mismo recurso
		//debo quitar el recurso de la lista de recursos asignados de PCBrunning
		int* instanciaRec = dictionary_get(recursosDisponibles, recurso);
		t_list* listAux = dictionary_get(recursosEnUso, recurso);

		PCB* PCBunlocked = (PCB*)list_remove(listAux,0);
		clock_gettime(CLOCK_REALTIME, &PCBunlocked->tiempo_Llegada_Ready);
		sem_wait(&mutexConsolasReady);
			list_add(consolasReady,PCBunlocked);
		sem_post(&mutexConsolasReady);
		sem_post(&hayConsolasReady);
		log_info(logger, "PID: <%d> - Signal: <%s> - Instancias: <%d>",runningPID, recurso, *instanciaRec);
		log_warning(logger, "PID: <%d> - Estado Anterior: <BLOQUEADO> - Estado Actual: <READY>", PCBunlocked->PID);
		if(list_size(listAux)==0){
			dictionary_remove(recursosEnUso, recurso);
			list_destroy(listAux);
		}
	}else{//Si no hay PCB que requiere el recurso.

		int* instanciaRec = dictionary_get(recursosDisponibles, recurso);//ir a diccionario de recursos disponibles.
		(*instanciaRec)++;
		dictionary_put(recursosDisponibles, recurso, instanciaRec);

		log_info(logger, "PID: <%d> - Signal: <%s> - Instancias: <%d>",runningPID, recurso, *instanciaRec);
	}
}

void signal_recurso_lista(char* recurso, PCB* PCBrunning){ //Sacar del pcb de la cola del diccionario y agregar otra a la cola disponible recursos.

	if(dictionary_has_key(recursosDisponibles,recurso)){

		quitarRecursoDelProceso(recurso, PCBrunning);
		sem_wait(&mutexConsolasReady);
			list_add_in_index(consolasReady, 0, PCBrunning);
		sem_post(&mutexConsolasReady);
		sem_post(&hayConsolasReady);

		liberar_recurso_y_actualizar_ready(recurso, PCBrunning->PID);
	}else{
		char* mensaje_error=string_from_format("SIGNAL: %s inexistente", recurso);
		log_warning(logger, "Finaliza el proceso <%d> - Motivo: <%s>", PCBrunning->PID, mensaje_error);
		//Enviando codigo de error a consola
		int mensajeFin=ERROR;
		send(PCBrunning->PID, &mensajeFin, sizeof(int), 0);
		//Enviando mensaje de error a imprimir en modulo Consola
		enviar_mensaje(mensaje_error, PCBrunning->PID);
		free(mensaje_error);
		//Liberar recursos del proceso PCBrunning
		sem_wait(&mutexConsolasExit);
			list_add(consolasExit, PCBrunning);
		sem_post(&mutexConsolasExit);
		sem_post(&hayConsolasExit);
	}

}

typedef struct{
	PCB*runningPCB;
	int time;
}retenerPCBvar;

void retenerPCB(void* buff){
	retenerPCBvar *variables = (retenerPCBvar*)buff;
	struct timespec ts;
	ts.tv_sec = variables->time;
	ts.tv_nsec = 0; //el tiempo viene en segundos
	nanosleep(&ts, NULL);//duerme, reteniendo al PCB durante 'time' milisegundos
	clock_gettime(CLOCK_REALTIME, &variables->runningPCB->tiempo_Llegada_Ready);
	sem_wait(&mutexConsolasReady);
		list_add(consolasReady, variables->runningPCB);
		log_warning(logger, "PID: <%d> - Estado Anterior: <BLOQUEADO> - Estado Actual: <READY>",(variables->runningPCB)->PID);
	sem_post(&mutexConsolasReady);
	sem_post(&hayConsolasReady);
	free(variables);
}

void i_o(char* tiempoBloqueoC, PCB *runningPCB){
	int time=strtol(tiempoBloqueoC, NULL, 10);
	pthread_t threadBloqueadorIO;
	retenerPCBvar *variables=(retenerPCBvar*)malloc(sizeof(retenerPCBvar));
	variables->runningPCB=runningPCB;
	variables->time=time;
	log_warning(logger, "PID: <%d> - Bloqueado por: <IO>", runningPCB->PID);
	pthread_create(&threadBloqueadorIO, NULL, (void*)retenerPCB, (void*)variables);
	pthread_detach(threadBloqueadorIO);
}

void compaction_manager();

void create_segment(PCB* readyPCB, int ID, int size){
	void SegmentDestroyer(Segmento* a){
		free(a);
	}
	//manda id y size.
	//recibe handshake estado.
	//manda al cpu contexto de vuelta.

	//en caso de fallo mandar error
	int flag;
	sem_wait(&mutexMemoria);
	send_newsegment_petition(socket_memoria,readyPCB->PID,ID,size);
	sem_post(&mutexMemoria);
    list_destroy_and_destroy_elements(readyPCB->tablaSegmentos, (void*)SegmentDestroyer);
    sem_wait(&mutexMemoria);
    readyPCB->tablaSegmentos = recv_tabla_segmentos(socket_memoria,&flag);
    sem_post(&mutexMemoria);

    switch(flag){ //Agregar al Case en el futuro si es necesario. Por ahora todas las opcioens son similares.

    case 1:
    	// Hay que solicitar compactación a memoria
    	compaction_manager();
    	create_segment(readyPCB,ID,size);
    	break;
    case 0:
    	log_info(logger, "PID: <%d> - Crear Segmento - Id: <%d> - Tamaño: <%d>", readyPCB->PID, ID, size);
		sem_wait(&mutexConsolasReady);
			list_add_in_index(consolasReady, 0, readyPCB); //agrego adelante para que vuelva a ejecutar
		sem_post(&mutexConsolasReady);
		sem_post(&hayConsolasReady);
    	break;
    case -1:
		char* mensaje_error=string_from_format("Out of Memory");
		log_warning(logger, "Finaliza el proceso <%d> - Motivo: <%s>", readyPCB->PID, mensaje_error);
		int mensajeFin=ERROR;
		send(readyPCB->PID, &mensajeFin, sizeof(int), 0);
		enviar_mensaje(mensaje_error, readyPCB->PID);
		replanificar=true;
		sem_wait(&mutexConsolasExit);
			list_add(consolasExit, readyPCB);
		sem_post(&mutexConsolasExit);
		sem_post(&hayConsolasExit);
    	break;
    default:
    	log_error(logger, "Ha recibido respuesta incorrecta.");
    }


}

void delete_segment(PCB* readyPCB, int ID){
	void SegmentDestroyer(Segmento* a){
		free(a);
	}
	sem_wait(&mutexMemoria);
	send_deletesegment_petition(socket_memoria, readyPCB->PID, ID); //manda id
	sem_post(&mutexMemoria);
	list_destroy_and_destroy_elements(readyPCB->tablaSegmentos, (void*)SegmentDestroyer);
	log_info(logger, "PID: <%d> - Eliminar Segmento - Id Segmento: <%d>", readyPCB->PID, ID);
	int flag;
	sem_wait(&mutexMemoria);
	  readyPCB->tablaSegmentos = recv_tabla_segmentos(socket_memoria,&flag);
	  sem_post(&mutexMemoria);
	//recibe tabla

}

int consulting_file_system(int socket_dest, char* fileName){//FLAG ES 0 SI ES OPEN, 1 SI ES CLOSE. 2 Create.

	t_paquete* paquete= crear_paquete_contexto(F_OPEN);

	int fileSize = strlen(fileName)+1;

	paquete->buffer->stream = realloc(paquete->buffer->stream, paquete->buffer->size +sizeof(int) + fileSize);
	memcpy(paquete->buffer->stream, &fileSize, sizeof(int));//proceso
	paquete->buffer->size += sizeof(int);
	memcpy(paquete->buffer->stream + paquete->buffer->size, fileName, fileSize);//nombre archivo en char*

	paquete->buffer->size += fileSize;
	int bytes_enviados=enviar_paquete(paquete, socket_dest);//+CodOp paquete y size
	eliminar_paquete(paquete);

	//If you need to send a file_create petition, open this.
	int answer;
	recv(socket_dest, &answer, sizeof(int), MSG_WAITALL);

	if(answer != 1){
		log_error(logger, "No se pudo abrir el archivo");
		exit(1);
	}

	return bytes_enviados;

}

t_dictionary* diccionario_colas_archivos;

//recibe el descriptor del archivo que se cerro y actualiza estructuras administrativas
void liberar_archivo(Descriptor* descr_del_proceso){
	t_list* cola_del_archivo=dictionary_get(diccionario_colas_archivos, descr_del_proceso->file_name);
	//si la cola del archivo está vacía, entonces nadie mas necesita el archivo.
	if(list_is_empty(cola_del_archivo)){
		//lo remuevo de la GDT
		list_remove_element(GDT, descr_del_proceso);
		//tambien libero la memoria del diccionario y el descriptor
		dictionary_remove(diccionario_colas_archivos, descr_del_proceso->file_name);
		list_destroy(cola_del_archivo);
		free(descr_del_proceso->file_name);
		free(descr_del_proceso);
	}else{
		//se debe desbloquear a un nuevo proceso y que ejecute primero
		PCB* un_PCB=list_remove(cola_del_archivo, 0);
		replanificar=false;
		sem_wait(&mutexConsolasReady);
			clock_gettime(CLOCK_REALTIME, &un_PCB->tiempo_Llegada_Ready);
			list_add(consolasReady, un_PCB);
		sem_post(&mutexConsolasReady);
		sem_post(&hayConsolasReady);
	}
}
double est_prox_rafaga_backup;

void process_f_close(t_list *parametros, PCB * runningPCB){
	char* name = (char*)list_get(parametros,0);
	bool descriptor_finder(Descriptor* aux){
		return strcmp(aux->file_name,name)==0?true:false;
	}
	Descriptor* descr_del_proceso=list_find(runningPCB->tablaArchivos, (void*)descriptor_finder);

	if(descr_del_proceso==NULL){
		char* mensaje_error=string_from_format("F_CLOSE: %s no abierto", name);
		log_warning(logger, "Finaliza el proceso <%d> - Motivo: <%s>", runningPCB->PID, mensaje_error);
		int mensajeFin=ERROR;
		send(runningPCB->PID, &mensajeFin, sizeof(int), 0);
		enviar_mensaje(mensaje_error, runningPCB->PID);
		replanificar=true;
		sem_wait(&mutexConsolasExit);
			list_add(consolasExit, runningPCB);
		sem_post(&mutexConsolasExit);
		sem_post(&hayConsolasExit);
		return;
	}
	//lo remuevo de la tabla del proceso y este vuelve a ejecutar
	list_remove_element(runningPCB->tablaArchivos, descr_del_proceso);
	sem_wait(&mutexConsolasReady);
		log_info(logger, "PID: <%d> - Cerrar Archivo: <%s>", runningPCB->PID, name);
		clock_gettime(CLOCK_REALTIME, &runningPCB->tiempo_Llegada_Ready);
		//valor arbitrario pequeño para que ejecute primero
		est_prox_rafaga_backup=runningPCB->est_prox_rafaga;
		runningPCB->est_prox_rafaga=0.01;
		log_warning(logger, "PID: <%d> - Estado Anterior: <BLOQUEADO> - Estado Actual: <READY>", runningPCB->PID);
		list_add(consolasReady, runningPCB);
	sem_post(&mutexConsolasReady);
	sem_post(&hayConsolasReady);

	//hay mas procesos esperando por este archivo? se asigna a nuevo PCB de ser necesario
	//sino, se elimina el descriptor del GDT
	liberar_archivo(descr_del_proceso);
}


void process_f_open(t_list* parametros, PCB *runningPCB){

	char* name = (char*)list_get(parametros,0);
	log_info(logger, "PID: <%d> - Abrir Archivo: <%s>", runningPCB->PID,name);

	bool descriptor_finder(Descriptor* aux){

		int found = strcmp(aux->file_name,name);

		if(found==0){
			return true;
		}

		return false;
	};

	Descriptor* arch_found = list_find(GDT,(void*)descriptor_finder);


	if(arch_found==NULL){
		//nueva cola vacía para el nuevo archivo creado/abierto
		t_list* nueva_cola_de_archivo=list_create();
		dictionary_put(diccionario_colas_archivos, name, nueva_cola_de_archivo);

		Descriptor* arch_new =(Descriptor*)malloc(sizeof(Descriptor));
		arch_new->file_name=string_duplicate(name);
		arch_new->pointer=0;
		consulting_file_system(socket_fs,name);
			//POR QUE ENTRAN MUTEXCONSOASBLOCKED PARA EL PROCESO  EN 717? ES ESTO UN ERROR?

				//creo que vamos a necesitar mutex para GDT
				list_add(GDT, arch_new);//EN AMBOS CASOS : entrada a la tabla globla de archivos abiertos
				list_add_in_index(runningPCB->tablaArchivos,0,arch_new);//y a la tabla de archivos abiertos procesos posicion 0.

				sem_wait(&mutexConsolasReady);

				//RECORDAR: WAIT NO DEBE REPLANIFICAR COLA READY, VUELVE A EJECUTAR MISMO PROCESO
				//le miento al algoritmo con un estimado de proxima rafaga falso jeje
				//muy pequeño arbitrario, asi ejecuta primero
				est_prox_rafaga_backup=runningPCB->est_prox_rafaga;
				runningPCB->est_prox_rafaga=0.01;
				clock_gettime(CLOCK_REALTIME, &runningPCB->tiempo_Llegada_Ready);
				list_add_in_index(consolasReady, 0, runningPCB); //agrego adelante para que vuelva a ejecutar
				log_warning(logger, "PID: <%d> - Estado Anterior: <BLOQUEADO> - Estado Actual: <READY>", runningPCB->PID);
				sem_post(&mutexConsolasReady);
				sem_post(&hayConsolasReady);
				//SE DEBERA DEVOLVER EL CONTEXTO DE EJECUCION A LA CPU PARA QUE CONTINUE EL MISMO PROCESO.
				return;

	}else{
		list_add_in_index(runningPCB->tablaArchivos,0,arch_found);
			//se bloquea al PCB En la cola del archivo que abrió
			t_list* cola_del_archivo=dictionary_get(diccionario_colas_archivos, name);
			list_add(cola_del_archivo, runningPCB);
			log_warning(logger, "PID: <%d> - Bloqueado por: <%s>", runningPCB->PID, name);

			return;


	}
}


int truncate_petition(char*name, int size){

	t_paquete* paquete;
	paquete= crear_paquete_contexto(F_TRUNCATE);

//filename size, filename, actual size.
		int fileSize = strlen(name)+1;

		paquete->buffer->stream = realloc(paquete->buffer->stream, paquete->buffer->size +sizeof(int) + (strlen(name)+1) + sizeof(int));

		memcpy(paquete->buffer->stream + paquete->buffer->size, &fileSize, sizeof(int));//el size del nombre.
		paquete->buffer->size += sizeof(int);
						//en la linea de abajo no sumar sizeof(int), ya se lo sumaste al buffer size
		memcpy(paquete->buffer->stream + paquete->buffer->size , name, (strlen(name)+1));//nombre archivo en char*
		paquete->buffer->size += (strlen(name)+1);
				//aca tampoco sumar sizeof(int) y tampoco strlen(name)+1. Ya los acumulaste en buffer size
		memcpy(paquete->buffer->stream + paquete->buffer->size , &size, sizeof(int)); //el actual size.
		paquete->buffer->size += sizeof(int);

		int bytes_enviados=enviar_paquete(paquete, socket_fs);//+CodOp paquete y size
		eliminar_paquete(paquete);

		return bytes_enviados;

}

void block_NODE(BlockedNode *nodo){
	sem_wait(&mutexConsolasBlocked);
	list_add(consolasBlocked,nodo); //meterlo a list add.
	sem_post(&mutexConsolasBlocked);
	sem_post(&hayConsolasBlocked);
}

sem_t mutexCBcounter;
int compactionBlockers=0;

void recieves_filesystem_petition(int cod_op, PCB* runningPCB, t_list* parametros){
	BlockedNode *nuevo_nodo=(BlockedNode*)malloc(sizeof(BlockedNode));
	nuevo_nodo->cod_op=cod_op;
	nuevo_nodo->parametros=parametros;
	nuevo_nodo->un_pcb=runningPCB;
	nuevo_nodo->impide_compactación=false;
	if(cod_op==F_WRITE || cod_op==F_READ){
		nuevo_nodo->impide_compactación=true;
		sem_wait(&mutexCBcounter);
		compactionBlockers++;
		sem_post(&mutexCBcounter);
	}
	block_NODE(nuevo_nodo);
	//el hilo principal termina aqui. Reinicia
}

void f_truncate_process(t_list* parametros, PCB* runningPCB){

	char* name = (char*)list_get(parametros,0);
	int size = atoi(list_get(parametros,1));
	int confirmation;
	truncate_petition(name,size);//Enviar petition de truncado del archivo.


	//bloquearse con un recv simple de un 1 (int) de confirmación.

	recv(socket_fs, &confirmation, sizeof(int), MSG_WAITALL);
	if(confirmation==-1){
		char* mensaje_error=string_from_format("F_TRUNCATE: OUT_OF_SPACE");
		log_warning(logger, "Finaliza el proceso <%d> - Motivo: <%s>", runningPCB->PID, mensaje_error);
		int mensajeFin=ERROR;
		send(runningPCB->PID, &mensajeFin, sizeof(int), 0);
		enviar_mensaje(mensaje_error, runningPCB->PID);
		replanificar=true;
		sem_wait(&mutexConsolasExit);
			list_add(consolasExit, runningPCB);
		sem_post(&mutexConsolasExit);
		sem_post(&hayConsolasExit);
		return;
	}else if(confirmation==2){
		char* mensaje_error=string_from_format("F_TRUNCATE: %s not found", name);
		log_warning(logger, "Finaliza el proceso <%d> - Motivo: <%s>", runningPCB->PID, mensaje_error);
		int mensajeFin=ERROR;
		send(runningPCB->PID, &mensajeFin, sizeof(int), 0);
		enviar_mensaje(mensaje_error, runningPCB->PID);
		replanificar=true;
		sem_wait(&mutexConsolasExit);
			list_add(consolasExit, runningPCB);
		sem_post(&mutexConsolasExit);
		sem_post(&hayConsolasExit);
		return;
	}
	log_info(logger, "PID: <%d> - Archivo: <%s> - Tamaño: <%d>", runningPCB->PID, name, size);

	est_prox_rafaga_backup=runningPCB->est_prox_rafaga;
	runningPCB->est_prox_rafaga=0.01;
	sem_wait(&mutexConsolasReady);
		clock_gettime(CLOCK_REALTIME, &runningPCB->tiempo_Llegada_Ready);
		list_add_in_index(consolasReady, 0, runningPCB);
		log_warning(logger, "PID: <%d> - Estado Anterior: <BLOQUEADO> - Estado Actual: <READY>", runningPCB->PID);
	sem_post(&mutexConsolasReady);
	sem_post(&hayConsolasReady);

}

void process_fseek(t_list*parametros, PCB* runningPCB){

	char* name = (char*)list_get(parametros,0);
	int puntero = atoi(list_get(parametros,1));

	bool descriptor_finder(Descriptor* aux){
			return strcmp(aux->file_name,name)==0?true:false;
		};

	sem_wait(&mutexConsolasReady);
	Descriptor* descr_del_archivo = list_find(runningPCB->tablaArchivos,(void*)descriptor_finder); //encontrar archivo a hacer seek.
	descr_del_archivo->pointer=puntero;

	log_info(logger, "PID: <%d> - Actualizar puntero Archivo: <%s> - Puntero <%d>", runningPCB->PID, name, puntero);

	est_prox_rafaga_backup=runningPCB->est_prox_rafaga;
			runningPCB->est_prox_rafaga=0.01;
			clock_gettime(CLOCK_REALTIME, &runningPCB->tiempo_Llegada_Ready);
			list_add_in_index(consolasReady, 0, runningPCB);
			log_warning(logger, "PID: <%d> - Estado Anterior: <BLOQUEADO> - Estado Actual: <READY>", runningPCB->PID);
		sem_post(&mutexConsolasReady);
		sem_post(&hayConsolasReady);
	// creo que esto es suficiente para reemplezar el puntero en la lista de archivos?
	//creo que no se requiere un complicado list_replace_destroy_element o lo que sea...



}

void send_freadwrite_petition(char* filename, int* memAddr, int* bytes, int* fsPointer, int codOp){
	t_paquete* paquete;
	paquete= crear_paquete_contexto(codOp);

	int fileSize = strlen(filename)+1;

	paquete->buffer->stream = realloc(paquete->buffer->stream, fileSize+4*sizeof(int));

	memcpy(paquete->buffer->stream, &fileSize, sizeof(int));//el size del nombre.
	paquete->buffer->size += sizeof(int);
	memcpy(paquete->buffer->stream + paquete->buffer->size, filename, fileSize);
	paquete->buffer->size += fileSize;
	memcpy(paquete->buffer->stream + paquete->buffer->size, memAddr, sizeof(int));
	paquete->buffer->size += sizeof(int);
	memcpy(paquete->buffer->stream + paquete->buffer->size, bytes, sizeof(int));
	paquete->buffer->size += sizeof(int);
	memcpy(paquete->buffer->stream + paquete->buffer->size, fsPointer, sizeof(int));
	paquete->buffer->size += sizeof(int);

	enviar_paquete(paquete, socket_fs);//+CodOp paquete y size
	eliminar_paquete(paquete);
}

void process_fwrite(t_list*parametros, PCB* runningPCB){
	char* filename=list_get(parametros, 0);
	bool descriptor_finder(Descriptor* aux){
		return strcmp(aux->file_name,filename)==0?true:false;
	};
	Descriptor* descr_del_archivo = list_find(GDT,(void*)descriptor_finder);
	int memAddr=atoi(list_get(parametros, 1));
	int bytes=atoi(list_get(parametros, 2));
	int fsPointer=descr_del_archivo->pointer;
	send_freadwrite_petition(filename, &memAddr, &bytes, &fsPointer,F_WRITE);
	int rta;
	recv(socket_fs, &rta, sizeof(int), MSG_WAITALL);

	log_info(logger, "PID: <%d> - Escribir Archivo: <%s> - Puntero <%d> - Dirección Memoria <%d> - Tamaño <%d>", runningPCB->PID, filename, descr_del_archivo->pointer, memAddr, bytes);

	sem_wait(&mutexConsolasReady);

	est_prox_rafaga_backup=runningPCB->est_prox_rafaga;
	runningPCB->est_prox_rafaga=0.01;
	clock_gettime(CLOCK_REALTIME, &runningPCB->tiempo_Llegada_Ready);
	list_add_in_index(consolasReady, 0, runningPCB);
	log_warning(logger, "PID: <%d> - Estado Anterior: <BLOQUEADO> - Estado Actual: <READY>", runningPCB->PID);
	sem_post(&mutexConsolasReady);
	sem_post(&hayConsolasReady);
}

void process_fread(t_list*parametros, PCB* runningPCB){
	char* filename=list_get(parametros, 0);
	bool descriptor_finder(Descriptor* aux){
		return strcmp(aux->file_name,filename)==0?true:false;
	};
	Descriptor* descr_del_archivo = list_find(GDT,(void*)descriptor_finder);
	int memAddr=atoi(list_get(parametros, 1));
	int bytes=atoi(list_get(parametros, 2));
	int fsPointer = descr_del_archivo->pointer;
	send_freadwrite_petition(filename, &memAddr, &bytes, &fsPointer,F_READ);
	int rta;
	recv(socket_fs, &rta, sizeof(int), MSG_WAITALL);

	log_info(logger, "PID: <%d> - Leer Archivo: <%s> - Puntero <%d> - Dirección Memoria <%d> - Tamaño <%d>", runningPCB->PID, filename, descr_del_archivo->pointer, memAddr, bytes);

	est_prox_rafaga_backup=runningPCB->est_prox_rafaga;
	runningPCB->est_prox_rafaga=0.01;
	clock_gettime(CLOCK_REALTIME, &runningPCB->tiempo_Llegada_Ready);
	list_add_in_index(consolasReady, 0, runningPCB);
	log_warning(logger, "PID: <%d> - Estado Anterior: <BLOQUEADO> - Estado Actual: <READY>", runningPCB->PID);
	sem_post(&mutexConsolasReady);
	sem_post(&hayConsolasReady);
}

//la ejecuta el thread principal cuando solicita y confirma la compactación de memoria
bool compaction_required=false;
sem_t mutex_CR, compaction_permission, mutex_CA;
bool compaction_accessed=true;

void compaction_manager(){
	sem_wait(&mutex_CR);
		compaction_required=true;
	sem_post(&mutex_CR);
	if(compactionBlockers>0){
		log_info(logger, "Compactación: <Esperando Fin de Operaciones de FS>");
		sem_wait(&compaction_permission);
	}
	log_warning(logger, "Compactación: <Se solicitó compactación>");
	//el hilo que controla la cola de operaciones a FS me dio permiso
	int codop=COMPACTION;
	send(socket_memoria, &codop,sizeof(int),0);
	//se espera finalizacion y devolucion de tablas
	recibir_tablas_compactadas();
	log_warning(logger, "Se finalizó el proceso de compactación");
}


void operarFS(){
	while(1){
		//si compactionBlockers==0 (no hay operaciones que impidan compactacion en cola)
		// y compactacion se solicito, doy permiso de compactación
		if(compactionBlockers==0){
			sem_wait(&mutex_CR);
			bool value_CR=compaction_required;
			sem_post(&mutex_CR);
			if(value_CR){
				sem_post(&compaction_permission);
				compaction_required=false;
				//continuo ejecutando otras operaciones de file system si las hubiese
			}
		}
		BlockedNode* un_nodo;
		sem_wait(&hayConsolasBlocked);
		sem_wait(&mutexConsolasBlocked);
		un_nodo=list_remove(consolasBlocked, 0);
		sem_post(&mutexConsolasBlocked);


		switch(un_nodo->cod_op){
		case F_OPEN:
			process_f_open(un_nodo->parametros, un_nodo->un_pcb);
			break;
		case F_CLOSE:
			process_f_close(un_nodo->parametros, un_nodo->un_pcb);
			break;
		case F_TRUNCATE:
			f_truncate_process(un_nodo->parametros, un_nodo->un_pcb);
			break;
		case F_SEEK:
			process_fseek(un_nodo->parametros,un_nodo->un_pcb);
			break;
		case F_WRITE:
			process_fwrite(un_nodo->parametros, un_nodo->un_pcb);
			sem_wait(&mutexCBcounter);
			compactionBlockers--;
			sem_post(&mutexCBcounter);
			break;
		case F_READ:
			process_fread(un_nodo->parametros, un_nodo->un_pcb);
			sem_wait(&mutexCBcounter);
			compactionBlockers--;
			sem_post(&mutexCBcounter);
			break;
		default:
			log_error(logger, "Operacion de filesystem corrompida. R: %d", un_nodo->cod_op);
			exit(1);
			break;
		}
		string_list_cleaner(un_nodo->parametros);
		free(un_nodo);
	}
}


//dado un codop, devuelve si se corresponde con una operación de file system
bool es_operacion_de_FS(int cod_op){
	const int FS_OP_AMOUNT=6;
	int fsOps[]={F_OPEN, F_CLOSE, F_READ, F_WRITE, F_TRUNCATE, F_SEEK};
	for(int i=0; i<FS_OP_AMOUNT; i++){
		if(fsOps[i]==cod_op){
			return true;
		}
	}return false;
}


void readOp(int cod_op, PCB *runningPCB, t_list* parametros){
	void liberadorCadenas(char* cadena){
		free(cadena);
	}
	int ID;

	if(es_operacion_de_FS(cod_op)){
		log_warning(logger, "PID: <%d> - Estado Anterior: <RUNNING> - Estado Actual: <BLOQUEADO>", runningPCB->PID);
		recieves_filesystem_petition(cod_op, runningPCB, parametros);
		replanificar=true;
		return;
	}

	switch(cod_op){
	case YIELD:
		log_info(logger, "PID: <%d> - Estado Anterior: <RUNNING> - Estado Actual: <READY>", runningPCB->PID);
		clock_gettime(CLOCK_REALTIME, &runningPCB->tiempo_Llegada_Ready);
		replanificar=true;
		sem_wait(&mutexConsolasReady);
			list_add(consolasReady, runningPCB);
		sem_post(&mutexConsolasReady);
		sem_post(&hayConsolasReady);
		string_list_cleaner(parametros);
		break;
	case EXIT:
		log_info(logger, "Finaliza el proceso <%d> - Motivo: <SUCCESS>", runningPCB->PID);
		replanificar=true;
		sem_wait(&mutexConsolasExit);
			list_add(consolasExit, runningPCB);
		sem_post(&mutexConsolasExit);
		sem_post(&hayConsolasExit);
		string_list_cleaner(parametros);
		break;
	case WAIT:
		char* recurso = list_get(parametros,0);
		wait_recurso_lista(recurso, runningPCB);
		string_list_cleaner(parametros);
		break;
	case SIGNAL:
		char* recurso1 = list_get(parametros,0);
		signal_recurso_lista(recurso1, runningPCB);
		string_list_cleaner(parametros);
		break;

	case I_O:
		char* tiempoBloqueo=list_get(parametros,0);
		log_info(logger, "PID: <%d> - Ejecuta IO: <%s>", runningPCB->PID, tiempoBloqueo);
		replanificar=true;
		i_o(tiempoBloqueo, runningPCB);
		string_list_cleaner(parametros);
		break;
	case ERROR:
		char* mensaje_error=list_get(parametros, 0);
		log_warning(logger, "Finaliza el proceso <%d> - Motivo: <%s>", runningPCB->PID, mensaje_error);
		int mensajeFin=ERROR;
		send(runningPCB->PID, &mensajeFin, sizeof(int), 0);
		enviar_mensaje(mensaje_error, runningPCB->PID);
		list_destroy_and_destroy_elements(parametros, (void*)liberadorCadenas);
		replanificar=true;
		sem_wait(&mutexConsolasExit);
			list_add(consolasExit, runningPCB);
		sem_post(&mutexConsolasExit);
		sem_post(&hayConsolasExit);
		break;
	case CREATE_SEGMENT:
		ID=atoi((char*)list_get(parametros,0));
		int size=atoi((char*)list_get(parametros,1));
		create_segment(runningPCB, ID, size); // 0 is segment ID. 1 is size.

		string_list_cleaner(parametros);
		break;

	case DELETE_SEGMENT:
		ID=atoi((char*)list_get(parametros,0));
		delete_segment(runningPCB, ID);
		sem_wait(&mutexConsolasReady);
			list_add_in_index(consolasReady, 0, runningPCB); //agrego adelante para que vuelva a ejecutar
		sem_post(&mutexConsolasReady);
		sem_post(&hayConsolasReady);
		string_list_cleaner(parametros);
		break;
	default:
		perror("Error al leer la operacion:");
		exit(1);
	}

}




void hrrnListSort(t_list* consolasReady){ //usar maximum para modificar la lista, para que el mas grande vaya a 0.
	struct timespec timeActual;
	clock_gettime(CLOCK_REALTIME, &timeActual);
	// ENUNCIADO: "El comparador devuelve true si el
	//	primer parametro debe aparecer antes que el segundo en la lista"
	bool maximum(PCB* pcb1, PCB* pcb2){
		//w1 y w2 son el tiempo de cada uno en espera desde que llegó a ready, en MILISEGUNDOS
		uint64_t w1 = (timeActual.tv_sec - pcb1->tiempo_Llegada_Ready.tv_sec)* 1000 + (timeActual.tv_nsec - pcb1->tiempo_Llegada_Ready.tv_nsec) / 1000000;
		uint64_t w2 = (timeActual.tv_sec - pcb2->tiempo_Llegada_Ready.tv_sec)* 1000 + (timeActual.tv_nsec - pcb2->tiempo_Llegada_Ready.tv_nsec) / 1000000;
		double R1= 1+(w1/pcb1->est_prox_rafaga);		//Los estimados se deberán calcular DESPUES de completar la instrucción de CPU
		double R2= 1+(w2/pcb2->est_prox_rafaga);
		if(pcb1->est_prox_rafaga==0.01){
			pcb1->est_prox_rafaga=est_prox_rafaga_backup;
		}
		if(pcb2->est_prox_rafaga==0.01){
			pcb2->est_prox_rafaga=est_prox_rafaga_backup;
		}
		return R1>R2;
	}
	list_sort(consolasReady, (void*)maximum);
	char* stringReadyPIDs=actualizarStringColaReady();
	log_info(logger, "Cola Ready <HRRN> : %s", stringReadyPIDs);
	free(stringReadyPIDs);
}

void hrrnRunning(void){

	void estimarProximaRafaga(PCB* readyPCB, uint64_t realEjecutado){
		double proxima_rafaga;
		double alfa=kerConfigData.HRRN_ALFA;
		//considerar que "readyPCB->est_prox_rafaga" es el estimado anterior ahora
		proxima_rafaga=alfa*realEjecutado + (1-alfa)*(readyPCB->est_prox_rafaga);
		readyPCB->est_prox_rafaga=proxima_rafaga;
	}
	while(1){
		replanificar=false;
		sem_wait(&hayConsolasReady);
		sem_wait(&mutexConsolasReady);
			hrrnListSort(consolasReady);
		sem_post(&mutexConsolasReady);
		sem_post(&hayConsolasReady);
		uint64_t realEjecutado=0;

		sem_wait(&mutexConsolasReady);
			PCB *nextPCB=(PCB*)list_get(consolasReady, 0);
		sem_post(&mutexConsolasReady);

		log_info(logger, "PID: <%d> - Estado Anterior: <READY> - Estado Actual: <RUNNING>", nextPCB->PID);
		while(!replanificar){
			sem_wait(&hayConsolasReady);
			sem_wait(&mutexConsolasReady);
				PCB *runningPCB=(PCB*)list_remove(consolasReady, 0);
			sem_post(&mutexConsolasReady);
			enviar_PCB(runningPCB); //hace uso de paquete_contexto()
				//salió a CPU
			t_list* parametros = list_create();
				//volvió
			int cod_op=recibir_PCB(runningPCB,parametros);
			realEjecutado += (end.tv_sec - start.tv_sec) * 1000 + (end.tv_nsec - start.tv_nsec) / 1000000;
			estimarProximaRafaga(runningPCB, realEjecutado);
			readOp(cod_op, runningPCB, parametros);
		}
	}
}

void fifoRunning(void){  //planificador corto
	while(1){
		sem_wait(&hayConsolasReady);
		char* stringReadyPIDs=actualizarStringColaReady();
		log_info(logger, "Cola ready <FIFO> : %s", stringReadyPIDs);
		free(stringReadyPIDs);
		sem_wait(&mutexConsolasReady);
		PCB *runningPCB=(PCB*)list_remove(consolasReady, 0);
		sem_post(&mutexConsolasReady);
		enviar_PCB(runningPCB); //hace uso de paquete_contexto()
		t_list* parametros = list_create();
		//tiempo de ejecución en CPU
		int cod_op=recibir_PCB(runningPCB,parametros); //hace uso de recibir_paquete_contexto()
		readOp(cod_op, runningPCB, parametros);
	}
}

void recibirStart(){
	log_info(logger,"Presionar ENTER para habilitar RUNNING. Recibiendo consolas mientras tanto... ");
	getchar();
	log_info(logger, "Se habilitó al planificador de corto plazo. Comenzando ejecución.");
	sem_post(&senialInicio);
	pthread_exit(NULL);
}

void gestionadorRunning(){
	sem_wait(&senialInicio); //espero a que me habiliten presionando ENTER
		if(strcmp("FIFO",kerConfigData.ALGORITMO_PLANIFICACION)==0)
		{ // FIFO elegido
			fifoRunning();
		}
		else // HRRN elegido
		{
			hrrnRunning();
		}

}

void liberarRecursosDelProceso(char* PID){
	t_list* recursos_asignados=dictionary_get(recursos_por_proceso, PID);
	if(recursos_asignados==NULL){
		//nunca solicitó WAIT
		return;
	}
	int cantidad_recursos=list_size(recursos_asignados);
	if(cantidad_recursos==0){
		//liberó todos sus recursos previamente
		return;
	}else{
		for(int i=0; i<cantidad_recursos;i++){
			//libero cada uno de los recursos que tiene asignados
			char* un_recurso=list_remove(recursos_asignados,0);
			liberar_recurso_y_actualizar_ready(un_recurso, atoi(PID));
			free(un_recurso);
		}list_destroy(recursos_asignados);
	}
}

void cerrarArchivosDelProceso(PCB* exitPCB){
	int cant_archivos=list_size(exitPCB->tablaArchivos);
	for(int i=0; i<cant_archivos; i++){
		Descriptor* un_descr=list_remove(exitPCB->tablaArchivos, 0);
		liberar_archivo(un_descr);
	}list_destroy(exitPCB->tablaArchivos);
}

void gestionadorExit(void){

	void liberador(void* a){
		free(a);
	}

	while(1){
		sem_wait(&hayConsolasExit);
		sem_wait(&mutexConsolasExit);
			PCB *exitPCB=(PCB*)list_remove(consolasExit, 0);
		sem_post(&mutexConsolasExit);
		list_remove_element(global_PCB_list, exitPCB);
		//limpio la lista de instrucciones
		list_destroy_and_destroy_elements(exitPCB->instrucciones, (void*)liberador);
		for(int i=0; i<CANTIDAD_REGISTROS_CPU;i++){
			free(exitPCB->registros[i]);
		}
		//limpio la tabla de segmentos del modulo memoria y la tabla en kernel
		char* charPID=string_itoa(exitPCB->PID);
		enviar_mensaje_razon(EXIT, charPID, socket_memoria);
		list_destroy_and_destroy_elements(exitPCB->tablaSegmentos, (void*)liberador);
		//libero los recursos por WAIT asignados si los tuviese
		liberarRecursosDelProceso(charPID);
		//cierro los archivos abiertos si los tuviese
		cerrarArchivosDelProceso(exitPCB);
		free(charPID);
		int mensajeFin=EXIT;
		send(exitPCB->PID, &mensajeFin, sizeof(int), 0);
		close(exitPCB->PID);
		free(exitPCB);
		sem_post(&hayEspacioReady);
	}

}

//función debajo recibe tablas compactadas y actualiza los PCBs en lista GLOBAL

void recibir_tablas_compactadas(){
	int size;
	int cantSegmentos, cantTablas;
	int desplazamiento=0;
	int PID;
	int ID, base, limite; //se cargan en cada iteracion
	recibir_operacion(socket_memoria);
	void* buffer = recibir_buffer(&size, socket_memoria);
	bool esElPCB(PCB* un_PCB){
		return un_PCB->PID==PID;
	}
	void liberador(void*a){
		free(a);
	}
	memcpy(&cantTablas, buffer, sizeof(int));
	desplazamiento+=sizeof(int);

	for(int j=0;j<cantTablas;j++){
		t_list* tabla_segmentos = list_create();
		memcpy(&PID, buffer+desplazamiento, sizeof(int));
		desplazamiento+=sizeof(int);
		memcpy(&cantSegmentos, buffer+desplazamiento, sizeof(int));
		desplazamiento+=sizeof(int);
		for(int i=0; i<cantSegmentos; i++){

			memcpy(&ID, buffer + desplazamiento, sizeof(int));
			desplazamiento+=sizeof(int);

			memcpy(&base, buffer + desplazamiento, sizeof(int));
			desplazamiento+=sizeof(int);

			memcpy(&limite, buffer + desplazamiento, sizeof(int));
			desplazamiento+=sizeof(int);
			Segmento * nuevoSegmento=(Segmento*)malloc(sizeof(Segmento));
			nuevoSegmento->ID=ID;
			nuevoSegmento->base=base;
			nuevoSegmento->limite=limite;
			list_add(tabla_segmentos, nuevoSegmento);
		}
		PCB *un_PCB=list_find(global_PCB_list, (void*)esElPCB);
		list_clean_and_destroy_elements(un_PCB->tablaSegmentos, (void*)liberador);
		un_PCB->tablaSegmentos=tabla_segmentos;
	}
	free(buffer);
}




