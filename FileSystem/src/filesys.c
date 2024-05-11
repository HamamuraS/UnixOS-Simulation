
#include "filesys.h"

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
    fsConfig=cargarConfigFS(config); //ahora todos los datos del config son accesibles a traves de fsConfig


    logger = log_create("status.log", "Log de FileSystem", 1, LOG_LEVEL_DEBUG);

    fs=(FileSys*)malloc(sizeof(FileSys));

    //inicio el socket de file system


    int socket_fs = iniciar_servidor(fsConfig.PUERTO_ESCUCHA);


    log_info(logger, "FileSystem escuchando a través del socket %d", socket_fs);


    //creo las conexiones a memoria y kernel

    socket_mem = crear_conexion(fsConfig.IP_MEMORIA, fsConfig.PUERTO_MEMORIA);
    enviarIdentidad(FileSystem, socket_mem);
    int respuestaMem;
    recv(socket_mem, &respuestaMem, sizeof(int), MSG_WAITALL);
    if(respuestaMem==1){
        log_info(logger, "Comunicación con Memoria correcta. Iniciando bitmap de bloques");
    }else{
        log_error(logger, "Comunicación con Memoria perturbada. Abortando FileSystem");
        return -1;
    }


    //inicializo el superbloque y leo los archivos de bitmap y bloques
    super_bloque = leer_superbloque(fsConfig.PATH_SUPERBLOQUE);

    //carga el struct global Bitmap bitmap, contiene t_bitarray* puntero a archivo bitmap mapeado a memoria
    crearArchivoBitmap(fsConfig.PATH_BITMAP, super_bloque.block_count);
    log_info(logger, "Archivo de Bitmap leido con exito. %ld bits.", bitarray_get_max_bit(bitmap.data));
    int cantidad_bloques_ocupados=0;
    for(int i=0; i<bitarray_get_max_bit(bitmap.data); i++){
    	if(bitarray_test_bit(bitmap.data, i)){
    		cantidad_bloques_ocupados++;
    	}
    }
    log_info(logger, "Se encontraron %d bloques ocupados.", cantidad_bloques_ocupados);

    //crea o abre el archivo de bloques y devuelve puntero a archivo mapeado a memoria
    datos_bloques=rdwrArchivoBloques(fsConfig.PATH_BLOQUES, super_bloque.block_count, super_bloque.block_size);
    log_info(logger, "Archivo de Bloque leido con exito");

    //carga lista con FCBs

    inicializarListaFCB(fsConfig.PATH_FCB);

    log_info(logger, "Esperando a Kernel");
    //SOCKET DE RESPUESTAS A LAS PETICIONES DE KERNEL
    socket_ker = esperar_cliente(socket_fs);
    int handshakeKer;
    recv(socket_ker, &handshakeKer, sizeof(int), MSG_WAITALL);
    if(handshakeKer==1){
        log_info(logger, "Comunicación con Kernel correcta.");
        send(socket_ker, &handshakeKer, sizeof(int), 0);
    }
    else{
        log_error(logger, "Comunicación con Kernel perturbada. Cerrando conexión y abortando FileSystem.");
        int respuestaNegativa=-1;
        send(socket_ker, &respuestaNegativa, sizeof(int), 0);
        close(socket_ker);
        return -1;
    }
    //fcntl(socket_ker, F_SETFL, O_NONBLOCK); //socket de kernel no bloqueante

    log_info(logger, "Esperando peticiones del kernel:");
    //switchear los codigos de operacion

	manejarConexionesFS();
    //libero los espacios de memoria usados por bitmap y los bloques
    bitarray_destroy(bitmap.data);
    munmap(datos_bloques, super_bloque.block_size * super_bloque.block_count);
    log_info(logger, "Liberando memoria y cerrando File System");

    return EXIT_SUCCESS;


}
