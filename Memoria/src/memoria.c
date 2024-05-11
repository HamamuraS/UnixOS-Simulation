
#include "memoria.h"




int main(void) {

	printf("Se requiere el archivo de configuración: ");
		char configName[35];
		char confAux [37]="./";
		fgets(configName, sizeof(configName), stdin);
		configName[strcspn(configName, "\n")] = '\0';
		strcat(confAux, configName);
	t_config* config = iniciar_config(confAux);

	if(config == NULL){
		printf("No se pudo abrir el archivo de configuracion\n");
		return 1;
	}
	logger = log_create("status.log", "Log de memoria", 1, LOG_LEVEL_DEBUG); //Logger listo

	configData = cargarConfigData(config);	//Config listo
	log_info(logger, "Algoritmo de asignación: %s", configData.ALGORITMO_ASIGNACION);

	algoritmos_dictionary=cargarDiccionarioAlgoritmos(); //contiene IDs de cada algoritmo de asignación


	int socket_mem = iniciar_servidor(configData.PUERTO_ESCUCHA);	//Socket de escucha listo
	log_info(logger, "Memoria escuchando a través del socket %d", socket_mem);

	t_list* connectedSockets = inicializarListaSockets(); //Lista para sockets provisionales
	//despues de esperarConexiones los sockets definitivos estarán cargados en las variables globales socketFS, socketKer, socketCPU

	bool modConectado[5]={false, false, false, false, false}; //confirmed connections, flags para registrar conexiones
	esperarConexiones(modConectado, connectedSockets, socket_mem);//bloquea hasta recibir las 3 conex. y devuelve 1 o -1 como respuesta de hanshake

	//Inicializando estructuras administrativas

	user_space=malloc(configData.TAM_MEMORIA);
	memset(user_space, ' ', configData.TAM_MEMORIA);

	huecos_libres=iniciar_lista_huecos_libres();
	//nota: calloc_segment hace la creacion y asignacion de segmentos. create_segment además maneja errores y comunica con kernel
	tabla_inicial=list_create();
	calloc_segment(tabla_inicial, 0, configData.TAM_SEGMENTO_0);

	sem_init(&user_space_isFree, 0, 1);

	diccionario_tablas=dictionary_create(); //será mi diccionario de t_list*tabla_segmento indexados por PID


	pthread_t threadEsperarKernel;
	pthread_t threadEsperarCPU;
	pthread_t threadEsperarFileSystem;

	pthread_create(&threadEsperarKernel, NULL, (void*)esperarCPU, NULL);
	pthread_create(&threadEsperarCPU, NULL, (void*)esperarKernel, NULL);
	pthread_create(&threadEsperarFileSystem, NULL, (void*)esperarFileSystem, NULL);

	///////////////////////////////////

	pthread_join(threadEsperarKernel, NULL);
	pthread_join(threadEsperarCPU, NULL);
	pthread_join(threadEsperarFileSystem, NULL);


	return EXIT_SUCCESS;
}

