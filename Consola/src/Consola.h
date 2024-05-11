#define CONSOLA_H

#include <stdio.h>
#include "../../ganSOsLibrary/src/header.h"
#include <wchar.h>
t_log* logger;
#include <stdlib.h>

#define MAX_LINE_LENGTH 256

void paquete_instrucciones(int,int,char**);
void borrar_lista(char**, int);
void logear_instrucciones(char**, int , t_log*);
char** leer_pseudocodigo(char*, int*);

void terminar_programa(int conexion, t_log* logger, t_config* config) {
	close(conexion);
	log_destroy(logger);
	config_destroy(config);
}

// La funcion arma una lista dinamica de strings
char** leer_pseudocodigo(char *ruta_archivo, int *num_instrucciones) {
    // Abrimos el archivo de pseudocódigo para lectura
    FILE *archivo_pseudocodigo = fopen(ruta_archivo, "r");

    if (archivo_pseudocodigo == NULL) {
        printf("No se pudo abrir el archivo de pseudocódigo\n");
        exit(1);
    } else {log_info(logger, "Se abrió el archivo %s con éxito.", ruta_archivo);}
    // Leemos el archivo línea por línea y las almacenamos en una lista
    char linea[MAX_LINE_LENGTH];
    char** lista_instrucciones = NULL;
    *num_instrucciones=0;
    while (!feof(archivo_pseudocodigo)) {
    	fgets(linea, MAX_LINE_LENGTH, archivo_pseudocodigo);
        if(linea[0]=='\n'){
        	continue;
        }
        linea[strcspn(linea, "\n")] = '\0'; // parsea la linea
        // Arma la lista dinamica
        char *instruccion = malloc(strlen(linea) + 1);
        strcpy(instruccion, linea);
        lista_instrucciones = realloc(lista_instrucciones, sizeof(char *) * (*num_instrucciones + 1));
        lista_instrucciones[(*num_instrucciones)++] = instruccion;

    }

    fclose(archivo_pseudocodigo);
    return lista_instrucciones;
}

/*void logear_instrucciones(char** lista_instrucciones, int num_instrucciones,t_log* logger) {
    log_info(logger, "Cargue las siguientes instrucciones");
	for (int i = 0; i < num_instrucciones; i++) {
        if(lista_instrucciones[i] !=NULL) {
		log_info(logger, lista_instrucciones[i]);
        }
    }
}*/
// EN DESARROLLO arma el paquete con la lista de instrucciones
void paquete_instrucciones(int conexion,int num_instrucciones,char** lista) {

    t_paquete* paquete = crear_paquete();
    crear_buffer(paquete);
    //int* cantidad_instrucciones = malloc(sizeof(int));
    //memcpy(cantidad_instrucciones, &num_instrucciones, sizeof(int));
    //agregar_a_paquete(paquete, cantidad_instrucciones, sizeof(int));
    // CODOP SIZE [INT STRING INT STRING INT STRING]
    for (int i = 0; i < num_instrucciones; i++) {
        agregar_a_paquete(paquete, lista[i], strlen(lista[i]) + 1);
    }
    enviar_paquete(paquete, conexion);
    eliminar_paquete(paquete);
}


void borrar_lista(char** lista_instrucciones, int num_instrucciones) {
    if (lista_instrucciones != NULL) {
        for (int i = 0; i < num_instrucciones; i++) {
            if (lista_instrucciones[i] != NULL) {
                free(lista_instrucciones[i]);
            }
        }
        free(lista_instrucciones);
    }
}
