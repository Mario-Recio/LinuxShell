#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "parser.h"

///Cabeceras de funcion//// 	
void terminar(int sig); 

void error(int sig);

void redireccionar(int sig);

void handlerhijosf(pid_t pidterm, int terminados);

void handlerhijosb(pid_t pidterm,int proceso, int hijo, pid_t ** hijosb, int * terminadosb);

void command(int posicion, int numcommands, int ** p,int fd0,int fd1,int fd2);

void cambiarentrada(int fd0, int numcommands,int desc[1024][3],int jsize);

void cambiarsalida(int fd1, int numcommands,int desc[1024][3],int jsize);

void cambiarerror(int fd2, int numcommands,int desc[1024][3],int jsize);
char * execcd();

void execjobs(char ** trabajos);

void execfg(int *idproceso, pid_t ** hijosb, int * terminadosb, int* numcommandsb, char ** trabajos,int desc[1024][3],int fd0,int fd1,int fd2);

void execumask();

void execexit(char * buf,char * pwd, pid_t ** hijosb, int * terminadosb, char * promt, char ** trabajos, int * numcommands, int * hijos,int ** p);

void checkbg(int *idproceso, void * contador, pid_t ** hijosb, int * terminadosb, int * numcommandsb, char ** trabajos);

int getCase(char * comando);

void quitarultimobg(pid_t ** hijosbg, int * idproceso, int * terminadosb, int * numcommandsb, char ** trabajos);

void quitarbg(pid_t ** hijosbg,int posterm, int * idproceso, int * terminadosb, int * numcommandsb, char ** trabajos);

///Variables globales////
tline * linea; ////Linea recibida de la entrada estandar tokenizada	
pid_t * hijos, *tmp_hijos; //Pid de los hijos en foreground
int jsize = 0; //Numero de trabajos en background
int desc[1024][3];//Array descriptores de ficheros
int fd0=-1,fd1=-1,fd2=-1;

