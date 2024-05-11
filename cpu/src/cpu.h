
#include "../../ganSOsLibrary/src/header.h"

/*REGISTROS DE CPU
Registros de 4 bytes: AX, BX, CX, DX.
Registros de 8 bytes: EAX, EBX, ECX, EDX
Registros de 16 bytes: RAX, RBX, RCX, RDX
 */
struct cpuConfig configData;
int socket_kernel, socket_memoria, socket_cpu;
execContext contexto;
const int SET_ID = 0;
const int YIELD_ID = 1;
const int EXIT_ID = 2;
const int I_O_ID = 3;
const int WAIT_ID = 4;
const int SIGNAL_ID = 5;
const int MOV_IN_ID = 6;
const int MOV_OUT_ID = 7;
const int F_OPEN_ID = 8;
const int F_CLOSE_ID = 9;
const int F_SEEK_ID = 10;
const int F_READ_ID = 11;
const int F_WRITE_ID = 12;
const int F_TRUNCATE_ID = 13;
const int CREATE_SEGMENT_ID = 14;
const int DELETE_SEGMENT_ID = 15;

//lista_registros usar solo para instanciar el diccionario_reg por prolijidad
//Registro es un alias para char*
Registro lista_registros[CANTIDAD_REGISTROS_CPU];

//lista_tokens usar solo para instanciar el diccionario_reg por prolijidad
char* lista_tokens[CANTIDAD_REGISTROS_CPU]=
{"AX","BX", "CX","DX","EAX","EBX","ECX","EDX","RAX","RBX","RCX","RDX"};

int MMU(int logicAddr, int offset, int* limiteSeg);

void inicializarRegistros(){
	//siempre se reserva un byte de más para el caracter núlo.
	for(int i=0; i<4;i++){
		lista_registros[i]=(char*)malloc(4+1);
		strcpy(lista_registros[i], "0000");

	}
	for(int i=4; i<8;i++){
		lista_registros[i]=(char*)malloc(8+1);
		strcpy(lista_registros[i], "00000000");
	}
	for(int i=8; i<12;i++){
		lista_registros[i]=(char*)malloc(16+1);
		strcpy(lista_registros[i], "0000000000000000");
	}
}

//Esta estructura se podrá utilizar para consultar un registro por su token, "AX", "BX"
t_dictionary *diccionario_reg;
void inicializarDiccionarioReg(){
	for(int i=0; i<CANTIDAD_REGISTROS_CPU; i++){
		dictionary_put(diccionario_reg, lista_tokens[i], lista_registros[i]);
	}
}

struct cpuConfig{
	char * PUERTO_ESCUCHA;
	int RETARDO_INSTRUCCION;
	char * PUERTO_MEMORIA;
	int TAM_MAX_SEGMENTO;
	char * IP_MEMORIA;
};

struct cpuConfig cargarConfigData(t_config* config){
	struct cpuConfig configData;
	configData.IP_MEMORIA = config_get_string_value(config, "IP_MEMORIA");
	configData.PUERTO_ESCUCHA = config_get_string_value(config, "PUERTO_ESCUCHA");
	configData.TAM_MAX_SEGMENTO = config_get_int_value(config, "TAM_MAX_SEGMENTO");
	configData.RETARDO_INSTRUCCION = config_get_int_value(config, "RETARDO_INSTRUCCION");
	configData.PUERTO_MEMORIA = config_get_string_value(config, "PUERTO_MEMORIA");
	return configData;
}

t_dictionary *diccionario_functionsId;
void inicializarDiccionarioFunId(){
	dictionary_put(diccionario_functionsId, "SET", (void*)&SET_ID);
	dictionary_put(diccionario_functionsId, "YIELD", (void*)&YIELD_ID);
	dictionary_put(diccionario_functionsId, "EXIT", (void*)&EXIT_ID);
	dictionary_put(diccionario_functionsId, "I/O", (void*)&I_O_ID);
	dictionary_put(diccionario_functionsId, "WAIT", (void*)&WAIT_ID);
	dictionary_put(diccionario_functionsId, "SIGNAL", (void*)&SIGNAL_ID);
	dictionary_put(diccionario_functionsId, "MOV_IN", (void*)&MOV_IN_ID);
	dictionary_put(diccionario_functionsId, "MOV_OUT", (void*)&MOV_OUT_ID);
	dictionary_put(diccionario_functionsId, "F_OPEN", (void*)&F_OPEN_ID);
	dictionary_put(diccionario_functionsId, "F_CLOSE", (void*)&F_CLOSE_ID);
	dictionary_put(diccionario_functionsId, "F_SEEK", (void*)&F_SEEK_ID);
	dictionary_put(diccionario_functionsId, "F_READ", (void*)&F_READ_ID);
	dictionary_put(diccionario_functionsId, "F_WRITE", (void*)&F_WRITE_ID);
	dictionary_put(diccionario_functionsId, "F_TRUNCATE", (void*)&F_TRUNCATE_ID);
	dictionary_put(diccionario_functionsId, "CREATE_SEGMENT", (void*)&CREATE_SEGMENT_ID);
	dictionary_put(diccionario_functionsId, "DELETE_SEGMENT", (void*)&DELETE_SEGMENT_ID);
}

