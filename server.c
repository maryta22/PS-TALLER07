#include <getopt.h>
#include <limits.h>
#include <sys/wait.h>
#include <pthread.h>
#include <syslog.h>
#include <fcntl.h>

#include "common.h"

void atender_cliente(int connfd);

void print_help(char *command)
{
	printf("Servidor simple de ejecución remota de comandos.\n");
	printf("uso:\n %s <puerto>\n", command);
	printf(" %s -h -d\n", command);
	printf("Opciones:\n");
	printf(" -h\t\t\tAyuda, muestra este mensaje\n");
	printf(" -d\t\t\tEjecuta daemon\n");
}

/**
 * Función que crea argv separando una cadena de caracteres en
 * "tokens" delimitados por la cadena de caracteres delim.
 *
 * @param linea Cadena de caracteres a separar en tokens.
 * @param delim Cadena de caracteres a usar como delimitador.
 *
 * @return Puntero a argv en el heap, es necesario liberar esto después de uso.
 *	Retorna NULL si linea está vacía.
 */
char **parse_comando(char *linea, char *delim)
{
	char *token;
	char *linea_copy;
	int i, num_tokens = 0;
	char **argv = NULL;

	linea_copy = (char *) malloc(strlen(linea) + 1);
	strcpy(linea_copy, linea);

	/* Obtiene un conteo del número de argumentos */
	token = strtok(linea_copy, delim);
	/* recorre todos los tokens */
	while( token != NULL ) {
		token = strtok(NULL, delim);
		num_tokens++;
	}
	free(linea_copy);

	/* Crea argv en el heap, extrae y copia los argumentos */
	if(num_tokens > 0){

		/* Crea el arreglo argv */
		argv = (char **) malloc((num_tokens + 1) * sizeof(char **));

		/* obtiene el primer token */
		token = strtok(linea, delim);
		/* recorre todos los tokens */
		for(i = 0; i < num_tokens; i++){
			argv[i] = (char *) malloc(strlen(token)+1);
			strcpy(argv[i], token);
			token = strtok(NULL, delim);
		}
		argv[i] = NULL;
	}

	return argv;
}

/**
 * Recoge hijos zombies...
 */
void recoger_hijos(int signal){
	while(waitpid(-1, 0, WNOHANG) >0);
	return;
}

/**
 * Recibe SIGINT, termina ejecución
 */
void salir(int signal){
	printf("BYE\n");
	exit(0);
}

//TODO:def *thread
void *thread(void *vargp);

//TODO dflag
bool dflag = false;

//#daemonize()
void daemonize ( char *nombre_programa){
	printf("Entrando modo daemon...\n");

	int fd0,fd1,fd2;
	pid_t pid;

	if ((pid = fork())<0){
		fprintf(stderr, "No es posible hacer un fork, error %s\n", strerror(errno));
		exit(1);
	}else if(pid != 0)
		exit(0);

	setsid();

	//cerrar solamente stdout, stdin y stderr
	close(0);
	close(1);
	close(2);

	//se deben abrir otra vez, porqur se podría abrir un archivo stdout, stdin o stderr
	//y los printf van a enviar a ese archivo
	fd0 = open("/dev/null", O_RDWR);
	fd1 = dup(fd0);
	fd2 = dup(fd0);

	//abrir un log para este daemin en el sistema syslog
	openlog(nombre_programa, LOG_CONS, LOG_DAEMON);
	if(fd0 != 0 || fd1 != 1 || fd2 !=2){
		syslog(LOG_ERR, "unexpected file descriptors %d %d %d", fd0, fd1, fd2);
		exit(1);
	}
}

