#include "header.h"

void saludar(){
	printf("%s", "Hola mundo! \n"); //de prueba jajaj
}

t_config* iniciar_config(char* conf)
{
	t_config* nuevo_config=config_create(conf);

	return nuevo_config;
}

int iniciar_servidor(char* puertoEscucha)
{
	int socket_servidor;

	struct addrinfo hints, *servinfo;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC; //
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	getaddrinfo(NULL, puertoEscucha, &hints, &servinfo);

	// Creamos el socket de escucha del servidor
	socket_servidor = socket(servinfo->ai_family,
						servinfo->ai_socktype,
						servinfo->ai_protocol);

	// Asociamos el socket a un puerto
	bind(socket_servidor, servinfo->ai_addr, servinfo->ai_addrlen);
	// Escuchamos las conexiones entrantes
	listen(socket_servidor, SOMAXCONN);

	freeaddrinfo(servinfo);
	//log_debug(logger, "Listo para escuchar a mi cliente");

	return socket_servidor;
}

int esperar_cliente(int socket_servidor)
{
	// Aceptamos un nuevo cliente
	int socket_cliente = accept(socket_servidor, NULL, NULL);
	log_info(logger, "Conexion recibida a través del socket %d", socket_cliente);
	return socket_cliente;
}

int recibir_operacion(int socket_cliente)
{
	int cod_op;
	if(recv(socket_cliente, &cod_op, sizeof(int), MSG_WAITALL) > 0)
		return cod_op;
	else
	{
		return -1;
	}
}

t_list* recibir_paquete(int socket_cliente)
{
	int size; //tamaño del stream
	int desplazamiento = 0;
	void * buffer; //stream
	t_list* valores = list_create();
	int tamanio;//tamaño del siguiente mensaje dentro del stream

	buffer = recibir_buffer(&size, socket_cliente); //buffer contiene t_buffer.size y t_buffer.stream, en orden y contiguo
	while(desplazamiento < size)	//Este while no seria necesario si la cantidad de valores es determinada
	{
		memcpy(&tamanio, buffer + desplazamiento, sizeof(int));
		desplazamiento+=sizeof(int);
		char* valor = malloc(tamanio);	//Asume que todos los valores son char*
		memcpy(valor, buffer+desplazamiento, tamanio);
		desplazamiento+=tamanio;
		list_add(valores, valor);
	}
	free(buffer);
	return valores;
}

void* recibir_buffer(int* size, int socket_cliente)
{
	void * buffer;

	recv(socket_cliente, size, sizeof(int), MSG_WAITALL);
	buffer = malloc(*size);
	recv(socket_cliente, buffer, *size, MSG_WAITALL);

	return buffer;
}

char* recibir_mensaje(int socket_cliente)
{
	int size;
	char* buffer = recibir_buffer(&size, socket_cliente);
	return buffer;
}

void enviar_mensaje_razon(int codop, char* mensaje, int socket_cliente)
{
	t_paquete* paquete = malloc(sizeof(t_paquete));

	paquete->codigo_operacion = codop;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = strlen(mensaje) + 1;
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream, mensaje, paquete->buffer->size);

	int bytes = paquete->buffer->size + 2*sizeof(int);

	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
	eliminar_paquete(paquete);
}

void enviar_mensaje(char* mensaje, int socket_cliente)
{
	t_paquete* paquete = malloc(sizeof(t_paquete));

	paquete->codigo_operacion = MENSAJE;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = strlen(mensaje) + 1;
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream, mensaje, paquete->buffer->size);

	int bytes = paquete->buffer->size + 2*sizeof(int);

	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
	eliminar_paquete(paquete);
}

int socket_create(char* ip, char* puerto){
	struct addrinfo hints;
	struct addrinfo *server_info;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	getaddrinfo(ip, puerto, &hints, &server_info);

	int created_socket = socket(server_info->ai_family,
	                    server_info->ai_socktype,
	                    server_info->ai_protocol);

	return created_socket;
}