int main(void){	
	///Variables locales////
	pid_t ** hijosb;//Pid hijos en background
	pid_t ** tmp_procesosb;//Variable para reasignar memoria
	pid_t * tmp_hijosb;//Variable para reasignar memoria
	pid_t pid; //Pid del hijo que se acaba de crear
	pid_t pidterm; //Variable para guardar el pid de forma temporal
	int * terminadosb; //Numero de hijos en bg terminados
	int * tmp_terminadosb; //Variable para guardar los hijos terminados de manera temporal
	int * numcommandsb; //Numero de comandos en bg del trabajo [x]
	int * tmp_numcommandsb; //Variable para reasignar memoria
	int numcommands; //Numero de comandos 
	char * promt; //Promt	
	char ** tmp_trabajos;//Variable para reasignar memoria
	char ** trabajos; //String de la cadena de mandatos
	char * pwd; //Directorio de trabajoa actual
	int ** p, **tmp_p; //Array de Pipes			
	int sizePr = 1024, sizeH = 1024, sizeC = 1024, sizeP=1024; //Tamaños	
	int contador = 0; //Contador
	int * idproceso, *tmp_idproceso; //Identificador de procesos en bg y reasignar memoria
	int i, j, z, numhijos = 0; //Contadores de los for
	char *buf; //Bufer para los comandos introducidos		
	bool comprobacion = true; //Boolean para comprobaciones
	
	
	///Señales////
			
	signal(SIGINT, terminar);
	
	signal(SIGUSR2, error);
	
	signal(SIGUSR1, redireccionar);
	
	///Reservas de memoria////
	
	hijos= (pid_t*) malloc(1024*sizeof(pid_t *));	
	
	promt=(char *) malloc(1024*sizeof(char *));	
		
	pwd=(char *) malloc(1024*sizeof(char *));
			
	p=(int **) malloc (1024*sizeof(int **));
	
	trabajos = (char **) malloc(1024*sizeof(char**));
	
	hijosb = (pid_t **) malloc(1024*sizeof(pid_t**));
	
	numcommandsb= (int *) malloc(1024*sizeof(int *));
	
	terminadosb = (int*) malloc(1024*sizeof(int*));
	
	idproceso = (int*) malloc(1024*sizeof(int*));
				
	buf = (char *) malloc(1024*sizeof(char *));
	//signal(SIGSTOP,detener);	
	
	///Promtp e inicio///
	strcpy(pwd, getenv("PWD"));
	strcpy(promt, strcat(pwd, "~msh:> "));
	printf("%s", promt); //Imprimir promp inicial
	
	//Bucle principal de la minishell
	while(fgets(buf, 1024*sizeof(char *), stdin)){		
		i = 0;
		comprobacion = true;
		linea = tokenize(buf); //Leer por entrada estandar y hacer tokenize			
		numcommands = linea->ncommands; //Guardar el número de comandos que se van a realizar
			
		//Comprobación que todos los nombres de los comandos sean válidos		
		if(numcommands==0){
			comprobacion=false;
		}else{ //comprueba que el comando es valido
			while(comprobacion && i < numcommands){
				comprobacion = linea->commands[i].filename != NULL;
				i++;
			}
		}			
		if(comprobacion){ 						
			//Reserva memoria e inicializa variables de background
			if(linea->background){
				hijosb[jsize] = (pid_t *) malloc(1024*sizeof(pid_t*));
				terminadosb[jsize]= *(int *) malloc(1024*sizeof(int));
				numcommandsb[jsize]=0;
				terminadosb[jsize] = 0; 
				trabajos[jsize] = (char*) malloc(1024*sizeof(char*));
				trabajos[jsize] = buf;
				contador++;
				idproceso[jsize] = contador;
				//Reserva mas memoria
				if(contador == sizeC){
					sizeC = sizeC+1024;						
					tmp_idproceso = realloc(idproceso,sizeC*sizeof(int *));
					idproceso = tmp_idproceso;									
				}					
			}
			if(numcommands>1){
				//Crea los pipes necesarios
				for (j = 0; j < numcommands-1; j++){ 							
					p[j]=(int *) malloc (2*sizeof(int *));
					p[j][0]=*(int *) malloc (1024*sizeof(int ));
					p[j][1]=*(int *) malloc (1024*sizeof(int ));
					pipe(p[j]);
						
					if(j==1023){
						sizeP=sizeP+1024;
					tmp_p=realloc(p,sizeP*sizeof(int**));				
						p=tmp_p;
					}						
				}
			}
			//Crear hijos y asignarlos al array					
			for (j = 0; j < numcommands; j++){ 				
				pid = fork(); 				
				if(pid < 0){
					fprintf(stderr,"Error al crear un hijo\n%s\n", strerror(errno));
					exit(1);											
				//Hijos
				}else if(pid == 0){ 					
					command(j, numcommands, p,fd0,fd1,fd2);																		
				}else{ //Padre		
					if(linea->background){
						hijosb[jsize][j] = *(pid_t *)malloc (1024*sizeof(pid_t));
						hijosb[jsize][j] = pid;
						numcommandsb[jsize]++;
						if(j==0){
							printf("[%d] %d\n", idproceso[jsize],pid);				}						
					}
					//Reserva mas memoria
					if(j == sizeH-1){
						sizeH = sizeH+1024;
						tmp_hijos=realloc(hijos,sizeH*sizeof(pid_t*));
						hijos=tmp_hijos;
							
						tmp_hijosb = realloc(hijosb[jsize],sizeH*sizeof(pid_t *));
								hijosb[jsize] = tmp_hijosb;
				tmp_numcommandsb=realloc(numcommandsb,sizeH*sizeof(int *));
								numcommandsb=tmp_numcommandsb;
					}
					hijos[j] = pid;
				}
			}
			if(linea->background){
				jsize++;
				//Reserva mas memoria
				if(jsize == sizePr){
					sizePr = sizePr+1024;
					tmp_terminadosb = realloc(terminadosb,sizePr*sizeof(int *));
					terminadosb = tmp_terminadosb;									
					tmp_trabajos = realloc(trabajos,sizePr*sizeof(char **));
					trabajos = tmp_trabajos;
					tmp_procesosb=  realloc(hijosb,sizePr*sizeof(pid_t**));
					hijosb = tmp_procesosb;
				}
			}							
			//Padre	
			if(pid != 0){
				//Cerrar pipes	
				for(j = 0; j < numcommands - 1; j++){
					close(p[j][0]);					 
					close(p[j][1]);	
				}			
				//Esperar a que todos los hijos terminen
				if(!linea->background){
					for(j = 0; j < numcommands; j++){ 						
						pidterm=waitpid(hijos[j], NULL,0);
						handlerhijosf(pidterm, 0);							
					}
				}
				//Formatea los pids
				for(j = 0; j < numcommands; j++){
					hijos[j] = -1;
				}
			}
		//Selecciona el comando interno
		}else if(numcommands>0){ 
			int a = getCase(linea->commands[i-1].argv[0]);
			switch(a){
					case 1:	 //cd					
						strcpy(pwd, execcd());						strcpy(promt,strcat(pwd,"~msh:> "));				
					break;
					
					case 2: //jobs
						execjobs(trabajos);
					break;
					
					case 3: //fg
						execfg(idproceso, hijosb, terminadosb, numcommandsb, trabajos, desc, fd0, fd1, fd2);
					break;
					
					case 4: //umask
						execumask();
					break;
					
					case 5: //exit
						execexit(buf, pwd, hijosb, terminadosb, promt, trabajos, numcommandsb,hijos,p);
					break;
					
					default:
						//El nombre de un comando no es correcto
						if(numcommands>0){
							printf("No se encuentra el mandato %s \n", linea->commands[i-1].argv[0]);	
						}								
			}			
		}		
		//Comprobar que mandatos en bg terminaron
		for(j = 0; j < jsize; j++){
			numhijos = numcommandsb[j];
			for(z = 0; z < numhijos; z++){
				pidterm = waitpid(hijosb[j][z], NULL, WNOHANG);
				handlerhijosb(pidterm, j, z, hijosb, terminadosb);	
			}
		}
		checkbg(idproceso, &contador, hijosb, terminadosb, numcommandsb, trabajos);
		//Reiniciar bufer
		buf = calloc(1024,sizeof(char *));
		//Volver a mostrar el promp
		linea = NULL;				
		printf("%s", promt);
	}
}