int main(int argc, char **argv)
{
	int opt, index;

	//Sockets
	int listenfd, connfd, *connfdp;
	//unsigned int clientlen;
	//Direcciones y puertos
	struct sockaddr_in clientaddr;
	struct hostent *hp;
	char *haddrp, *port;

	while ((opt = getopt (argc, argv, "h")) != -1){
		switch(opt)
		{
			case 'h':
				print_help(argv[0]);
				return 0;
			case 'd':
				dflag = true;
				return 0;
			default:
				fprintf(stderr, "uso: %s <puerto>\n", argv[0]);
				fprintf(stderr, "     %s -h\n", argv[0]);
				return -1;
		}
	}

	/* Recorre argumentos que no son opción */
	for (index = optind; index < argc; index++)
		port = argv[index];

	if(argv == NULL){
		fprintf(stderr, "uso: %s <puerto>\n", argv[0]);
		fprintf(stderr, "     %s -h\n", argv[0]);
		return 1;
	}

	//Valida el puerto
	int port_n = atoi(port);
	if(port_n <= 0 || port_n > USHRT_MAX){
		fprintf(stderr, "Puerto: %s invalido. Ingrese un número entre 1 y %d.\n", port, USHRT_MAX);
		return 1;
	}

	//Registra funcion para recoger hijos zombies
	signal(SIGCHLD, recoger_hijos);

	//Registra funcion para señal SIGINT (Ctrl-C)
	signal(SIGINT, salir);

	//Abre un socket de escucha en port
	listenfd = open_listenfd(port);

	if(listenfd < 0)
		connection_error(listenfd);

	//if dflag
	if(dflag)
		daemonize(argv[0]);

	printf("server escuchando en puerto %s...\n", port);

	/*while (1) {
		clientlen = sizeof(clientaddr);
		connfd = accept(listenfd, (struct sockaddr *)&clientaddr, &clientlen);

		//El proceso hijo atiende al cliente
		if(fork() == 0){
			close(listenfd);
			// Determine the domain name and IP address of the client
			hp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr,
						sizeof(clientaddr.sin_addr.s_addr), AF_INET);
			haddrp = inet_ntoa(clientaddr.sin_addr);

			printf("server conectado a %s (%s)\n", hp->h_name, haddrp);
			atender_cliente(connfd);
			printf("server desconectando a %s (%s)\n", hp->h_name, haddrp);
			close(connfd);
			exit(0);
		}

		close(connfd);
	}*/

	//TODO: thread

		socklen_t clientlen = sizeof(struct sockaddr_in);
		pthread_t tid;

		if (argc!=2){
			fprintf(stderr, "usage: %s <port>\n", argv[0]);
			exit(0);
		}

		port = atoi(argv[1]);

		listenfd = open_listenfd(port);
		while(1){
			connfdp = malloc(sizeof(int));
			*connfdp = accept(listenfd, (struct sockaddr *) &clientaddr, &clientlen);
			pthread_create(&tid, NULL, thread, connfdp);
		}
}

//TODO: func *thread()
void *thread(void *vargp){
        int connfd = *((int *)vargp);
        pthread_detach(pthread_self());
        free(vargp);
        atender_cliente(connfd);
        close(connfd);
        return NULL;
}

void atender_cliente(int connfd)
{
	int n, status;
	char buf[MAXLINE] = {0};
	char **argv;
	pid_t pid;

	//Comunicación con cliente es delimitada con '\0'
	while(1){
		n = read(connfd, buf, MAXLINE);
		if(n <= 0)
			return;

		printf("Recibido: %s", buf);

		//Detecta "CHAO" y se desconecta del cliente
		if(strcmp(buf, "CHAO\n") == 0){
			write(connfd, "BYE\n", 5);
			return;
		}

		//Remueve el salto de linea antes de extraer los tokens
		buf[n - 1] = '\0';

		//Crea argv con los argumentos en buf, asume separación por espacio
		argv = parse_comando(buf, " ");

		if(argv){
			if((pid = fork()) == 0){
				dup2(connfd, 1); //Redirecciona STDOUT al socket
				dup2(connfd, 2); //Redirecciona STDERR al socket
				if(execvp(argv[0], argv) < 0){
					fprintf(stderr, "Comando desconocido...\n");
					exit(1);
				}
			}

			//Espera a que el proceso hijo termine su ejecución
			waitpid(pid, &status, 0);

			if(!WIFEXITED(status))
				write(connfd, "ERROR\n",7);
			else
				write(connfd, "\0", 1); //Envia caracter null para notificar fin

			/*Libera argv y su contenido
			para evitar fugas de memoria */
			for(int i = 0; argv[i]; i++)
				free(argv[i]);
			free(argv);

		}else{
			strcpy(buf, "Comando vacío...\n");
			write(connfd, buf, strlen(buf) + 1);
		}

		memset(buf, 0, MAXLINE); //Encera el buffer
	}
}
