
#include "cpu.h"

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

	configData = cargarConfigData(config);	//configData contiene todos los datos del archivo .config

	logger = log_create("status.log", "Log de CPU", 1, LOG_LEVEL_DEBUG);


	inicializarRegistros(); //reserva memoria para cada registro y se les asigna una cadena inicial "0000..."

	//inicializar el diccionario de registros con los mallocs de inicializarRegistros
	diccionario_reg=dictionary_create();
	inicializarDiccionarioReg();

	//inicializar diccionario de id de función para switchear tokens en DECODE
	diccionario_functionsId=dictionary_create();
	inicializarDiccionarioFunId();

	socket_cpu = iniciar_servidor(configData.PUERTO_ESCUCHA);
	printf("El socket de escucha de cpu es %d \n", socket_cpu);

	//CONEXION A MEMORIA
	socket_memoria = crear_conexion(configData.IP_MEMORIA  ,configData.PUERTO_MEMORIA );
	if(socket_memoria==-1){
		log_error(logger, "no se logró la conexión con Memoria. Abortando.");
		exit(1);
	}
	enviarIdentidad(CPU, socket_memoria);
	int respuestaMemoria;
	recv(socket_memoria, &respuestaMemoria, sizeof(int), MSG_WAITALL);
	if(respuestaMemoria==1){
		log_info(logger, "Comunicación con Memoria correcta");
	}else{
		log_error(logger, "Comunicación con el módulo memoria alterada. Abortando CPU");
		close(socket_memoria);
		exit(1);
	}

	//CONEXION CON KERNEL
	log_info(logger, "CPU esperando a Kernel");
	socket_kernel=esperar_cliente(socket_cpu);
	int handshake;
	int resultOk = 1;
	int resultError= -1;

	recv(socket_kernel, &handshake, sizeof(int), MSG_WAITALL);
	if(handshake == 1){
		send(socket_kernel, &resultOk, sizeof(int), 0);
		log_info(logger, "Comunicación con Kernel correcta");}
	else{
	   send(socket_kernel, &resultError, sizeof(int), 0);}

	//RECIBIR CONTEXTO DE EJECUCION DE KERNEL -> ejecutar ciclo
	while(1){

		int cod_op = recibir_operacion(socket_kernel);
		if(cod_op==RUNNING){
			contexto = recibir_paquete_contexto(socket_kernel);
			log_info(logger, "======================EJECUTANDO PID: %d=====================", contexto.PID);
		}else{
			log_error(logger, "No se recibió el contexto correctamente");
			exit(1);
		}

		cargarRegistros(contexto.registros); //necesario antes de cada ciclo, cargar los registros de CPU
		log_info(logger, "Registros de CPU cargados.");
		log_info(logger, "Recibido PC: %d", contexto.program_counter);

		interrupted = false; //se debe setear en 1 en operaciones que expulsen el contexto
		//Comenzando cíclo de ejecución
		while(!interrupted){
			//FETCH
			char* cadenaOp=list_get(contexto.instrucciones, contexto.program_counter);
			contexto.program_counter++;
			char** listaOperandos=string_split(cadenaOp, " ");
			//string del nombre de la operación
			char* instrToken=string_duplicate(listaOperandos[0]);
			//lista con los parametros de la operación.
			//Las direcciones en parametros son las mismas que las de los char* en listaOperandos
			for(int i=1;i<string_array_size(listaOperandos);i++){
				char*un_parametro=string_duplicate(listaOperandos[i]);
				list_add(contexto.parametros,un_parametro);
			}
			logging_instrucciones(instrToken, contexto.parametros);
			//DECODE
			int* functionId=(int*)dictionary_get(diccionario_functionsId, instrToken);
			char*recurso, *regName, *value, *memAddr, *fileName, *position, *logicAddr, *bytes, *segmentId;
			if(functionId==NULL){
				char* mensaje_error=string_from_format("No se reconoció la instrucción %s", instrToken);
				exit_process_error(mensaje_error);
				continue;
			}
			switch(*functionId){
				//EXECUTE
				case 0:
					regName=(char*)list_remove(contexto.parametros, 0);
					value=(char*)list_remove(contexto.parametros, 0);
					int retVal=set(regName, value);
					if(retVal>0){
						char* mensaje_error=string_from_format("SET: Overflow de registro %s", regName);
						exit_process_error(mensaje_error);
					}
					free(regName);free(value);
					break;
				case 1:
					yield();
					interrupted=1;
					break;
				case 2:
					exit_process();
					interrupted=1;
					break;
				case 3:
					char*tiempoBloqueo=(char*)list_remove(contexto.parametros, 0);
					i_o(tiempoBloqueo);
					interrupted=1;
					break;
				case 4:
					recurso=(char*)list_remove(contexto.parametros, 0);
					wait_process(recurso);
					interrupted=1;
					break;
				case 5:
					recurso=(char*)list_remove(contexto.parametros, 0);
					signal_process(recurso);
					interrupted=1;
					break;
				case 6:
					regName=(char*)list_remove(contexto.parametros, 0);
					memAddr=(char*)list_remove(contexto.parametros, 0);
					mov_in(regName, memAddr);
					break;
				case 7:
					memAddr=(char*)list_remove(contexto.parametros, 0);
					regName=(char*)list_remove(contexto.parametros, 0);
					mov_out(regName, memAddr);
					break;
				case 8:
					fileName=(char*)list_remove(contexto.parametros, 0);
					f_open(fileName);
					interrupted=1;
					break;
				case 9:
					fileName=(char*)list_remove(contexto.parametros, 0);
					f_close(fileName);
					interrupted=1;
					break;
				case 10:
					fileName=(char*)list_remove(contexto.parametros, 0);
					position=(char*)list_remove(contexto.parametros, 0);
					f_seek(fileName, position);
					interrupted=1;
					break;
				case 11:
					fileName=(char*)list_remove(contexto.parametros, 0);
					logicAddr=(char*)list_remove(contexto.parametros, 0);
					bytes=(char*)list_remove(contexto.parametros, 0);
					f_read(fileName, logicAddr, bytes);
					interrupted=1;
					break;
				case 12:
					fileName=(char*)list_remove(contexto.parametros, 0);
					logicAddr=(char*)list_remove(contexto.parametros, 0);
					bytes=(char*)list_remove(contexto.parametros, 0);
					f_write(fileName, logicAddr, bytes);
					interrupted=1;
					break;
				case 13:
					fileName=(char*)list_remove(contexto.parametros, 0);
					bytes=(char*)list_remove(contexto.parametros, 0);
					f_truncate(fileName, bytes);
					interrupted=1;
					break;
				case 14:
					segmentId=(char*)list_remove(contexto.parametros, 0);
					bytes=(char*)list_remove(contexto.parametros, 0);
					create_segment(segmentId, bytes);
					interrupted=1;
					break;
				case 15:
					segmentId=(char*)list_remove(contexto.parametros, 0);
					delete_segment(segmentId);
					interrupted=1;
					break;

				default:
					log_error(logger, "Suceso inesperado. Finalizando por default.");
					exit(1);
					break;
			}

			free(instrToken);
			string_array_destroy(listaOperandos);
			//los parametros de contexto se eliminan en cada función
		}

	}


	return EXIT_SUCCESS;
}