//Si ha terminado un hijo, aumenta el contador de hijos terminados
void handlerhijosf(pid_t pidterm, int terminados){
	if(pidterm > 0){		
			terminados++;
	}else{
		puts("Error al esperar a un hijo\n");
		exit(1);
	}	
}
//Si ha terminado un hijo en bg, aumenta el contador de hijos terminados para ese proceso
void handlerhijosb(pid_t pidterm, int proceso, int hijo, pid_t ** hijosb, int * terminadosb){
	if(pidterm != 0){			
		terminadosb[proceso]++;	
		hijosb[proceso][hijo] = -1;	
	}else if(pidterm < 0){
		puts("Error al esperar a un hijo\n");
		exit(1);	
	}
}

//Termina los hijos en fg
void terminar(int sig){
	int i = 0;
	if(linea != NULL && !linea->background && getpid() != 0){
		for(i; i < linea->ncommands; i++){	
			kill(hijos[i], SIGKILL);	
		}
	}
	return;
}

//Detener ejecucion del hijo por un error
void error(int sig){
	exit(1);
}

//Redirecciona las entradas y salidas estandar de nuevo a teclado y pantalla
void redireccionar(int sig){
	printf("why wont you work?!\n");
	dup2(0,fd0);																		
	dup2(1,fd1);																
	dup2(2,fd2);		
}

//Mandato cd
/*Si no hay argumentos, cambia el directorio al indicado por la variable de entorno HOME. 
Si hay un argumento, hace lo propio con el. 
Devuelve el directorio actual, se haya modificado o no*/
char * execcd(){	
	char * dir;
	dir=(char *) malloc(1024*sizeof(char *));
	if(linea->commands[0].argc == 1){				
		dir = getenv("HOME");
		if(dir == NULL){
			fprintf(stderr, "No existe la variable $HOME\n");
			free(dir);
			return getcwd(dir, 1024*sizeof(char*));
		}			
	}else if(linea->commands[0].argc == 2){
		strcpy(dir,linea->commands[0].argv[1]);			
	}else{
		fprintf(stderr, "Uso: %s optional<dir>\n", linea->commands[0].argv[0]);
		free(dir);
		return getcwd(dir, 1024*sizeof(char*));
	}
	
	if (chdir(dir) != 0) {
			fprintf(stderr, "No se puede cambiar de directorio %s\n", strerror(errno));  
			free(dir);
			return getcwd(dir, 1024*sizeof(char*));
	}else{				
		free(dir);
		return getcwd(dir, 1024*sizeof(char*));
	}	
}

