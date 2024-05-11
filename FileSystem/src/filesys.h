#ifndef FILESYS_H
#define FILESYS_H

#include "../../ganSOsLibrary/src/header.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include<commons/bitarray.h>
#include <dirent.h>

int socket_ker, socket_mem;
struct stat st;
void* datos_bloques; //archivo de bloques mapeado en memoria

typedef struct{
   int block_size;
   int block_count;
} SuperBloque;
SuperBloque super_bloque;

typedef struct {
    t_bitarray *data;
    int size;
} Bitmap;
Bitmap bitmap;

typedef struct{
   char* nombre_archivo;
   uint32_t tamanio;
   uint32_t punteroDir;
   uint32_t punteroIn;
   int cantidad_indirectos;
} FCB;

typedef struct{
   t_list* listaFCB;
   int contadorFCB;
} FileSys;
FileSys* fs;

struct configData{
   char* IP_MEMORIA;
   char* PUERTO_MEMORIA;
   char* PUERTO_ESCUCHA;
   char* PATH_SUPERBLOQUE;
   char* PATH_BITMAP;
   char* PATH_BLOQUES;
   char* PATH_FCB;
   int RETARDO_ACCESO_BLOQUE;
};
struct configData fsConfig;


struct configData cargarConfigFS(t_config* config){
   struct configData configData;
    configData.IP_MEMORIA = config_get_string_value(config, "IP_MEMORIA");
    configData.PUERTO_MEMORIA = config_get_string_value(config, "PUERTO_MEMORIA");
    configData.PUERTO_ESCUCHA = config_get_string_value(config, "PUERTO_ESCUCHA");
    configData.PATH_SUPERBLOQUE = config_get_string_value(config, "PATH_SUPERBLOQUE");
    configData.PATH_BITMAP = config_get_string_value(config, "PATH_BITMAP");
    configData.PATH_BLOQUES = config_get_string_value(config, "PATH_BLOQUES");
    configData.PATH_FCB = config_get_string_value(config, "PATH_FCB");
    configData.RETARDO_ACCESO_BLOQUE = config_get_int_value(config, "RETARDO_ACCESO_BLOQUE");
    return configData;
}

void inicializarListaFCB(char * directorioFCBs);
void abrirArchivo(char* nombre_archivo);
FCB* crearFCB(char *nombre_archivo);
void agregarFCB(char* nombre_archivo, uint32_t tamanio);
char* recv_truncateseek_petition(uint32_t* number);
void crearOAbrirArchivo(char* nombre_archivo);
void truncarArchivo(char* nombre_archivo, uint32_t nuevo_tamanio);
void manejarConexionesFS();
void crearArchivoBitmap(char* path_bitmap, int block_count);
void crearArchivo(char* nombre_archivo);
void escribirArchivo(char* nombre_archivo, int realAddr, int bytes, int puntero);
void leerArchivo(char* nombre_archivo, int realAddr, int bytes, int puntero);
void asentarFCB(FCB* fcb);

pthread_t hiloRecepcionista, hiloAtendedor; //recepcionista utiliza recibirConexionesIniciales, atendedor utiliza manejarConexionesIniciales
sem_t faltanConexiones, hayConexion; //para proteger escritura y lectura de listaWaitingSockets, lista compartida
                           //utilizo dos semaforos para sincronizar el orden de ejecución. Estrategia productor-consumidor

void retardo(){
	struct timespec ts;
	int sleep_time=fsConfig.RETARDO_ACCESO_BLOQUE;
	ts.tv_sec = sleep_time / 1000; //milisegundos a segundos
	ts.tv_nsec = (sleep_time % 1000) * 1000000; //milisegundos a nanosegundos
	nanosleep(&ts, NULL);//duerme, reteniendo al PCB durante 'time' milisegundos
}

void escribirBloque(int block_pointer, int offset, void* value, int size){
	int realOffset = block_pointer+offset;
	memcpy(datos_bloques+realOffset, value, size);
	retardo();
}