void paquete(int conexion)
{
	// Ahora toca lo divertido!
	char* leido=NULL;
	t_paquete* paquete=NULL;
	printf("Los mensajes se agregaran a un paquete a enviar. Ingresar vacío para cortar.\n");
	// Leemos y esta vez agregamos las lineas al paquete
	paquete = crear_paquete(); //paquete ahora tiene opcode=PAQUETE, size=0, puntero a stream=NULL
		while(1){
				leido = readline("> ");
				if(!(strcmp(leido, ""))){
					free(leido);
					break;
				}
				agregar_a_paquete(paquete, leido, strlen(leido));//actualiza size de stream y el stream
				free(leido);									//agrega [size, stream] contiguamente a lo ya escrito
		}
	enviar_paquete(paquete, conexion);
	eliminar_paquete(paquete);
	// ¡No te olvides de liberar las líneas y el paquete antes de regresar!

}

t_list* inicializarListaSockets(){
	t_list* connectedSockets=list_create();
	return connectedSockets;
}

void enviarIdentidad(modulo ident, int sock_comm)
{
	t_paquete* paquete=NULL;
	//modulo *mensaje= &ident;
	paquete = malloc(sizeof(t_paquete));
	paquete->codigo_operacion = IDENTIDAD;
	//crear_buffer(paquete); //paquete ahora tiene opcode=IDENTIDAD, size=0, puntero a stream=NULL
	//agregar_a_paquete(paquete, &ident, sizeof(int));
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size =sizeof(int);
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream, &ident, paquete->buffer->size);

	enviar_paquete(paquete, sock_comm);
	eliminar_paquete(paquete);
}

int recibirIdentidad(int socket_cliente)
{
	//DEBE HABERSE HECHO UN RECIBIR_OPERACION ANTES DE USAR ESTO, ASUMIMOS QUE CODOP YA SE LEYÓ CORRECTAMENTE
	int size; //tamaño del stream
	int valor; //ya se que voy a recibir un int, el dato modulo
	int *buffer = recibir_buffer(&size, socket_cliente); //buffer contiene stream, información util. Tamaño de ese stream está en size
	valor=*buffer;
	free(buffer);
	return valor;
}

void addConnectionToList(t_list* listaSockets, modulo ident, int socket_comm){
	structIdent *nodo=malloc(sizeof(structIdent));
	nodo->identidad=ident;
	nodo->socketConn=socket_comm;
	list_add(listaSockets, nodo); //la memoria se libera en deleteConnectionFromList (OPCIONAL) o en liberarListaSockets (OBLIGATORIO)
}

modulo deleteConnectionFromList(t_list* listaSockets, int socket_comm){
	bool condition(structIdent* nodo){
		return nodo->socketConn==socket_comm;
	}
	structIdent *nodo=list_remove_by_condition(listaSockets, (void*)(condition));
	free(nodo);
	return nodo->identidad;
}

void liberarListaSockets(t_list* lista){
	void element_destroyer(structIdent *nodo){
		free(nodo);
	}
	list_destroy_and_destroy_elements(lista, (void*)element_destroyer);
}

modulo identidadDelSocket(t_list *lista, int socket){
	bool condition(structIdent* nodo){
		return nodo->socketConn==socket;
	}
	structIdent *nodo=list_find(lista, (void*)(condition));
	return nodo->identidad;
}

t_paquete* crear_paquete(void)
{
	t_paquete* paquete = malloc(sizeof(t_paquete));
	paquete->codigo_operacion = PAQUETE;
	crear_buffer(paquete);
	return paquete;
}

t_paquete* crear_paquete_contexto(int motivo){
	t_paquete* paquete = malloc(sizeof(t_paquete));
	paquete->codigo_operacion = motivo;
	crear_buffer(paquete);
	return paquete;
}

void crear_buffer(t_paquete* paquete)
{
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = 0;
	paquete->buffer->stream = NULL;
}

void empaquetar_lista(t_paquete* paquete, t_list* lista){//cantInstr-tamaño-instr-tamaño-instr...
	paquete->buffer->stream = realloc(paquete->buffer->stream, paquete->buffer->size + sizeof(int));
	int tamanioLista=list_size(lista);
	memcpy(paquete->buffer->stream+paquete->buffer->size, &tamanioLista, sizeof(int));
	paquete->buffer->size += sizeof(int);
	for(int i=0; i<tamanioLista; i++){
		char* elemento=(char*)list_remove(lista, 0);
		agregar_a_paquete(paquete, elemento, strlen(elemento)+1);
	}list_destroy(lista);
}

