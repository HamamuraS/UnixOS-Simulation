#include "Consola.h"


#define MAX_LINE_LENGTH 256 // Suponiendo instrucciones tipo, sino hay que subirlo.
//La consola tiene que conectarse a levantar la config, leer un pseudocodigo con instrucciones
//Con las instrucciones leidas arma un paquete y envia a kernel, espera respuesta.
//No hace falta montarla como servidor, por que unicamente es cliente de Kernel

int main(void)
{
	/*---------------------------------------------------PARTE 2-------------------------------------------------------------*/
	int num_instrucciones;
	int socket_ker;
	char* ip_kernel;
	char* puerto_kernel;
	char** lista_instrucciones;

	// Usando el logger creado previamente
	logger=log_create("./CONSOLA.log","CONSOLA",true, LOG_LEVEL_INFO); // @suppress("Symbol is not resolved")

	/* ---------------- ARCHIVOS DE CONFIGURACION ---------------- */


	printf("Se requiere el archivo de configuración: ");
		char configName[35];
		char confAux [37]="./";
		fgets(configName, sizeof(configName), stdin);
		configName[strcspn(configName, "\n")] = '\0';
		strcat(confAux, configName);
	t_config* config = iniciar_config(confAux);


	if(config == NULL){
			log_error(logger, "No se pudo abrir el archivo %s \n", confAux);
			return 1;
	}
	ip_kernel = config_get_string_value(config, "IP_KERNEL");
	puerto_kernel = config_get_string_value(config, "PUERTO_KERNEL");

	printf("Nombre de archivo INSTRUCCIONES a ejecutar: ");
	char instrName[35];
	char instrAux[37];
	fgets(instrName, sizeof(instrName), stdin);
	instrName[strcspn(instrName, "\n")] = '\0';
	strcat(instrAux, instrName);
	lista_instrucciones=leer_pseudocodigo(instrAux, &num_instrucciones);

	socket_ker = crear_conexion(ip_kernel, puerto_kernel);
	int handshake =1;
	send(socket_ker, &handshake, sizeof(int), 0);
	int respuesta;
	recv(socket_ker, &respuesta, sizeof(int), MSG_WAITALL);
	if(respuesta==1){
		log_info(logger, "Conexion exitosa con Kernel. Esperando respuesta...");
	}else{
		log_error(logger, "Comunicación con el Kernel alterada. Abortando Consola");
		close(socket_ker);
		exit(1);
	}

	paquete_instrucciones(socket_ker, num_instrucciones,lista_instrucciones);
	borrar_lista(lista_instrucciones, num_instrucciones);

	int cod_op = recibir_operacion(socket_ker);
	switch(cod_op){
		case EXIT:
			log_info(logger, "Proceso finalizado correctamente. Cerrando Consola");
			terminar_programa(socket_ker, logger, config);
			return 0;
			break;
		case ERROR:
			int codigo_mensaje; //parche para poder usar las funciones de mensajes
			recv(socket_ker, &codigo_mensaje, sizeof(int), MSG_WAITALL);//
			char* mensaje_error=recibir_mensaje(socket_ker);
			log_error(logger, "%s", mensaje_error);
			free(mensaje_error);
			terminar_programa(socket_ker, logger, config);
			return 1;
			break;
	}

	return 0;
}



