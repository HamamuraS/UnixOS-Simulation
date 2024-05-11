
#include "kernel.h"



int server_recieves(int socket_cliente, t_list* lista){ //Standard ser4ver recibiendo paquete con minima configuracion.

				int cod_op = recibir_operacion(socket_cliente); //handshake principal occure en recibir operacion.
				switch (cod_op) { //cada parte tendria que enviar exito o regreso como parte de handshake? Hablar sobre esto dsp.
				case MENSAJE:
					recibir_mensaje(socket_cliente);
					break;
				case PAQUETE:
					lista = recibir_paquete(socket_cliente);
					log_info(logger, "Me llegaron los siguientes valores:\n");
					list_iterate(lista, (void*) iterator);
					break;
				case IDENTIDAD: //conexión secundaria
					int ident = recibirIdentidad(socket_cliente);
					list_add_in_index(lista, socket_cliente, &ident);
					if(ident==Consola){
						printf("Se recibió a Consola\n");
						break;
					}else{
						printf("Conexión erronea");
						break;
					}
					break;
				case -1: // -1 si se desconecta la consola.
					log_error(logger, "el cliente se desconecto. Terminando servidor");
					send(socket_cliente,&cod_op,sizeof(int),0);
					return EXIT_FAILURE;
				default:
					log_warning(logger,"Operacion desconocida. No quieras meter la pata");
					break;
				}

	return 0;
}


/*
int kernel_sends(int socket_receptor){ //Recibe datos configuracion, conecta a CPU Memoria y File system
	char* msj;
	asprintf(&msj, "Enviando petición al socket %d", socket_receptor);
	int cod_op = recibir_operacion(socket_receptor);
					switch (cod_op) {
					case MENSAJE:
						enviar_mensaje(msj, socket_receptor);
						break;
					case PAQUETE:
						paquete(socket_receptor);
						break;
					case -1: // -1 si se desconecta la consola.
						log_error(logger, "el cliente se desconecto. Terminando servidor");
						return EXIT_FAILURE;
					default:
						log_warning(logger,"Operacion desconocida. No quieras meter la pata");
						break;
					}
	return 0;
}*/




/*
 Levanta el archivo de configuración !!!!! Done
Se conecta a CPU, Memoria y File System !!!! Done
Espera conexiones de las consolas !!!!! Done
Recibe de las consolas las instrucciones y arma el PCB
Planificación de procesos con FIFO
PCB
El PCB será la estructura base que utilizaremos dentro del Kernel para administrar los procesos lanzados por medio de las consolas.
 El mismo deberá contener como mínimo los datos definidos a continuación que representan la información administrativa necesaria y
 el Contexto de Ejecución del proceso que se deberá enviar a la CPU a través de la conexión de dispatch al momento de poner a ejecutar un proceso,
  pudiéndose extender esta estructura con más datos que requiera el grupo.
PID: Identificador del proceso (deberá ser un número entero, único en todo el sistema).
Instrucciones: Lista de instrucciones a ejecutar.
Program_counter: Número de la próxima instrucción a ejecutar.
Registros de la CPU: Estructura que contendrá los valores de los registros de uso general de la CPU.
Tabla de Segmentos: Contendrá ids, direcciones base y tamaños de los segmentos de datos del proceso.
Estimado de próxima ráfaga: Estimación utilizada para planificar los procesos en el algoritmo HRRN,
la misma tendrá un valor inicial definido por archivo de configuración y será recalculada bajo la fórmula de promedio ponderado vista en clases.
Tiempo de llegada a ready: Timestamp en que el proceso llegó a ready por última vez (utilizado para el cálculo de tiempo de espera del algoritmo HRRN).
Tabla de archivos abiertos: Contendrá la lista de archivos abiertos del proceso con la posición del puntero de cada uno de ellos.
 */