//Mandato jobs
//Muestra los procesos en bg pendientes, contenidos en la variable trabajos
void execjobs(char ** trabajos){
	int i = 0;
	char * status;
	for(i; i < jsize; i++){
		printf("[%d]+  Running		%s", i + 1, trabajos[i]);	
	}
}


//Mandato fg
/*si no hay argumentos, pasa a esperar de forma bloqueante a todos los hijos sin terminar,del ultimo proceso en bg. 
si hay un argumento, se elimina el proceso con el identifiador correspondiente, si existe.
en ambos casos, se elimina el proceso de la lista de bg*/
void execfg(int *idproceso, pid_t ** hijosb, int * terminadosb, int * numcommandsb, char ** trabajos,int desc[1024][3],int fd0,int fd1,int fd2){	
	int status;
	int i;
	int id = 0;
	if(jsize == 0){
		puts("No hay ningun trabajo en background");
	}else{
		if(linea->commands[0].argc == 1){		
			printf("%s",trabajos[jsize-1]);
			for(i = 0; i < numcommandsb[jsize-1]; i++){
				//lo espera si no ha terminado			
				if(hijosb[jsize-1][i] != -1){				
					/*no hemos sido capaces de llamar a los hijos para
					hacer la redireccion, ya que creemos, que al haber empezado el exec,
					 ya no tienen la rutina de la señal. tampoco podemos decir al padre 
					 que cambie los descriptores de los hijos, que que no comparten la variable/*/
					//kill(hijosb[jsize-1][i],SIGUSR1);					
					waitpid(hijosb[jsize-1][i], NULL, 0);
				}
			}
			quitarultimobg(hijosb, idproceso, terminadosb, numcommandsb, trabajos);
		}else if(linea->commands[0].argc == 2){
			if(atoi(linea->commands[0].argv[1]) <= idproceso[jsize-1]){	
				 id = atoi(linea->commands[0].argv[1]) - 1;
				 printf("%s",trabajos[id]);
				 for(i = 0; i < numcommandsb[id]; i++){
				 	//lo espera si no ha terminado
					if(hijosb[id][i] != -1){
						waitpid(hijosb[id][i], NULL, 0);
					}
				}
				quitarbg(hijosb, id, idproceso, terminadosb, numcommandsb, trabajos);
			}else{
				fprintf(stderr, "No existe un proceso [%d] en background\n", atoi(linea->commands[0].argv[1]));
				return;
			}
		}
	}		
	if(linea->commands[0].argc > 2){
		fprintf(stderr, "Uso: %s <pid>\n", linea->commands[0].argv[0]);
		return;
	}
}	


//Mandato umask
/*comprueba que el numero es octal y entre 1-4 cifras
se ejecuta umask del argumento si lo hay. en caso contrario, muestra el umask con 4 cifras*/
void execumask(){
	int deffich=666,defcarp=777;
	int count=0;
	bool erroneo=false;
	int newmask;
	mode_t newmask2;
	if(linea->commands[0].argc==2){
		while(linea->commands[0].argv[1][count] && !erroneo){
			if(linea->commands[0].argv[1][count]<'0' || linea->commands[0].argv[1][count]>'7'){
				erroneo=true;	
			}
			count++;
		}
		if((strlen(linea->commands[0].argv[1])>=1 && strlen(linea->commands[0].argv[1])<=4) && !erroneo){
			newmask=atoi(linea->commands[0].argv[1]);			
			umask(newmask);
		}else{
			fprintf(stderr, "Uso: %s <3/4 digits octal number for permissions>\n", linea->commands[0].argv[0]);
		return;
		}
	}else{
		newmask2=umask(0);
		umask(newmask2);
		printf( "%.4d\n", newmask2);			
	}			
}

//Libera memoria
void execexit(char * buf, char * pwd, pid_t ** hijosb, int * terminadosb, char * promt,  char ** trabajos, int * numcommandsb, int * hijos,int ** p){
	int i = 0;
	printf("%s\n", promt);
	free(trabajos);
	free(hijosb);
	free(terminadosb);
	free(buf);
	free(promt);
	free(pwd);
	free(numcommandsb);
	free(hijos);
	free(p);
	exit(0);
}