void* leerBloque(int block_pointer, int offset, int size){
	void* value=malloc(sizeof(size));
	int realOffset=block_pointer+offset;
	memcpy(value,datos_bloques+realOffset, size);
	retardo();
	return value;
}

//ocupa el primer bloque que encuentra, y devuelve su indice en el bitmap
int ocuparBloqueLibre(FCB* fcb) {
    for (int i = 0; i < bitarray_get_max_bit(bitmap.data); i++) {
    	bool bit_status=bitarray_test_bit(bitmap.data, i);
        if (!bit_status) {
            bitarray_set_bit(bitmap.data, i);
            log_info(logger, "Acceso a Bitmap - Bloque: <%d> - Estado: <%d>", i, 0);
            if(fcb->punteroDir==-1){
                fcb->punteroDir= i*super_bloque.block_size;
            }
            else if(fcb->punteroIn==-1){
                fcb->punteroIn= i*super_bloque.block_size;
                ocuparBloqueLibre(fcb);
            }
            else{
            	int newBlockPointer=i*super_bloque.block_size;
            	int bytes_ocupados=sizeof(int)*fcb->cantidad_indirectos;
            	escribirBloque(fcb->punteroIn, bytes_ocupados, &newBlockPointer, sizeof(int));
            	fcb->cantidad_indirectos++;
            }
            asentarFCB(fcb);
            return i;
        }else{
        	log_info(logger, "Acceso a Bitmap - Bloque: <%d> - Estado: <%d>", i, 1);
        }
    }
    return -1; // No hay bloques libres disponibles
}


void borrarContenido(int block_pointer, int offset, int size){
	int realOffset = block_pointer+offset;
	memset(datos_bloques+realOffset, '\0', size);
	retardo();
}

void limpiarBloque(int block_pointer){
	int block_size = super_bloque.block_size;
	memset(datos_bloques+block_pointer, '\0', block_size);
	retardo();
}

void liberarUltimoBloqueFCB(FCB* fcb){

    if(fcb->punteroIn!=-1){
    	//tiene un puntero indirecto asignado, obtengo el puntero del ultimo bloque
    	int cant_punteros=fcb->cantidad_indirectos;
    	int offset=fcb->punteroIn+sizeof(int)*(cant_punteros-1);//-1 para apuntar a la base del ultimo
    	int puntero_ultimo_bloque;
    	memcpy(&puntero_ultimo_bloque, datos_bloques+offset, sizeof(int));
    	retardo();
    	//borro la data en el bloque y lo libero en el bitarray
    	limpiarBloque(puntero_ultimo_bloque);
    	bitarray_clean_bit(bitmap.data, puntero_ultimo_bloque/super_bloque.block_size);
    	log_info(logger, "Acceso a Bitmap - Bloque: <%d> - Estado: <%d>", puntero_ultimo_bloque/super_bloque.block_size, 0);
    	//borro el puntero del bloque de punteros
    	borrarContenido(fcb->punteroIn, sizeof(int)*(cant_punteros-1), sizeof(int));
    	//si solo quedaba un puntero, borro el bloque de punteros
    	if(cant_punteros==1){
    		bitarray_clean_bit(bitmap.data, fcb->punteroIn/super_bloque.block_size);
    		log_info(logger, "Acceso a Bitmap - Bloque: <%d> - Estado: <%d>", fcb->punteroIn/super_bloque.block_size, 0);
    		//se debe grabar en el config
    		fcb->punteroIn=-1;
    	}
    	fcb->cantidad_indirectos--;
    }else{
    	//solo tiene el bloque principal, lo limpio, y libero
    	limpiarBloque(fcb->punteroDir);
    	bitarray_clean_bit(bitmap.data, fcb->punteroDir/super_bloque.block_size);
    	log_info(logger, "Acceso a Bitmap - Bloque: <%d> - Estado: <%d>", fcb->punteroDir/super_bloque.block_size, 0);
		fcb->punteroDir=-1;
    }
    //en cualquiera de los dos casos, se pierde un bloque
    fcb->tamanio-=super_bloque.block_size;
    asentarFCB(fcb);
}