void logging_instrucciones(char* token, t_list* parametros){
	int cant_instr=list_size(parametros);
	char* cadena_parametros=string_new();
	for(int i=0; i<cant_instr; i++){
		string_append(&cadena_parametros, " ");
		string_append(&cadena_parametros, (char*)list_get(parametros, i));
	}log_info(logger, "PID: <%d> - Ejecutando %s%s", contexto.PID,token ,cadena_parametros);
}

bool interrupted;
void exit_process_error(char* mensaje_de_error);
//si hay overflow, devuelve la cantidad de bytes excedidos
// si no se encuentra el registro, devuelve -1
//sino, devuelve 0
int set(char* regName, char* valor){
	Registro reg=dictionary_get(diccionario_reg, regName);
	if(reg==NULL){
		char* mensaje_error=string_from_format("SET: No se encontró el registro %s", regName);
		exit_process_error(mensaje_error);
		return -1;
	}
	if(strlen(valor)<=strlen(reg)){
		char*cadenaLimpia=string_repeat('0', strlen(reg));
		strcpy(reg, cadenaLimpia); //se sobreescribe el contenido anterior del registro con "0s"
		strncpy(reg, valor, strlen(valor));
		//sleep en milisegundos por archivo de configuracion
		struct timespec ts;
		int sleep_time=configData.RETARDO_INSTRUCCION;
		ts.tv_sec = sleep_time / 1000; //milisegundos a segundos
		ts.tv_nsec = (sleep_time % 1000) * 1000000; //milisegundos a nanosegundos
		nanosleep(&ts, NULL);//duerme, reteniendo al PCB durante 'time' milisegundos
		free(cadenaLimpia);
	}
	return strlen(valor)<=strlen(reg)?0:strlen(valor)-strlen(reg);
}

void set_sin_retardo(char* regName, char* valor){
	Registro reg=dictionary_get(diccionario_reg, regName);
	if(strlen(valor)<=strlen(reg)){
		char*cadenaLimpia=string_repeat('0', strlen(reg));
		strcpy(reg, cadenaLimpia); //se sobreescribe el contenido anterior del registro con "0s"
		strncpy(reg, valor, strlen(valor));
		//sleep en milisegundos por archivo de configuracion
		free(cadenaLimpia);
	}
}

//funcion que se corre antesde cada ciclo
void cargarRegistros(char* registrosContext[]){
	for(int i=0; i<CANTIDAD_REGISTROS_CPU; i++){
		set_sin_retardo(lista_tokens[i], registrosContext[i]);
	}
}

//función que se corre dentro de cada operación que devuelva el contexto a kernel
void cargarRegistrosContexto(char* registrosContext[]){
	for(int i=0;i<CANTIDAD_REGISTROS_CPU; i++){
		strcpy(registrosContext[i], lista_registros[i]);
	}
}


void yield(){
	cargarRegistrosContexto(contexto.registros);
	//tabla de segmentos? tabla de archivos abiertos?
	int check_value=paquete_contexto(socket_kernel, contexto, YIELD);
	if(check_value>0){
		log_info(logger, "Se devolvió contexto de ejecución por YIELD.");
	}else{
		log_error(logger, "YIELD no pudo devolver el contexto de ejecución.");
	}
}

void exit_process_error(char* mensaje_de_error){
	void liberador(char* cadena){
		free(cadena);
	}
	list_clean_and_destroy_elements(contexto.parametros, (void*)liberador);
	list_add(contexto.parametros, mensaje_de_error);
	int check_value=paquete_contexto(socket_kernel, contexto, ERROR);
	if(check_value>0){
		log_warning(logger, "Se devolvió contexto por ERROR - %s", mensaje_de_error);
	}else{
		log_error(logger, "No se pudo devolver el contexto de ejecución por motivo ERROR.");
	}
	interrupted=true;
}