void empaquetar_registros(t_paquete* paquete, char* registros[]){
	for(int i=0; i<12; i++){
		agregar_a_paquete(paquete, registros[i], strlen(registros[i])+1);
		free(registros[i]);
	}
}

void agregar_segmento_a_paquete(t_paquete* paquete, Segmento* sgm){
	paquete->buffer->stream = realloc(paquete->buffer->stream, paquete->buffer->size + 3*sizeof(int));

	memcpy(paquete->buffer->stream + paquete->buffer->size, &(sgm->ID), sizeof(int));
	memcpy(paquete->buffer->stream + paquete->buffer->size + sizeof(int), &(sgm->base), sizeof(int));
	memcpy(paquete->buffer->stream + paquete->buffer->size + 2*sizeof(int), &(sgm->limite), sizeof(int));
	paquete->buffer->size += 3* sizeof(int);
}

void empaquetar_lista_segmentos(t_paquete* paquete, t_list* lista){//cantInstr-tamaño-instr-tamaño-instr...
	paquete->buffer->stream = realloc(paquete->buffer->stream, paquete->buffer->size + sizeof(int));
	int tamanioLista=list_size(lista);
	memcpy(paquete->buffer->stream+paquete->buffer->size, &tamanioLista, sizeof(int));
	paquete->buffer->size += sizeof(int);
	for(int i=0; i<tamanioLista; i++){
		Segmento* elemento=(Segmento*)list_get(lista, i);
		agregar_segmento_a_paquete(paquete, elemento);
	}
}

void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio)
{
	paquete->buffer->stream = realloc(paquete->buffer->stream, paquete->buffer->size + tamanio + sizeof(int));

	memcpy(paquete->buffer->stream + paquete->buffer->size, &tamanio, sizeof(int)); //agrega el tamaño de valor
	memcpy(paquete->buffer->stream + paquete->buffer->size + sizeof(int), valor, tamanio);	//contiguamente, copia el valor

	paquete->buffer->size += tamanio + sizeof(int);
}

int enviar_paquete(t_paquete* paquete, int socket_cliente)
{
	int bytes = paquete->buffer->size + 2*sizeof(int); //porque al stream le va a agregar el opcode(int) y el bufferSize(int)
	void* a_enviar = serializar_paquete(paquete, bytes);

	int checkout=send(socket_cliente, a_enviar, bytes, 0); //a_enviar es el stream final con opcode, t_buffer.size, t_buffer.stream contiguos, en ese orden
							// bytes es el tamaño del stream final
	free(a_enviar); //porque serializar hace malloc para copiar codOp y t_buffer en una misma dirección
	return checkout;
}

int send_newsegment_petition(int socket_dest, int PID, int segID, int size){
	t_paquete* paquete= crear_paquete_contexto(CREATE_SEGMENT);
	paquete->buffer->stream = realloc(paquete->buffer->stream, paquete->buffer->size + 3*sizeof(int));

	memcpy(paquete->buffer->stream + paquete->buffer->size, &PID, sizeof(int));//proceso que solicita
	memcpy(paquete->buffer->stream + paquete->buffer->size + sizeof(int), &segID, sizeof(int));//ID del segmento a crear
	memcpy(paquete->buffer->stream + paquete->buffer->size+ 2*sizeof(int), &size, sizeof(int));//tamaño del segmento

	paquete->buffer->size += 3*sizeof(int);
	int bytes_enviados=enviar_paquete(paquete, socket_dest);//+CodOp paquete y size
	eliminar_paquete(paquete);
	return bytes_enviados;
}

void recv_newsegment_petition(int socket_escucha, int* PID, int* segID, int* size){
	int buffSize;
	void* buffer = recibir_buffer(&buffSize, socket_escucha);
	int desplazamiento=0;
	memcpy(PID, buffer + desplazamiento, sizeof(int));
	desplazamiento+=sizeof(int);
	memcpy(segID, buffer + desplazamiento, sizeof(int));
	desplazamiento+=sizeof(int);
	memcpy(size, buffer + desplazamiento, sizeof(int));
	desplazamiento+=sizeof(int);
	free(buffer);
}