int main(void) {

	logger = log_create("status.log", "Log de kernel", 1, LOG_LEVEL_DEBUG);
	sem_init(&mutexLogger, 0, 1);

	printf("Se requiere el archivo de configuración: ");
		char configName[35];
		char confAux [37]="./";
		fgets(configName, sizeof(configName), stdin);
		configName[strcspn(configName, "\n")] = '\0';
		strcat(confAux, configName);
	t_config* config = iniciar_config(confAux); //Usa "./kernel.config"

	if(config == NULL){
		log_error(logger, "No se pudo abrir el archivo de configuracion %s", confAux);
		return 1;
	}

	sem_init(&configSem, 0, 1);
	kerConfigData = liftConfig(config); //accesible desde cualquier funcion o hilo. Proteger con semaforo configSem

	log_info(logger, "Algoritmo de planificación: %s", kerConfigData.ALGORITMO_PLANIFICACION);
	log_info(logger, "Grado de multiprogramación: %d", kerConfigData.GRADO_MAX_MULTIPROGRAMACION);
	log_info(logger, "Iniciando conexiones...");

	recursos_por_proceso=dictionary_create();
	recursosDisponibles=dictionary_create();
	recursosEnUso = dictionary_create(); //Inicializar el diccionario donde vamos a guardar PCBs bloqueados.
	init_count_recursos(); //Inicializar lista de recursos disponibles en config.
	diccionario_colas_archivos=dictionary_create();

	//CONEXION A MEMORIA

	socket_memoria = crear_conexion(kerConfigData.IP_MEMORIA  ,kerConfigData.PUERTO_MEMORIA );
	if(socket_memoria==-1){
		log_error(logger, "no se logró la conexión con Memoria. Abortando.");
		exit(1);
	}
	enviarIdentidad(Kernel, socket_memoria);
	int respuestaMemoria;
	recv(socket_memoria, &respuestaMemoria, sizeof(int), MSG_WAITALL);

	if(respuestaMemoria==1){
		sem_wait(&mutexLogger);
		log_info(logger, "Comunicación con Memoria correcta");
		sem_post(&mutexLogger);
	}else{
		log_error(logger, "Comunicación con el módulo memoria alterada. Abortando kernel");
		close(socket_cpu);
		exit(1);
	}sem_init(&mutexMemoria, 0, 1);

	//CONEXION A CPU
	socket_cpu = crear_conexion(kerConfigData.IP_CPU  , kerConfigData.PUERTO_CPU);
	if(socket_cpu==-1){
		log_error(logger, "no se logró la conexión con CPU. Abortando.");
		exit(1);
	}
	int respuestaCPU = send_handshake(Kernel, socket_cpu);
	if(respuestaCPU==1){
			log_info(logger, "Comunicación con CPU correcta.");
	}

	//CONEXION A FILESYSTEM
	int respuestaFS;
	socket_fs = crear_conexion(kerConfigData.IP_FILESYSTEM, kerConfigData.PUERTO_FILESYSTEM );
	if(socket_fs==-1){
		log_error(logger, "no se logró la conexión con File system. Abortando.");
		exit(1);
	}
	respuestaFS = send_handshake(Kernel, socket_fs);
	if(respuestaFS == 1){
		log_info(logger, "Socket de escucha a FS correcto.");
	}

	int socket_ker = iniciar_servidor(kerConfigData.PUERTO_ESCUCHA); //creacion de socket.

	consolasNew = list_create(); //lista de nuevos procesos en NEW, contendrá PCBs cargados
	consolasReady = list_create();
	consolasExit = list_create();
	global_PCB_list= list_create();
	consolasBlocked = list_create();
	GDT=list_create();

	//INICIANDO mutex para recursos compartidos
	sem_init(&mutexConsolasNew, 0, 1);
	sem_init(&mutexConsolasReady, 0, 1);
	sem_init(&mutexConsolasExit, 0, 1);
	sem_init(&mutexConsolasBlocked, 0, 1);

	//INICIANDO semaforos para controlar manejadores de estado
	sem_init(&hayEspacioReady, 0, kerConfigData.GRADO_MAX_MULTIPROGRAMACION);
	sem_init(&hayConsolasNew, 0, 0);
	sem_init(&hayConsolasReady, 0, 0);
	sem_init(&hayConsolasExit, 0, 0);
	sem_init(&hayConsolasBlocked, 0, 0);
	sem_init(&senialInicio, 0, 0);

	//INICIANDO semaforos-mutex para coordinar el inicio de compactacion
	sem_init(&mutexCBcounter, 0, 1);
	sem_init(&mutex_CR, 0, 1);
	sem_init(&compaction_permission, 0, 0);

	//INICIANDO threads manejadores de estados
	pthread_t threadGestionReady;
	pthread_create(&threadGestionReady, NULL, (void*)gestionadorReady, NULL);
	pthread_t threadGestionRunning;
	pthread_create(&threadGestionRunning, NULL, (void*)gestionadorRunning, NULL);
	pthread_t threadGestionExit;
	pthread_create(&threadGestionExit, NULL, (void*)gestionadorExit, NULL);
	pthread_t threadRecibirSTART;
	pthread_create(&threadRecibirSTART, NULL, (void*)recibirStart, NULL);
	//THREAD BLOQUEADOS POR FILESYSTEM
	pthread_t threadGestionFS;
	pthread_create(&threadGestionFS, NULL, (void*)operarFS, NULL);


	while(1){ // Condición que haga finalizar a kernel?
		pthread_t nuevasConexThr;
		int socket_consola=esperar_cliente(socket_ker);
		pthread_create(&nuevasConexThr, NULL, (void*)gestionarNuevasConexiones, (void*)&socket_consola);
		pthread_detach(nuevasConexThr);
	}

	pthread_join(threadGestionReady, NULL);
	pthread_join(threadGestionRunning, NULL);
	pthread_join(threadGestionExit, NULL);
	pthread_join(threadRecibirSTART, NULL);
	pthread_join(threadGestionFS, NULL);

	return 0;
}