void exit_process(){
	cargarRegistrosContexto(contexto.registros);
	//tabla de segmentos? tabla de archivos abiertos?
	int check_value=paquete_contexto(socket_kernel, contexto, EXIT);
	if(check_value>0){
		log_info(logger, "Se devolvió contexto de ejecución por EXIT.");
	}else{
		log_error(logger, "EXIT no pudo devolver el contexto de ejecución.");
	}
}

//EJECUTAR I/O WAIT SIGNAL. "interpretar" todas las demas

void i_o(char* tiempoBloqueo){ //importante: se maneja el tiempo como un string. Kernel lo traducirá a Int
	cargarRegistrosContexto(contexto.registros);
	list_add(contexto.parametros, tiempoBloqueo);
	int check_value=paquete_contexto(socket_kernel, contexto, I_O);
	if(check_value>0){
		log_info(logger, "Se devolvió contexto de ejecución por I/O.");
	}else{
		log_error(logger, "I/O no pudo devolver el contexto de ejecución.");
	}
}

void wait_process(char* recurso){
	cargarRegistrosContexto(contexto.registros);
	t_list* parametros=list_create();
	list_add(parametros, recurso);
	contexto.parametros=parametros;

	int check_value=paquete_contexto(socket_kernel, contexto, WAIT);
	if(check_value>0){
		log_info(logger, "Petición de Wait enviada a Kernel");
	}else{
		log_error(logger, "no se pudo enviar señal WAIT.");
	}
}

void signal_process(char* recurso){
	cargarRegistrosContexto(contexto.registros);
	t_list* parametros=list_create();
	list_add(parametros, recurso);
	contexto.parametros=parametros;

	int check_value=paquete_contexto(socket_kernel, contexto, SIGNAL);
	if(check_value>0){
		log_info(logger, "Petición de Signal enviada a Kernel");
	}else{
		log_error(logger, "no se pudo enviar señal SIGNAL.");
	}
}

void send_movin_petition(int addr, int bytes){
	t_paquete* paquete=crear_paquete_contexto(MOV_IN);

	paquete->buffer->stream = realloc(paquete->buffer->stream, paquete->buffer->size + 2*sizeof(int));

	memcpy(paquete->buffer->stream + paquete->buffer->size, &addr, sizeof(int));
	memcpy(paquete->buffer->stream + paquete->buffer->size + sizeof(int), &bytes, sizeof(int));

	paquete->buffer->size += 2*sizeof(int);

	enviar_paquete_contexto(paquete, socket_memoria);
	eliminar_paquete(paquete);
}

void mov_in(char* registro, char* memAddr){
	//tamaño de registro?
	Registro reg=dictionary_get(diccionario_reg, registro);
	int bytes=strlen(reg); //recordar, el elemento en memoria no tiene caracter nulo

	//dirección fisica?
	int limiteSeg;
	int physicAddr=MMU(atoi(memAddr), bytes, &limiteSeg);
	if(physicAddr==(-1)){
		int tam_max_segmento=configData.TAM_MAX_SEGMENTO;
		int num_segmento = floor(atoi(memAddr) / tam_max_segmento);
		char* mensaje_error=string_from_format("MOV_IN: SEG_FAULT - Segmento: <%d> - Offset: <%d> - Tamaño: <%d>", num_segmento, limiteSeg, bytes);
		exit_process_error(mensaje_error);
		return;
	}
	//efectúo MOV_IN
	send_movin_petition(physicAddr, bytes);
	char*leido=(char*)malloc(sizeof(bytes)+1);
	recv(socket_memoria, leido, bytes, 0);
	leido[bytes]='\0';
	//LOGGING
	int tam_max_segmento=configData.TAM_MAX_SEGMENTO;
	int num_segmento = floor(atoi(memAddr) / tam_max_segmento);
	log_info(logger, "PID: <%d> - Acción: <LECTURA> - Segmento: <%d> - Dirección Física: <%d> - Valor: <%s>", contexto.PID, num_segmento,physicAddr, leido);
	set(registro, leido);

}

void send_movout_petition(int addr, void* valor, int tamanio){
	t_paquete* paquete=crear_paquete_contexto(MOV_OUT);

	paquete->buffer->stream = realloc(paquete->buffer->stream, paquete->buffer->size + sizeof(int));

	memcpy(paquete->buffer->stream + paquete->buffer->size, &addr, sizeof(int));
	paquete->buffer->size += sizeof(int);

	agregar_a_paquete(paquete, valor, tamanio);

	enviar_paquete_contexto(paquete, socket_memoria);
	eliminar_paquete(paquete);
}