int send_tabla_segmentos(int socket_dest, t_list* tabla_segmentos, int rflag){
	t_paquete* paquete=crear_paquete_contexto(rflag);

	empaquetar_lista_segmentos(paquete, tabla_segmentos);

	int bytes_enviados=enviar_paquete(paquete, socket_dest);//+CodOp paquete y size
	eliminar_paquete(paquete);
	return bytes_enviados;
}

t_list* recv_tabla_segmentos(int socket_escucha, int* rflag){
	t_list* tabla_segmentos = list_create();
	int size, codOp;
	int cantSegmentos;
	int desplazamiento=0;
	int ID, base, limite; //se cargan en cada iteracion
	codOp = recibir_operacion(socket_escucha);
	*rflag=codOp;// deberia ser 0, -1 o 1
	void* buffer = recibir_buffer(&size, socket_escucha);
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
	free(buffer);
	return tabla_segmentos;
}

int send_deletesegment_petition(int socket_dest, int PID, int segID){
	t_paquete* paquete= crear_paquete_contexto(DELETE_SEGMENT);
	paquete->buffer->stream = realloc(paquete->buffer->stream, paquete->buffer->size + 2*sizeof(int));

	memcpy(paquete->buffer->stream + paquete->buffer->size, &PID, sizeof(int));//proceso que solicita
	memcpy(paquete->buffer->stream + paquete->buffer->size + sizeof(int), &segID, sizeof(int));//ID del segmento a crear

	paquete->buffer->size += 2*sizeof(int);
	int bytes_enviados=enviar_paquete(paquete, socket_dest);//+CodOp paquete y size
	eliminar_paquete(paquete);
	return bytes_enviados;
}

void recv_deletesegment_petition(int socket_escucha, int* PID, int* segID){
	int buffSize;
	void* buffer = recibir_buffer(&buffSize, socket_escucha);
	int desplazamiento=0;
	memcpy(PID, buffer + desplazamiento, sizeof(int));
	desplazamiento+=sizeof(int);
	memcpy(segID, buffer + desplazamiento, sizeof(int));
	desplazamiento+=sizeof(int);
	free(buffer);
}