//devuelve string con nombre de archivo para f_open o f_close (creo que tambien f_create)
char* recv_open_petition(){
	int buffSize;
	char* fileName;
	int desplazamiento=0;
	int tamanio;
	//void* buffer = recibir_buffer(&buffSize, socket_ker);
	void * buffer;

	recv(socket_ker, &buffSize, sizeof(int), MSG_DONTWAIT);
	buffer = malloc(buffSize);
	recv(socket_ker, buffer, buffSize, MSG_DONTWAIT);
	memcpy(&tamanio, buffer, sizeof(int));
	desplazamiento+=sizeof(int);
	fileName=(char*)malloc(tamanio);
	memcpy(fileName, buffer + desplazamiento, tamanio);
	desplazamiento+=tamanio;
	free(buffer);
	return fileName;
}

//devuelve el string con nombre de archivo. realAddr y bytes los carga indirectamente
char* recv_readwrite_petition(int* realAddr, int* bytes, int* fsPointer){
    int buffSize;
    char* fileName;
    int desplazamiento=0;
    int tamanio;
    void* buffer = recibir_buffer(&buffSize, socket_ker);
    memcpy(&tamanio, buffer, sizeof(int));
    desplazamiento+=sizeof(int);
    fileName=(char*)malloc(tamanio);
    memcpy(fileName, buffer + desplazamiento, tamanio);
    desplazamiento+=tamanio;

    memcpy(realAddr, buffer+desplazamiento, sizeof(int));
    desplazamiento+=sizeof(int);
    memcpy(bytes, buffer+desplazamiento, sizeof(int));
    desplazamiento+=sizeof(int);
    memcpy(fsPointer, buffer+desplazamiento, sizeof(int));
    desplazamiento+=sizeof(int);

    free(buffer);
    return fileName;
}

//number es tamaño para truncate

char* recv_truncate_petition(uint32_t* number){
	int buffSize;
	char* fileName;
	int desplazamiento=0;
	int tamanio;
	void* buffer = recibir_buffer(&buffSize, socket_ker);

	memcpy(&tamanio, buffer, sizeof(int));
	desplazamiento+=sizeof(int);
	fileName=(char*)malloc(tamanio);
	memcpy(fileName, buffer + desplazamiento, tamanio);
	desplazamiento+=tamanio;

	memcpy(number, buffer+desplazamiento, sizeof(int));
	desplazamiento+=sizeof(int);

	free(buffer);
	return fileName;
}

//devuelve NULL si no existe el FCB
FCB* encontrarFCB(char* nombre_archivo) {
    bool es_el_FCB(FCB* unFCB){
    	return strcmp(unFCB->nombre_archivo, nombre_archivo)==0?true:false;
    }
	FCB* elFCB_pedido = list_find(fs->listaFCB, (void*)es_el_FCB);
	return elFCB_pedido;
}



void manejarConexionesFS(){
	while(1){
		
        char* nombre_archivo;
		uint32_t nuevo_tamanio;
        int bytes_rdwr;
        int realAddr;
        int puntero;

    	int cod_op=recibir_operacion(socket_ker);
		switch (cod_op) {

			case F_OPEN:
				nombre_archivo = recv_open_petition();
				crearOAbrirArchivo(nombre_archivo);
				break;

			case F_TRUNCATE:
				nombre_archivo = recv_truncate_petition(&nuevo_tamanio);
				truncarArchivo(nombre_archivo, nuevo_tamanio);
				break;

			case F_READ:
				nombre_archivo = recv_readwrite_petition(&realAddr, &bytes_rdwr, &puntero);
                leerArchivo(nombre_archivo, realAddr, bytes_rdwr, puntero);
				break;

			case F_WRITE:
				nombre_archivo = recv_readwrite_petition(&realAddr, &bytes_rdwr, &puntero);
                escribirArchivo(nombre_archivo, realAddr, bytes_rdwr, puntero);
				break;

			default:
				log_error(logger,"Ocurrió un error. Codop %d. Cerrando conexion", cod_op);
				close(socket_ker);
				exit(1);
				break;
		}
    }
}