void mov_out(char* registro, char* memAddr){

	//tamaño de registro?
	Registro reg=dictionary_get(diccionario_reg, registro);
	if(reg==NULL){
		char* mensaje_error=string_from_format("MOV_OUT: No se encontró el registro %s", registro);
		exit_process_error(mensaje_error);
		return;
	}

	int realOffset=strlen(reg); //eliminaré el caracter nulo

	//dirección fisica?
	int limiteSeg;
	int physicAddr=MMU(atoi(memAddr), realOffset, &limiteSeg);
	if(physicAddr==(-1)){
		int tam_max_segmento=configData.TAM_MAX_SEGMENTO;
		int num_segmento = floor(atoi(memAddr) / tam_max_segmento);
		char* mensaje_error=string_from_format("MOV_OUT: SEG_FAULT - Segmento: <%d> - Offset: <%d> - Tamaño: <%d>", num_segmento, limiteSeg, realOffset);
		exit_process_error(mensaje_error);
		return;
	}
	void* mensaje=malloc(realOffset);
	memcpy(mensaje, reg, realOffset);
	send_movout_petition(physicAddr, mensaje, realOffset);
	free(mensaje);
	int result;
	recv(socket_memoria, &result, sizeof(int), 0);
	if(result==1){
		int tam_max_segmento=configData.TAM_MAX_SEGMENTO;
		int num_segmento = floor(atoi(memAddr) / tam_max_segmento);
		log_info(logger, "PID: <%d> - Acción: <ESCRITURA> - Segmento: <%d> - Dirección Física: <%d> - Valor: <%s>", contexto.PID, num_segmento,physicAddr, reg);
	}
}

void f_open(char* fileName){
	cargarRegistrosContexto(contexto.registros);
	t_list* parametros=list_create();
	list_add(parametros, fileName);
	contexto.parametros=parametros;
	//tabla de segmentos? tabla de archivos abiertos?
	int check_value=paquete_contexto(socket_kernel, contexto, F_OPEN);
	if(check_value>0){
		log_info(logger, "Enviada petición F_OPEN: %s a Kernel", fileName);
	}else{
		log_error(logger, "no se pudo enviar señal F_OPEN.");
	}
}

void f_close(char* fileName){
	cargarRegistrosContexto(contexto.registros);
	t_list* parametros=list_create();
	list_add(parametros, fileName);
	contexto.parametros=parametros;
	//tabla de segmentos? tabla de archivos abiertos?
	int check_value=paquete_contexto(socket_kernel, contexto, F_CLOSE);
	if(check_value>0){
		log_info(logger, "Enviada petición F_CLOSE: %s a Kernel", fileName);
	}else{
		log_error(logger, "no se pudo enviar señal F_CLOSE.");
	}
}

void f_seek(char* fileName, char* position){
	cargarRegistrosContexto(contexto.registros);
	t_list* parametros=list_create();
	list_add(parametros, fileName);
	list_add(parametros, position);
	contexto.parametros=parametros;
	//tabla de segmentos? tabla de archivos abiertos?
	int check_value=paquete_contexto(socket_kernel, contexto, F_SEEK);
	if(check_value>0){
		log_info(logger, "Enviada petición F_SEEK %s to %s a Kernel", fileName, position);
	}else{
		log_error(logger, "no se pudo enviar señal F_SEEK.");
	}
}

void f_read(char* fileName, char* logicAddr, char* bytes){
	cargarRegistrosContexto(contexto.registros);
	t_list* parametros=list_create();
	list_add(parametros, fileName);

	int limiteSeg;
	int physicAddr=MMU(atoi(logicAddr), atoi(bytes), &limiteSeg);
	if(physicAddr==(-1)){
		int tam_max_segmento=configData.TAM_MAX_SEGMENTO;
		int num_segmento = floor(atoi(logicAddr) / tam_max_segmento);
		char* mensaje_error=string_from_format("F_READ: SEG_FAULT - Segmento: <%d> - Offset: <%d> - Tamaño: <%d>", num_segmento, limiteSeg, atoi(bytes));
		exit_process_error(mensaje_error);
		return;
	}
	char* physicAddrString=string_itoa(physicAddr);
	list_add(parametros, physicAddrString);
	list_add(parametros, bytes);
	contexto.parametros=parametros;
	//tabla de segmentos? tabla de archivos abiertos?
	int check_value=paquete_contexto(socket_kernel, contexto, F_READ);
	if(check_value>0){
		log_info(logger, "Enviada petición F_READ %s, %s bytes to %s a Kernel ", fileName, bytes, logicAddr);
	}else{
		log_error(logger, "no se pudo enviar señal F_READ.");
	}
}