void* serializar_paquete(t_paquete* paquete, int bytes)
{
	void * magic = malloc(bytes);
	int desplazamiento = 0;

	memcpy(magic + desplazamiento, &(paquete->codigo_operacion), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(magic + desplazamiento, &(paquete->buffer->size), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(magic + desplazamiento, paquete->buffer->stream, paquete->buffer->size);
	desplazamiento+= paquete->buffer->size;

	return magic;
}

int enviar_paquete_contexto(t_paquete* paquete, int socket_cliente)
{
	int bytes = paquete->buffer->size + 2*sizeof(int); //porque al stream le va a agregar el opcode(int) y el bufferSize(int)
	void* a_enviar = serializar_paquete(paquete, bytes);
	int check=send(socket_cliente, a_enviar, bytes, 0); //a_enviar es el stream final con opcode, t_buffer.size, t_buffer.stream contiguos, en ese orden
							// bytes es el tamaño del stream final
	free(a_enviar); //porque serializar hace malloc para copiar codOp y t_buffer en una misma dirección
	return check;
}

//devuelve la cantidad de bytes enviados por send() o -1 si falló el envío
int paquete_contexto(int socket_dest, execContext contexto ,int motivo){
	t_paquete* paquete=crear_paquete_contexto(motivo);

	agregar_a_paquete(paquete, &contexto.PID, sizeof(int));
	empaquetar_lista(paquete, contexto.instrucciones);
	agregar_a_paquete(paquete, &contexto.program_counter, sizeof(int));
	empaquetar_registros(paquete, contexto.registros);
	empaquetar_lista(paquete, contexto.parametros);
	empaquetar_lista_segmentos(paquete, contexto.tablaSegmentos);

	int bytes_enviados=enviar_paquete_contexto(paquete, socket_dest);
	eliminar_paquete(paquete);
	return bytes_enviados;
}

execContext recibir_paquete_contexto(int socket_cliente){
	int size; //tamaño del stream
	int desplazamiento = 0;
	void * buffer; //stream
	t_list* instrucciones = list_create();
	t_list* parametros = list_create();
	int programCounter, PID;
	execContext retVal;
	//tablaSegmentos tabla;
	int tamanio;//tamaño del siguiente mensaje dentro del stream
	int cantInstrucciones, cantParametros;

	buffer = recibir_buffer(&size, socket_cliente);

	memcpy(&tamanio, buffer+desplazamiento, sizeof(int));
	desplazamiento+=sizeof(int);
	memcpy(&PID, buffer+desplazamiento, tamanio);
	desplazamiento+=sizeof(int);

	memcpy(&cantInstrucciones, buffer+desplazamiento, sizeof(int));
	desplazamiento+=sizeof(int);
	for(int i=0; i<cantInstrucciones; i++){
		memcpy(&tamanio, buffer + desplazamiento, sizeof(int));
		desplazamiento+=sizeof(int);
		char* instr=malloc(tamanio);
		memcpy(instr, buffer+desplazamiento, tamanio);
		list_add(instrucciones, instr);
		desplazamiento+=tamanio;
	}

	memcpy(&tamanio, buffer+desplazamiento, sizeof(int));
	desplazamiento+=sizeof(int);
	memcpy(&programCounter, buffer+desplazamiento, tamanio);
	desplazamiento+=sizeof(int);

	for(int i=0; i<CANTIDAD_REGISTROS_CPU; i++){
		memcpy(&tamanio, buffer+desplazamiento, sizeof(int));
		desplazamiento+=sizeof(int);
		char* reg=malloc(tamanio);
		memcpy(reg, buffer+desplazamiento, tamanio);
		retVal.registros[i]=reg;
		desplazamiento+=tamanio;
	}

	memcpy(&cantParametros, buffer+desplazamiento, sizeof(int));
	desplazamiento+=sizeof(int);
	for(int i=0; i<cantParametros; i++){
		memcpy(&tamanio, buffer + desplazamiento, sizeof(int));
		desplazamiento+=sizeof(int);
		char* param=malloc(tamanio);
		memcpy(param, buffer+desplazamiento, tamanio);
		list_add(parametros, param);
		desplazamiento+=tamanio;
	}

	int cantSegmentos, ID, base, limite;
	//RECIBIR Tabla de segmentos/archivos abiertos
	t_list* tabla_segmentos=list_create();
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

	free(buffer);

	retVal.PID=PID;
	retVal.instrucciones=instrucciones;
	retVal.program_counter=programCounter;
	retVal.parametros=parametros;
	retVal.tablaSegmentos=tabla_segmentos;
	return retVal;
}

int crear_conexion(char *ip, char* puerto)
{
	struct addrinfo hints;
	struct addrinfo *server_info;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	getaddrinfo(ip, puerto, &hints, &server_info);
	// Ahora vamos a crear el socket.
	int socket_cliente = socket_create(ip, puerto);

	// Ahora que tenemos el socket, vamos a conectarlo
	int check=connect(socket_cliente, server_info->ai_addr, server_info->ai_addrlen);

	freeaddrinfo(server_info);

	return check!=0?-1:socket_cliente;
}

void eliminar_paquete(t_paquete* paquete)
{
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}


void iterator(char* value) {	//usada para mostrar paquete de mensajes
	log_info(logger,"%s", value);
}

int select_socketCalling(int socket_srv, fd_set current_sockets){
	fd_set ready_sockets;
	FD_ZERO(&current_sockets);
	FD_SET(socket_srv, &current_sockets);
	while(1){
		ready_sockets = current_sockets; //current_sockets tiene el socket de escucha y luego almacena los demas sockets.
		if(select(FD_SETSIZE, &ready_sockets, NULL, NULL, NULL)<0){	//ready_sockets es copia es current, porque select -
			perror("select error");									// - destruye la lista que le ingresa por el 2do parametro
			exit(EXIT_FAILURE);
		}
		for(int i=0; i<FD_SETSIZE; i++){
			if(FD_ISSET(i, &ready_sockets)){
				if(i==socket_srv){
					int client_socket=esperar_cliente(socket_srv); //SE BLOQUEA ACA si ningun otro socket tiene algo que hacer
					FD_SET(client_socket, &current_sockets);
				}else{
					return i; //Entra acá si el socket i tiene algo que hacer
					//Una vez atendida la petición del socket i se debe correr FD_CLR(i, &current_sockets);
				}
			}
		}
	}
}