SuperBloque leer_superbloque(char* path_superBloque){
   //le paso al super bloque los valores y lo retorno
   t_config* config = config_create(path_superBloque);
   SuperBloque super_bloque;


   super_bloque.block_size = config_get_int_value(config, "BLOCK_SIZE");
   super_bloque.block_count = config_get_int_value(config, "BLOCK_COUNT");


   return super_bloque;
}

void crearArchivoBitmap(char* path_bitmap, int block_count) {
    if(access(path_bitmap, F_OK) == -1){
        int fd = open(path_bitmap, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
        if (fd == -1) {
            log_info(logger, "Error al crear el archivo de bitmap");
            exit(1);
        }
        //bitmap size cantidad de bytes
        bitmap.size = ceil(block_count/8);
        // Calcula el tamaño del bitmap en bytes

        // Establece el tamaño del archivo
        if (ftruncate(fd, bitmap.size) == -1) {
            log_info(logger, "Error al establecer el tamaño del archivo de bitmap");
            exit(1);
        }
        // Mapea el archivo a memoria
        void* datosBitmap = mmap(NULL, bitmap.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (datosBitmap == MAP_FAILED) {
            log_info(logger, "Error al mapear el archivo de bitmap");
            exit(1);
        }

        // Crea un bitarray utilizando los datos mapeados
        bitmap.data = bitarray_create_with_mode(datosBitmap, bitmap.size, LSB_FIRST);
        if (bitmap.data == NULL) {
            log_info(logger, "Error al crear el bitarray");
            exit(1);
        }
        // establezco todos los bits en 0
        for(int i=0; i<bitarray_get_max_bit(bitmap.data); i++){
            bitarray_clean_bit(bitmap.data, i);
        }

    }
    else{
        int fd = open(path_bitmap, O_RDWR);
        if (fd == -1) {
            log_info(logger, "Error al abrir el archivo de bitmap");
            exit(1);
        }

        // Calculo el tamaño del bitmap en bytes
        bitmap.size = ceil(block_count/8);

        // Mapeo el archivo a memoria
        void* datosBitmap = mmap(NULL, bitmap.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (datosBitmap == MAP_FAILED) {
            log_info(logger, "Error al mapear el archivo de bitmap");
            exit(1);
        }

        // Creo un bitarray utilizando los datos mapeados
        bitmap.data = bitarray_create_with_mode((char*)datosBitmap, bitmap.size, LSB_FIRST);
        if (bitmap.data == NULL) {
            log_info(logger, "Error al crear el bitarray");
            exit(1);
        }

    }
    
}

void* rdwrArchivoBloques(char* path_bloques, int block_count, int block_size) {
    off_t tamanioTotal = (off_t)block_count * block_size;
    if(access(path_bloques, F_OK)!=-1){// chequea si el archivo existe o hay que crearlo
    	//el archivo de bloques ya existe
        int fd = open(path_bloques, O_RDWR);
        if (fd == -1) {
            log_info(logger, "Error al abrir el archivo de bloques");
            exit(1);
        }
        if (ftruncate(fd, tamanioTotal) == -1) {
            log_info(logger, "Error al establecer el tamaño del archivo de bloques");
            exit(1);
        }
        void* datosBloques = mmap(NULL, tamanioTotal, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        return datosBloques;
    }
    else{
        //la misma logica para el caso de que el archivo no exista
        int fd = open(path_bloques, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
        if (fd == -1) {
            log_info(logger, "Error al crear el archivo de bloques");
            exit(1);
        }

        if (ftruncate(fd, tamanioTotal) == -1) {
            log_info(logger, "Error al establecer el tamaño del archivo de bloques");
            exit(1);
        }
        void* datosBloques = mmap(NULL, tamanioTotal, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        memset(datosBloques, '\0', tamanioTotal);
        return datosBloques;
    }
}

char* adressFromConfigName(char* fileConfigName){
	char* adress=string_duplicate(fsConfig.PATH_FCB);
	string_append(&adress, "/");
	string_append(&adress, fileConfigName);
	return adress;
}

//devuelve string con la ruta al archivo en el directorio de fcbs
char* adressFromName(char* filename){
	char* adress=string_duplicate(fsConfig.PATH_FCB);
	string_append(&adress, "/");
	string_append(&adress, filename);
	string_append(&adress, ".config");
	return adress;
}

//auxiliar de crearFCB, crea una nueva entrada en el directorio de los fcbs
void crearArchivoFCB(FCB* fcb){
	char* adress=adressFromName(fcb->nombre_archivo);
	//apertura si existe, creación sino. Append si ya existe
	open(adress, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
    t_config *new_config=config_create(adress);//config del archivo recien creado
    config_set_value(new_config, "NOMBRE_ARCHIVO", fcb->nombre_archivo);
    char* string_tamanio=string_itoa(fcb->tamanio);
    config_set_value(new_config, "TAMANIO_ARCHIVO", string_tamanio);
    config_set_value(new_config, "PUNTERO_DIRECTO", "-1");
    config_set_value(new_config, "PUNTERO_INDIRECTO", "-1");
    config_set_value(new_config, "CANTIDAD_INDIRECTOS", "0");

    config_save_in_file(new_config, adress);	//crea el archivo config en el adress
    config_destroy(new_config);
    free(string_tamanio);
    free(adress);
}

FCB* crearFCB(char *nombre_archivo){
    FCB* fcb = (FCB*)malloc(sizeof(FCB));
    //ESTOS DOS no apuntan a ningun bloque
    fcb->punteroIn = -1;
    fcb->punteroDir = -1;
    fcb->nombre_archivo=nombre_archivo;
    fcb->tamanio = 0;
    fcb->cantidad_indirectos=0;
    list_add(fs->listaFCB, fcb);
    fs->contadorFCB++;
    crearArchivoFCB(fcb);
    return fcb;
}

//esta se invoca al iniciar FS para cargar la lista de PCBs de fs
void inicializarListaFCB(char * directorioFCBs){
	fs->contadorFCB=0;
	fs->listaFCB=list_create();

	DIR* dir = opendir(directorioFCBs);
	if (dir == NULL) {
		mkdir(directorioFCBs, 0777); //escritura lectura y ejecución
		log_info(logger,"Creado el directorio de FCBs");
		return; //vuelvo porque el directorio está vacío
	}
    struct dirent* entry;
    //crea y asigna un PCB por cada archivo en el directorio
    while ((entry = readdir(dir)) != NULL) {
      	if(entry->d_type == DT_DIR){
      		continue;
        }
    	log_info(logger, "Entrada: %s", entry->d_name);
    	char* ruta=adressFromConfigName(entry->d_name);
    	t_config *un_config=config_create(ruta);
    	FCB* unFCB=(FCB*)malloc(sizeof(FCB));
    	unFCB->nombre_archivo=string_duplicate(config_get_string_value(un_config, "NOMBRE_ARCHIVO"));
    	unFCB->tamanio=config_get_int_value(un_config, "TAMANIO_ARCHIVO");
    	unFCB->punteroDir=config_get_int_value(un_config, "PUNTERO_DIRECTO");
    	unFCB->punteroIn=config_get_int_value(un_config, "PUNTERO_INDIRECTO");
    	config_destroy(un_config);
    	free(ruta);
    	list_add(fs->listaFCB, unFCB);
    	fs->contadorFCB++;
    }
    closedir(dir);
    log_info(logger, "Se cargó la lista de FCB's. <%d> entradas detectadas.", fs->contadorFCB);
}

//para guardar en el archivo del fcb los datos actualizados
void asentarFCB(FCB* fcb) {
	char* ruta_al_archivo=adressFromName(fcb->nombre_archivo);
	t_config *fcb_config=config_create(ruta_al_archivo);

	char* str_tamanio=string_itoa(fcb->tamanio);
	config_set_value(fcb_config, "TAMANIO_ARCHIVO", str_tamanio);
	char* str_directptr=string_itoa(fcb->punteroDir);
	config_set_value(fcb_config, "PUNTERO_DIRECTO", str_directptr);
	char* str_indirectptr=string_itoa(fcb->punteroIn);
	config_set_value(fcb_config, "PUNTERO_INDIRECTO", str_indirectptr);
	char* str_cantidad_ind=string_itoa(fcb->cantidad_indirectos);
	config_set_value(fcb_config, "CANTIDAD_INDIRECTOS", str_cantidad_ind);

	config_save(fcb_config);
	config_destroy(fcb_config);
}


void crearOAbrirArchivo(char* nombre_archivo){
	int rta=1;
	FCB* fcbPedido=encontrarFCB(nombre_archivo);
	if(fcbPedido==NULL){
		log_info(logger, "Crear Archivo: <%s>", nombre_archivo);
		crearFCB(nombre_archivo);
	}else{
		log_info(logger, "Abrir Archivo: <%s>", nombre_archivo);
	}
	send(socket_ker, &rta, sizeof(int), 0);
}

//TRUNCATE: agregar bloques al fcb si los necesita, o quitarselos si no los necesita
//IMPORTANTE: los cambios en el fcb tienen que actualizarse en el archivo de configuración correspondiente


void truncarArchivo(char* nombre_archivo, uint32_t nuevo_tamanio) {
    FCB* fcbPedido = encontrarFCB(nombre_archivo);
    if (fcbPedido != NULL) {
        int bloques_necesarios = (int)ceil(1.0*nuevo_tamanio/super_bloque.block_size);
        int cantidad_bloques_actuales = ceil(1.0*fcbPedido->tamanio / super_bloque.block_size);
        if(cantidad_bloques_actuales>bloques_necesarios){
        	//se pidió achicar el archivo
            int bloques_sobrantes= cantidad_bloques_actuales-bloques_necesarios;
            int i;
            for(i=0; i<bloques_sobrantes; i++){
                liberarUltimoBloqueFCB(fcbPedido);
            }
        }
        else{
            int i;
            int cant_bloques_faltantes=bloques_necesarios-cantidad_bloques_actuales;
            for(i=0; i<cant_bloques_faltantes; i++){
                int result=ocuparBloqueLibre(fcbPedido);
                if(result==-1){
                	log_error(logger, "Error al truncar el archivo <%s>", nombre_archivo);
                	send(socket_ker, &result, sizeof(int), 0);
                	return;
                }
            }
        }
        // actualizo el tamaño del archivo en el FCB
        fcbPedido->tamanio = nuevo_tamanio;
        

        // obtengo la direccion del archivo FCB
        char* adress = adressFromName(nombre_archivo);

        // abro el archivo FCB con la configuración existente
        t_config* archivo_config = config_create(adress);

        // actualizo el tamaño en el config
        char* string_tamanio = string_itoa(nuevo_tamanio);
        config_set_value(archivo_config, "TAMANIO_ARCHIVO", string_tamanio);

        // guardo los cambios en el config
        int rta=-1;
        if (config_save(archivo_config) != -1) {
            log_info(logger, "Truncar Archivo: <%s> - Tamaño: %d", nombre_archivo, nuevo_tamanio);
            rta=1;

        } else {

            log_error(logger, "Error al truncar el archivo <%s>", nombre_archivo);
        
        }
        send(socket_ker, &rta, sizeof(int), 0);
        // libero los recursos
        config_destroy(archivo_config);
        free(string_tamanio);
        free(adress);

    } else {

        log_error(logger, "No se encontró <%s> para truncar", nombre_archivo);
        int rta=2;
        send(socket_ker, &rta, sizeof(int), 0);
    }
}


void send_fwrite_petition(int realAddr, int bytes);


int block_pointer_from_number(FCB* fcb, int block_number){
	if(block_number==0){
		//primer bloque
		return fcb->punteroDir;
	}else if(block_number>0){
		//n bloque
		int* puntero_temp=(int*)leerBloque(fcb->punteroIn, sizeof(int)*(block_number-1), sizeof(int));
		int puntero=*puntero_temp;
		free(puntero_temp);
		return puntero;
	}else{
		perror("invalid block number");
		exit(1);
	}
}

void send_fwrite_petition(int realAddr, int bytes){
    t_paquete* paquete=crear_paquete_contexto(F_WRITE);

    paquete->buffer->stream = realloc(paquete->buffer->stream, paquete->buffer->size + 2*sizeof(int));

    memcpy(paquete->buffer->stream + paquete->buffer->size, &realAddr, sizeof(int));
    memcpy(paquete->buffer->stream + paquete->buffer->size + sizeof(int), &bytes, sizeof(int));

    paquete->buffer->size += 2*sizeof(int);

    enviar_paquete_contexto(paquete, socket_mem);
    eliminar_paquete(paquete);
}

void escribirArchivo(char* nombre_archivo, int realAddr, int bytes, int puntero){

    FCB* fcbPedido = encontrarFCB(nombre_archivo);
    int rta =-1;

    if(fcbPedido != NULL){

        log_info(logger, "Escribir Archivo: <%s> - Puntero: <%d> - Memoria: <%d> - Tamaño: <%d>", nombre_archivo, puntero, realAddr, bytes);
        send_fwrite_petition(realAddr, bytes);

        void* buffer=malloc(bytes);
        recv(socket_mem, buffer, bytes, MSG_WAITALL);

        int block_number=puntero/super_bloque.block_size;

        int block_pointer=block_pointer_from_number(fcbPedido, block_number);
        int offset = puntero%super_bloque.block_size;
        //con el offset (dentro del bloque) detecto si bytes excede del bloque
        if(offset+bytes>=super_bloque.block_size){
        	int exceded_bytes=offset+bytes-super_bloque.block_size+1;
        	//ejemplo 64 bloque, 60 offset+8 bytes, puedo escribir hasta el 63, me excedi 5 bytes
        	int scnd_block_pointer=block_pointer_from_number(fcbPedido, block_number+1);
        	int first_bytes=bytes-exceded_bytes;//a escribir en primer bloque
        	int scnd_bytes=exceded_bytes; //a escribir en el segundo bloque
        	void* first_buffer=malloc(first_bytes);
        	void* scnd_buffer=malloc(scnd_bytes);
        	memcpy(first_buffer, buffer, first_bytes);
        	memcpy(scnd_buffer, buffer+first_bytes, scnd_bytes);
        	int global_block_number=block_pointer/super_bloque.block_size;
        	log_info(logger, "Acceso Bloque - Archivo: <%s> - Bloque Archivo: <%d> - Bloque File System <%d>", nombre_archivo , block_number, global_block_number);
        	escribirBloque(block_pointer, offset, first_buffer, first_bytes);
        	int global_scndBlock_number=scnd_block_pointer/super_bloque.block_size;
        	log_info(logger, "Acceso Bloque - Archivo: <%s> - Bloque Archivo: <%d> - Bloque File System <%d>", nombre_archivo , block_number+1, global_scndBlock_number);
        	escribirBloque(scnd_block_pointer, 0, scnd_buffer, scnd_bytes);
        	free(first_buffer); free(scnd_buffer);
        }else{
        	//se escribe dentro del mismo bloque

            int global_block_number=block_pointer/super_bloque.block_size;
            log_info(logger, "Acceso Bloque - Archivo: <%s> - Bloque Archivo: <%d> - Bloque File System <%d>", nombre_archivo , block_number, global_block_number);
        	escribirBloque(block_pointer, offset, buffer, bytes);
        }
        rta =1;
        send(socket_ker, &rta, sizeof(int), 0);

    } else {

        log_error(logger, "No se encontró <%s>. Cancelando escritura.", nombre_archivo);
        send(socket_ker, &rta, sizeof(int), 0);

    }
}


void send_fread_petition(int realAddr, void* datos, int bytes){
    t_paquete* paquete=crear_paquete_contexto(F_READ);

    paquete->buffer->stream = realloc(paquete->buffer->stream, paquete->buffer->size + sizeof(int));

    memcpy(paquete->buffer->stream + paquete->buffer->size, &realAddr, sizeof(int));
    paquete->buffer->size += sizeof(int);

    agregar_a_paquete(paquete, datos, bytes);

    enviar_paquete_contexto(paquete, socket_mem);
    eliminar_paquete(paquete);
}

void leerArchivo(char* nombre_archivo, int realAddr, int bytes, int puntero) {

    FCB* fcbPedido = encontrarFCB(nombre_archivo);

    if (fcbPedido != NULL) {

        int block_number=puntero/super_bloque.block_size;
        //
        int block_pointer=block_pointer_from_number(fcbPedido, block_number);
        int inblock_offset=puntero%super_bloque.block_size;
        void* datos;
        if(inblock_offset+bytes>=super_bloque.block_size){
        //Me pidieron leer entre dos bloques
        	int scnd_block_pointer=block_pointer_from_number(fcbPedido, block_number+1);
        	int exceded_bytes=inblock_offset+bytes-super_bloque.block_size+1;
        	//ejemplo size 64, offset 60, bytes 8, me excedi 5 bytes porque se escribe hasta 63
        	int first_bytes=bytes-exceded_bytes;
        	int scnd_bytes=exceded_bytes;
        	log_info(logger, "Acceso Bloque - Archivo: <%s> - Bloque Archivo: <%d> - Bloque File System <%d>", nombre_archivo , block_number, block_pointer/super_bloque.block_size);
        	void* first_buffer=leerBloque(block_pointer, inblock_offset, first_bytes);
        	log_info(logger, "Acceso Bloque - Archivo: <%s> - Bloque Archivo: <%d> - Bloque File System <%d>", nombre_archivo , block_number+1, scnd_block_pointer/super_bloque.block_size);
        	void* scnd_buffer=leerBloque(scnd_block_pointer, 0, scnd_bytes);
        	datos=malloc(bytes);
        	memcpy(datos, first_buffer, first_bytes);
        	memcpy(datos+first_bytes, scnd_buffer, scnd_bytes);
        	free(first_buffer); free(scnd_buffer);
        }else{
        	//se lee dentro de un solo bloque
        log_info(logger, "Acceso Bloque - Archivo: <%s> - Bloque Archivo: <%d> - Bloque File System <%d>", nombre_archivo , block_number, block_pointer/super_bloque.block_size);
        datos=leerBloque(block_pointer, inblock_offset, bytes);


        }

        send_fread_petition(realAddr, datos, bytes);
        int buffer;
        recv(socket_mem, &buffer, sizeof(int), MSG_WAITALL);

        
        if (buffer != -1) {
            log_info(logger, "Leer Archivo: <%s> - Puntero: <%d> - Memoria: <%d> - Tamaño: <%d>", nombre_archivo, puntero, realAddr, bytes);
            int rta= 1;
            send(socket_ker, &rta, sizeof(int), 0);
        } else {
            log_info(logger, "No se pudo leer el archivo: <%s>", nombre_archivo);
            int rta= -1;
            send(socket_ker, &rta, sizeof(int), 0);
        }
        free(datos);
    } else {

        log_error(logger, "No se encontró <%s>. No se puede leer el archivo.", nombre_archivo);
    
    }
}



#endif
