//Comando para compilar: gcc -Wall -Wextra myshell.c libparser_64.a -static -o myshell.out

#include <sys/signal.h>
#include "parser.h"
#include <unistd.h>
#include <sys/fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>

int main(void) {
    //Variables necesarias para el buen funcionamiento del programa
	char buf[1024];
	tline * line;
    int file;
    int pid = 0;

	//Variables que almacenan tanto los PID como los mandatos para más tarde ser usados por JOBS y FG
	int vector_espera[1024];
    char** bufUno;
    //char** bufDos;
	int pids[100][100];

	//Variable dinamica bidimensional que almacena las PIPES
    int ** pipes;
    
	//Variables contador
	int contador=0;
    int contador2=0;
	int k=0;
    int n=0;

	int * esperar;

	//Redirecciones originales para ser restauradas más tarde
    int STDentrada = dup(fileno(stdin));
    int STDsalida = dup(fileno(stdout));
    int STDerror = dup(fileno(stderr));
    
	//Ignoracion de señales
	signal(SIGINT,SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    
	//Mensaje estandar del prompt
	printf("msh> ");

	//Inicializacion de memoria de los bufer de mandatos para JOBS y FG
    bufUno = calloc(100, sizeof(tline));
    //bufDos = calloc(100, sizeof(tline));
    
	//Inicio del programa
    while (fgets(buf, 1024, stdin)) {
		//Ignoramos las señales
        signal(SIGINT,SIG_IGN);
        signal(SIGQUIT, SIG_IGN);

		//Tokenizamos la entrada
        line = tokenize(buf);
        
		//Creamos dos variables: status almacena el estado de cada proceso; redBG almacena un FLAG que indica si el proceso se ejecuta en background
        int status[100];
        int backgroun = 0;

		//Si la linea está vacía continuar
        if (line==NULL) {
            continue;
        }
		//Si la redireccion de entrada no es NULA
        if (line->redirect_input != NULL) {
            printf("redirección de entrada: %s\n", line->redirect_input);
            file = open(line->redirect_input, O_RDONLY);//Abrimos un fichero, guardando su descriptor en "file"
            if (file == -1) { //Si el descriptor es -1 : error
                printf(line->redirect_input, "%s Error: ", strerror(errno));
                return 1;
            }else{
                dup2(file,STDIN_FILENO); //Si el descriptor no es -1 le duplicamos por la entrada estandar
            }
        }
        
		//Si la redireccion de salida no es NULA
        if (line->redirect_output != NULL) {
            printf("redirección de salida: %s\n", line->redirect_output);
            file = open(line->redirect_output, O_CREAT | O_RDWR); //Abrimos un fichero, guardando su descriptor en "file"
            if (file == -1) { //Si el descriptor es -1 : error
                printf(line->redirect_output, "%s Error: ", strerror(errno));
                return 1;
            }else{ //Si el descriptor no es -1 le duplicamos por la salida estandar
                dup2(file,fileno(stdout));
            }
        }
        
		//Si la redireccion de error no es NULA
        if (line->redirect_error != NULL) {
            printf("redirección de error: %s\n", line->redirect_error);
            file = creat(line->redirect_error, O_CREAT | O_RDWR);//Abrimos un fichero, guardando su descriptor en "file"
            if (file == -1) { //Si el descriptor es -1 : error
                printf(line->redirect_error, "%s Error: ", strerror(errno));
                return 1;
            }else{ //Si el descriptor no es -1 le duplicamos en el error
                dup2(file,STDERR_FILENO);
            }
        }
		//Si el comando debe ejecutarse en background
        if (line->background) {
            printf("comando a ejecutarse en background\n");
            backgroun = 1;//Activamos la FLAG
        }
        
		//Si solo introducimos un comando por consola
		if (line->ncommands==1) {
			//Y es CD
            if (strcmp(line->commands[0].argv[0],"cd") == 0) { //Si no añadimos argumentos al comando volvemos al directorio HOME
                if (line->commands[0].argv[1] == NULL) {
                    chdir(getenv("HOME"));
					printf("%s\n",getenv("HOME"));
                }else{
                    if (chdir(line->commands[0].argv[1]) != -1) { //Si introducimos un argumento y este existe, cambiamos el directorio
                        printf("Directorio Cambiado\n");
                        printf("%s\n",line->commands[0].argv[1]);
                    }else{//Si el directorio no existe
                        fprintf(stderr, "Directorio no encontrado\n");
                    }
                }
			//Y es JOBS
            }else if(strcmp(line->commands[0].argv[0],"jobs") == 0){ //Jobs
                k=0;
                n=0;
                for (int i=0; i<contador; i++) { //Este FOR recorre los PIDS y mandatos que se introducieron individualmente
                    if (waitpid(vector_espera[i], NULL, WNOHANG)==0) { //Si la orden WAITPID devuelve un 0 al darle el PID, el mandato seguirá ejecutándose
                        k++;
                        n++;
                        printf("[%d] Running    ", k);
                        printf("%s\n", bufUno[i]);
                    }
                }
				//N representa un FLAG que se mantiene en 0 si no hay ningun mandato ejecutandose aun, lo que provocara la limpieza de los arrays de mandatos y PIDS
                if(n==0){
                    contador=0;
                    int ent=sizeof(vector_espera);
                    for(int i=0 ; i <= ent; i++){
                        vector_espera[i] =0;
                    }
                }
                //DOS O + COMANDOS
                /*for (int i=0; i<contador2; i++) {
                    int p=0;
                    for (int j=0; j<8; j++) {
                        if (waitpid(pids[i][j], NULL, WNOHANG)==0) {
                            k++;
                            n++;
                            p++;
                        }
                    }
                    if (p!=0) {
                        printf("[%d] Running    ", k);
                        printf("%s\n", bufDos[i]);
                    }
                }*/
			//Y es FG
            }else if(strcmp(line->commands[0].argv[0],"fg") == 0){
                int flag=0;
                int ente = sizeof(vector_espera);
                for(int i=0;i<ente;i++){ //Recorremos el array que almacena los pids
                    if(line->commands[0].argv[1] != NULL){ //Si tiene argumento el comando
                        char numero = *line->commands[0].argv[1];
						if(i==numero){ //Si la iteracion actual es justo el numero de proceso que buscamos de la lista JOBS
							if(waitpid(vector_espera[i],NULL,WNOHANG)==0){ //Y si ese numero sigue activo, activamos el FLAG
								flag=i;
							}
						}
					}else{
						if(waitpid(vector_espera[i],NULL,WNOHANG)==0){ //Si algun pid de la lista sigue ejecutandose, se activa un FLAG
							flag=i;
						}
					}
				}
                if(flag!=0){ //Si el FLAG se activa se espera a que termine el proceso de manera activa
                    waitpid(vector_espera[flag],&status[flag],0);
                }
            }else{
				//Si es cualquier otro comando
                pid = fork();//Creamos un hijo
                vector_espera[contador] = pid; //Guardamos el pid del hijo en el array
                bufUno[contador] = strdup(buf); //Guardamos el mandato en el array
                contador++;
                
                if (pid<0) { //Si el PID es 0 error en la creacion del hijo
                    printf("%s", "Fallo en el fork, hijo no creado");
                }else if (pid==0){//Si el PID es distinto de 0
                    signal(SIGINT , SIG_DFL);
                    signal(SIGQUIT, SIG_DFL);//Restauramos las señales a su uso normal
                    if (line->commands[0].filename==NULL) {//Si no existe el mandato error
                        fprintf(stderr, "Comando invalido\n");
                        exit(1);
                    }else{//Si el comando existe se ejecuta el mandato junto a sus argumentos
                        execvp(line->commands[0].filename, line->commands[0].argv);
                    }
                    printf("%s", "Fallo en el hijo ejecucion comando");
                    exit(1);
                }else{
                    //printf("%s", "Padre");
                    if(backgroun==0){//Si el FLAG de ejecucion en background esta desactivamos esperamos activamente a que el comando termine
                        wait (&status[contador-1]);
                    }else{
                        waitpid(pids[contador-1][0], NULL, WNOHANG); //Si el FLAG esta activo, no esperamos activamente
                    }
                }
            }
        }else{ //Si son 2 o mas comandos
            bufUno[contador2] = strdup(buf); //Guardamos la linea completa de mandatos en el array
            contador2++;
	
            int com = line->ncommands; //Obtenemos el numero de mandatos
            pipes = (int **) malloc ((line->ncommands-1) * sizeof(int));//Asignamos memoria a la variable que gestiona los pipes
            for (int i=0; i<com-1; i++) {//Asignamos memoria a cada elemento de la variables pipes
                pipes[i] = (int *) malloc (2*sizeof(int));
                pipe(pipes[i]);
            }

			esperar = (int *) malloc ((line->ncommands) * sizeof(int));
			//Ejecutamos el for tantas veces como comandos existan
            for (int i=0; i<com; i++) {
                pid = fork(); //Creamos un hijo
                pids[contador2-1][i]=pid; //Guardamos el pid del hijo en el array

				if(pid!=0){
					esperar[i]=pid;
				}

				//Si el PID es menor que 0 error en la creacion del hijo
                if (pid <0) {
                    fprintf(stderr, "Fallo en el Fork()");
                    exit(-1);
                }else if (pid==0){ //Si el PID es 0
                    signal(SIGINT , SIG_DFL);
                    signal(SIGQUIT, SIG_DFL);//Restauramos las señales a su uso normal
                    if (line->commands[i].filename==NULL) {//Si el comando no existe
                        fprintf(stderr, "Comando invalido\n");
                        exit(1);
                    }else{//Si el comando existe
                        if (i==0) {//Primer hijo
                            close(pipes[i][0]); //Cerramos la salida del pipe
                            dup2(pipes[i][1], 1);//Duplicamos stdout en la entrada del pipe
                            execvp(line->commands[i].argv[0], line->commands[i].argv);//Ejecutamos el codigo
                            printf("El comando no ha funcionado correctamente");
                            exit(1);
                        }else if (i<com-1){//Un hijo intermedio
                            close(pipes[i-1][1]); //Cerramos la entrada de la pipe anterior
                            close(pipes[i][0]); //Cerramos la salida del pipe actual
                            dup2(pipes[i-1][0], 0); //Duplicamos la salida del pipe anterior en stdin
                            dup2(pipes[i][1], 1); //Duplicamos stdout en la entrada del pipe 
                            execvp(line->commands[i].argv[0], line->commands[i].argv);//Ejecutamos el codigo
                            printf("El comando no ha funcionado correctamente");
                            exit(1);
                        }else{//El ultimo hijo
                            close(pipes[i-1][1]); //Cerramos la entrada de la pipe anterior
                            dup2(pipes[i-1][0], 0);//Duplicamos la salida del pipe anterior en stdin
                            execvp(line->commands[i].argv[0], line->commands[i].argv);//Ejecutamos el codigo
                            printf("El comando no ha funcionado correctamente");
                            exit(1);
                        }
                    }
                }else{//Padre
                    if((i!=(line->ncommands-1))){
                        close(pipes[i][1]); //Cerramos las pipes
                    }
					
                }
            }

			for(int i=0;i<(int)(sizeof(esperar)/sizeof(int));i++){
				if (backgroun==0) {
					waitpid(esperar[i],NULL,0);
				}else{
					waitpid(esperar[i], NULL, WNOHANG);
				}
			}

			backgroun=0;


		}

		//Restauramos las redirecciones a las originales, guardadas al comienzo del programa
		dup2(STDentrada , fileno(stdin));
        dup2(STDsalida , fileno(stdout));
        dup2(STDerror , fileno(stderr));
        
        printf("msh> ");
    }  
		//Liberamos la memoria que ocupan las variables dinamicas
		free(pipes);
		free(bufUno);
}