//Selecciona el comando interno a ejecutar
int getCase(char * comando){
	if(strcmp(comando,"cd") == 0){	
		return 1;
	}else if(strcmp(comando,"jobs") == 0){	
		return 2;
	}else if (strcmp(comando,"fg") == 0){	
		return 3;
	}else if (strcmp(comando,"umask") == 0){
		return 4;
	}else if (strcmp(comando,"exit") == 0){
		return 5;
	}else{
		return-1;
	}
}

/*si la cantidad de hijos terminados de un proceso en background, es igual al numero de hijos que 
tenia, ese proceso a terminado y se elimina, mostrando su mensaje de "hecho". se comprueba al leer un mandato*/
void checkbg(int *idproceso, void * contador, pid_t ** hijosb, int * terminadosb, int * numcommandsb, char ** trabajos){
	int i = 0, completados = 0;
	int status;
	pid_t result;
	if(jsize > 0){
		while(i < jsize){							
			if(terminadosb[i] == numcommandsb[i]){				
				printf("[%d]+  Hecho		%s", idproceso[i], trabajos[i]);			
				quitarbg(hijosb, i, idproceso, terminadosb, numcommandsb, trabajos);
				//Vuelve a mirar en esa posición	
				i--;	
			}
			i++;			
		}		
	}
	if(jsize == 0){
		*(int *)contador = 0;
	}
}

/*elimina el utlimo proceso en bg, cambiando tambien las variables que
 guardan la información correspondiente a los procesos*/
void quitarultimobg(pid_t ** hijosb, int * idproceso, int * terminadosb, int * numcommandsb, char ** trabajos){
	int i=0,j=0;	
	//Se eliminan los hijos del proceso terminado
	for(j = 0; j < numcommandsb[jsize-1]; j++){
		hijosb[jsize-1][j] = -1;
	}

	terminadosb[jsize-1] = 0;
	numcommandsb[jsize-1] = 0;
	idproceso[jsize-1] = 0;
	trabajos[jsize-1] = "/0";
	
	jsize--;

}

/*elimina el proceso en bg identificado por la variable postem, cambiando tambien las variables que guardan la 
información correspondiente a los procesos*/
void quitarbg(pid_t ** hijosb, int posterm, int * idproceso, int * terminadosb, int * numcommandsb, char ** trabajos){
	int i;
	int j = 0;
	int limite;	
		//Se eliminan los hijos del proceso terminado
	
	for(i = posterm; i < jsize-1; i++){
		limite = numcommandsb[i+1];
		for(j = 0; j < numcommandsb[i]; j++){
			hijosb[i][j] = -1;
		}
		for(j = 0; j < limite; j++){
			hijosb[i][j] = hijosb[i+1][j];
		}
	}
	for(j = 0; j < numcommandsb[jsize-1]; j++){
		hijosb[jsize-1][j] = -1;
	}
	
	for(i = posterm; i < jsize-1; i++){
		//El contador de hijos del proceso terminado se elimina del array de terminados
		terminadosb[i] = terminadosb[i+1]; 
		numcommandsb[i] = numcommandsb[i+1];
		idproceso[i] = idproceso[i+1];
		trabajos[i] = trabajos[i+1];
	}
	terminadosb[jsize-1] = 0;
	numcommandsb[jsize-1] = 0;
	idproceso[jsize-1] = -1;
	trabajos[jsize-1] = "/0";
	jsize--;
}

//Redirecciona la entrada estandar a fichero
void cambiarentrada(int fd0, int numcommands, int desc[1024][3], int jsize){
	int i;
	//Hacer comprobacion de permisos					
	fd0 = open(linea->redirect_input,O_RDWR | O_APPEND, S_IRUSR | S_IWUSR);
	if(fd0 == -1){
		fprintf(stderr, "Error al abrir un fichero. Compruebe que el fichero existe o los permisos\n%s\n", strerror(errno));
		for(i = 0; i < numcommands; i++){
			kill(hijos[i], SIGUSR2);
		}
	}else{
		desc[jsize][0]=fd0;
		dup2(fd0, 0); //Redireccion de la entrada estandar
	}
}