void f_write(char* fileName, char* logicAddr, char* bytes){
	cargarRegistrosContexto(contexto.registros);
	t_list* parametros=list_create();
	list_add(parametros, fileName);

	int limiteSeg;
	int physicAddr=MMU(atoi(logicAddr), atoi(bytes), &limiteSeg);
	if(physicAddr==(-1)){
		int tam_max_segmento=configData.TAM_MAX_SEGMENTO;
		int num_segmento = floor(atoi(logicAddr) / tam_max_segmento);
		char* mensaje_error=string_from_format("F_WRITE: SEG_FAULT - Segmento: <%d> - Offset: <%d> - Tamaño: <%d>", num_segmento, limiteSeg, atoi(bytes));
		exit_process_error(mensaje_error);
		return;
	}
	char* physicAddrString=string_itoa(physicAddr);
	list_add(parametros, physicAddrString);
	list_add(parametros, bytes);
	contexto.parametros=parametros;
	//tabla de segmentos? tabla de archivos abiertos?
	int check_value=paquete_contexto(socket_kernel, contexto, F_WRITE);
	if(check_value>0){
		log_info(logger, "Enviada petición F_WRITE %s, %s bytes from %s a Kernel ", fileName, bytes, logicAddr);
	}else{
		log_error(logger, "no se pudo enviar señal F_WRITE.");
	}
}

void f_truncate(char* fileName, char* newSize){
	cargarRegistrosContexto(contexto.registros);
	t_list* parametros=list_create();
	list_add(parametros, fileName);
	list_add(parametros, newSize);
	contexto.parametros=parametros;
	//tabla de segmentos? tabla de archivos abiertos?
	int check_value=paquete_contexto(socket_kernel, contexto, F_TRUNCATE);
	if(check_value>0){
		log_info(logger, "Enviada petición F_TRUNCATE %s to %s bytes a Kernel", fileName, newSize);
	}else{
		log_error(logger, "no se pudo enviar señal F_TRUNCATE.");
	}
}

void create_segment(char* segmentId, char* size){
	cargarRegistrosContexto(contexto.registros);
/*
	int numberSize=atoi(size);
	if(numberSize>configData.TAM_MAX_SEGMENTO){
		char* mensaje_error=string_from_format("CREATE_SEGMENT: %d supera tamaño maximo", numberSize);
		exit_process_error(mensaje_error);
		return;
	}
*/
	t_list* parametros=list_create();
	list_add(parametros, segmentId);
	list_add(parametros, size);
	contexto.parametros=parametros;

	int check_value=paquete_contexto(socket_kernel, contexto, CREATE_SEGMENT);
	if(check_value>0){
		log_info(logger, "Petición de Create_Segment enviada a Kernel");
	}else{
		log_error(logger, "no se pudo enviar señal Create_Segment.");
	}
}

void delete_segment(char* segmentId){

	cargarRegistrosContexto(contexto.registros);
		t_list* parametros=list_create();
		list_add(parametros, segmentId);
		contexto.parametros=parametros;
		//tabla de segmentos? tabla de archivos abiertos?
		int check_value=paquete_contexto(socket_kernel, contexto, DELETE_SEGMENT);
		if(check_value>0){
			log_info(logger, "Petición de Delete_Segment enviada a Kernel");
		}else{
			log_error(logger, "no se pudo enviar señal Delete_Segment.");
		}
}

//si se excede el segmento indicado por dirección logica, devuelve -1
int MMU(int logicAddr, int offset, int*miBase){
	int tam_max_segmento=configData.TAM_MAX_SEGMENTO;
	int num_segmento = floor(logicAddr / tam_max_segmento);
	int desplazamiento=logicAddr % tam_max_segmento;
	*miBase=desplazamiento;
	bool es_el_segmento(Segmento *a){
		return a->ID==num_segmento;
	}
	Segmento* mi_segmento=list_find(contexto.tablaSegmentos, (void*)es_el_segmento);
	if(mi_segmento==NULL){
		return -1;
	}
	int base=mi_segmento->base;
	int limite=mi_segmento->limite;

	if(base+desplazamiento+offset>base+limite){
		return -1;
	}

	int physicAddr=base+desplazamiento;
	return physicAddr;
}