//Redirecciona la salida estandar a fichero
void cambiarsalida(int fd1, int numcommands,int desc[1024][3],int jsize){
	int i;
	//Hacer comprobacion de permisos	
	fd1 = open(linea->redirect_output,O_CREAT | O_RDWR | O_APPEND, S_IRUSR | S_IWUSR);
	if(fd1 == -1){
		fprintf(stderr, "Error al abrir un fichero. Compruebe que el fichero existe o los permisos\n%s\n", strerror(errno));
	for(i = 0; i < numcommands; i++){
		kill(hijos[i], SIGUSR2);
	}
	}else{
		desc[jsize][1]=fd1;
		dup2(fd1, 1); //Redireccion del output
	}
}

//Redirecciona la salida estandar de error a fichero
void cambiarerror(int fd2, int numcommands,int desc[1024][3], int jsize){
	int i;
	fd2 = open(linea->redirect_error, O_CREAT | O_RDWR | O_APPEND, S_IRUSR | S_IWUSR);
	dup2(fd2, 2); //Redireccion del stderror
	desc[jsize][2]=fd2;
	if(fd2 == -1){
		fprintf(stderr, "Error al abrir un fichero. Compruebe que el fichero existe o los permisos\n%s\n", strerror(errno));
		for(i = 0; i < numcommands; i++){
			kill(hijos[i], SIGUSR2);
		}	
	}
}

/*cierra las pipes que cada hijo no va a usar. si es el primer mandato, desde la segunda al final, si es el ultimo, 
todas menos la ultima, y si es intermedio, las que esten antes y despues de sus adyacentes
comprueba redirecciones, configura las pipes que va a usar, y llama a exec*/
void command(int posicion, int numcommands, int ** p,int fd0, int fd1, int fd2){
	 int i; //Contador del for
	 if (linea->redirect_error != NULL) { //Comprobar que hay que cambiar el stderror
			cambiarerror(fd2, numcommands,desc,jsize);
	}		
	 if(posicion == 0 && numcommands > 1){//Multiples mandatos,Primer mandato			
		//Pipes
		close(p[posicion][0]); //Cerrar la lectura 			
		//Cerrar el resto de pipes
		for(i = posicion + 1; i < numcommands-1; i++){
			close(p[i][0]);
			close(p[i][1]);			
		}
		//Redireccion de la output
		
		dup2(p[posicion][1], 1);
					
		//Redireccion input
		if (linea->redirect_input != NULL) { //Comprobar que hay que cambiar input
			cambiarentrada(fd0, numcommands,desc,jsize);			
		}			
	}else if(posicion == numcommands - 1 && numcommands>1){ //Multiples comandos,Ultimo mandato			
		//Cerrar todas las pipes menos la ultima
		for(i = 0; i < posicion - 1; i++){
			close(p[i][0]);
			close(p[i][1]);			
		}			
		//Cerrar la escritura de la ultima pipe
		close(p[posicion-1][1]);			
		//Redireccion input
		dup2(p[posicion - 1][0], 0);
		//Redireccion output
		if (linea->redirect_output != NULL) { //Comprobar que hay que cambiar el output				
			cambiarsalida(fd1, numcommands,desc,jsize);				
		}
		//Redireccion del stderror	
				
	}else if(numcommands>1){ //Multiples mandatos,mandato intermedio
		//Cerrar todas las pipes anteriores
		for(i = 0; i < posicion - 1; i++){
			close(p[i][0]);
			close(p[i][1]);			
		}			
		close(p[posicion - 1][1]); //Cerrar escritura de a pipe anterior
		close(p[posicion][0]); //Cerrar lectura de la pipe
		//Cerrar todas las pipes posteriores
		for(i = posicion + 1; i < numcommands - 1; i++){
			close(p[i][0]);
			close(p[i][1]);			
		}		
		//Redireccion input
		dup2(p[posicion - 1][0], 0);
		//Redireccion output
		dup2(p[posicion][1], 1);					
	}else if (numcommands == 1){//un unico mandato
		if (linea->redirect_input != NULL) { 
			cambiarentrada(fd0, numcommands,desc,jsize);
		}
		if (linea->redirect_output != NULL) { 
			cambiarsalida(fd1, numcommands,desc,jsize);
		}
		if (linea->redirect_error != NULL) { 
			cambiarerror(fd2, numcommands,desc,jsize);
		}		
	}
 	
 	execv(linea->commands[posicion].filename, linea->commands[posicion].argv);
 	fprintf(stderr, "Error en el exec\n%s\n", strerror(errno));
 	exit(1);
}

