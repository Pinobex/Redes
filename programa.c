/*
Aplicação de um chat p2p - Trabalho da disciplina Redes de Computadores - SSC0641 - 29/06/15.
Professor Julio Cezar Estrella.
Estagiário PAE Carlos Henrique Gomes Ferreira.
 
Alunos: Rodrigo Oliveira 8006522
Sérgio Afonso Baptista Junior 7987219

*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h> 
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <syscall.h>
#include <fcntl.h>	


#define BFMSG 512
#define MAXCONTATOS 20
//#define PORT  5050 Comentar para testar em um mesmo computador. Pois quando for aplicado em outros
//computadores o programa podem ter a mesma porta como base.
#define PRINCIPAL 0
#define CONEXAO 1
#define OFF 0
#define ON 1

/* Aqui temos uma struct que vai conter os dados de um servidor para ser inicializado.
Estamos usando uma struct pois para inicializar uma thread podermos passar os dados
de uma unica vez*/
struct opServidor{
	int porta;
	int tipo;
	int num;
};
/*Aqui o analogo para o cliente.*/
struct opCliente{
	int porta;
	int tipo;
	int num;
	char ip[BFMSG];
};


void iniciarServidor(int porta,int tipo,int num);
void iniciarCliente(int porta,char ip[BFMSG],int tipo,int num);  


/*Aqui temos todos os 6 semaforos do tipo mutex que serão usados
nas funções que existe uma variavel compartilhada entre as threads.*/
pthread_mutex_t s1=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t s2=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t s3=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t s4=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t s5=PTHREAD_MUTEX_INITIALIZER;

/*Nesse ponto temos a declaração do vetor que vai armazenar se temos uma conexão aberta (ON)
no número dela, que a posição do vetor.E abaixo temos a matriz que vai armazenar o IP do contato
apatir do numero da conexão que ele é. */
int numeroConexao[MAXCONTATOS];
char contatos[MAXCONTATOS][BFMSG];

/*Temos aqui e outra parte importante, que é o buffer de envio de mensagem, toda mensagem que for enviada
vai ser armazenada nele.E abaixo temos outro vetor que vai definir apartir da posição dele,se ela for
igual a ON,a conexão deve enviar a mensagem contida no buffer.*/
char bufferEnvio[BFMSG];
int auxEnvio[MAXCONTATOS];

/*Esse vetor segue o mesmo raciocinio dos outros já citados. Quando a conexão, que é definida pela posição
do vetor, for igual a ON, a conexão deve ser fechado. */
int auxFechar[MAXCONTATOS];

int PORT=0;//so para teste interno
int aux=0;

/*variaveis auxiliares para o armazenamento dos dados.*/
int auxCliente=0;
int auxPorta=0;
char auxIP[BFMSG];
int auxNum;

/*Função usada pelos clientes para usar a variavel auxiliar auxCliente, que
tem a finaliade de avisar a thread que cria um novo cliente, está dentro de uma 
função para ser usado o semaforo pois a variavel uma variavel compartilhada.*/
void auxCriarCliente(int i){
	pthread_mutex_lock(&s1);
	auxCliente=i;
	pthread_mutex_unlock(&s1);
}
/*Função que coloca nas variaveis auxiliares os valores para criação de um novo cliente,
está dentro de uma função por causa dos semaforos */
void criarCliente(int j,char i[BFMSG],int k){
	pthread_mutex_lock(&s2);
	auxPorta=j;
	strcpy(auxIP,i);
	auxNum=k;
	pthread_mutex_unlock(&s2);
}
/* Funçao que faz, ou pelomenos tenta, deixar um socket ser bloqueante.*/
int naoBloqueante(int sock) {
	int flags, status;
  
  	flags = fcntl(sock, F_GETFL, 0);
  	if (flags == -1) {
    	return -1;
  	}
  	flags |= O_NONBLOCK;
	status = fcntl (sock, F_SETFL, flags);
  	if (status == -1) {
    	return -1;
  	}
  return 0;
}
/* Função que apartir do número da conexão faz ele ficar disponivel para receber novas conexões.
Existe nela também o uso do semaforo*/
void fecharConexao(int i){
	pthread_mutex_lock(&s3);
  	numeroConexao[i]=OFF;
  	pthread_mutex_unlock(&s3);
}
/*Função que retorna um número de conexão dispoinivel, caso não exista retorna -1.*/
int conexaoLivre(){
  	int i=0;
  	while ( i != MAXCONTATOS){
    	if(numeroConexao[i]==OFF){
      		numeroConexao[i]=ON;
      	return (i+1);
    	}
    i++;
  	}
  	return -1;
}
/*Função que adiciona na Matriz o numero do IP dado o numero da conexão.Também faz uso do semaforo*/
void adicionarIP(char ip[BFMSG],int numero){
  	pthread_mutex_lock(&s4);
  	strcpy(contatos[numero],ip);
  	pthread_mutex_unlock(&s4);
}
/*Essa é uma função que mostra na tela apartir da matriz quais são as conexões que estão funcionando. Também faz
uso de semaforos.*/
void exibirContatos(){
	pthread_mutex_lock(&s5);
  	int i=0;
  	while(i < MAXCONTATOS){
    	if(numeroConexao[i]==ON){
      		printf("Contato[%d]%s\n",i,contatos[i]);
    	}
    i++;
 	}
  	pthread_mutex_unlock(&s5);
}

/*Essa é a thread do tipo servidor, os parametros são passados de uma unica vez em um ponteiro para void, segunindo
o padrão POSIX.*/
void *servidor(void *aux2){

	int sock, connected, bytes_recv, i, true = 1;
	char send_data [BFMSG] , recv_data[BFMSG];
    struct sockaddr_in server_addr, client_addr;
    int sin_size,aux=0,first=0;
    char str[BFMSG];

    //transformndo o parametro em ponteiro para struct novamente.
    struct opServidor *par = (struct opServidor *) aux2;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1){
    	perror("Erro no Socket");
     	exit(1);
    }
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR , &true,sizeof(int)) == -1){
      perror("Erro Setsockopt");
      exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(par->porta);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    bzero(&(server_addr.sin_zero),8);

    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1){
      	perror("Nao foi possivel realizar o bind");
      	exit(1);
    }

    if (listen(sock, 10) == -1){
      perror("Erro de Listen");
      exit(1);
    }
    
    fflush(stdout);

    while(1){

      	sin_size = sizeof(struct sockaddr_in);
      	//estabelecendo a conexão quando o cliente requisitar.
      	connected = accept(sock, (struct sockaddr *)&client_addr, &sin_size);
      	printf("Conexão aceita de (%s , %d)\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
      	/*Se a thread for do tipo principal ela vai apenas passar a porta disponivel e fechar conexão*/
      	if( par->tipo == PRINCIPAL ){

          	printf("(Servidor Principal Aberto)\n");

          	while(1){
            	aux = conexaoLivre();
            	//printf("porta a ser conectada:%d\n",aux+PORT);
            	sprintf(send_data,"%d",aux+PORT);
           		send(connected,send_data,strlen(send_data),0);
            	fflush(stdout);
            	close(connected);
            break;
          	}
          	iniciarServidor(aux + PORT,CONEXAO,aux-1);
      	}
      	/* caso for do tipo conexão ela vai verificiar a todo tempo se tem q enviar uma mensagem, vai verificar
      	se tem q fechar a conexão e mandar a mensagem padrão para deixar o cliente desbloqueado e também vai receber
      	as mensagem e printar na tela  e comparar se for EXIT vai fechar a conexão.*/
      	else if (par->tipo == CONEXAO){
      		
      		//Funcoes que devem deixar o socket de tipo não bloqueante.
          	fcntl(connected, F_SETFL, O_ASYNC);
    		fcntl(connected, F_SETFL, O_NONBLOCK);

    		printf("Servidor Conexão Aberto\n");

          	while(1){
          		fcntl(connected, F_SETFL, O_NONBLOCK);
          			
          		if(first==0){
              		adicionarIP(inet_ntoa(client_addr.sin_addr),par->num);
              		first=1;
            	}

          		if(auxEnvio[par->num]==ON){
              		sleep(7.5);
              		send(connected,bufferEnvio,strlen(bufferEnvio),0);
              		auxEnvio[par->num]=OFF;
              		fflush(stdout);
            	}

            	if(auxFechar[par->num]==ON){
            		printf("Conexão com %s terminou\n",contatos[par->num]);
            		send(connected,"EXIT",strlen("EXIT"),0);
            		fecharConexao(par->num);
            		auxFechar[par->num]=OFF;
            		close(connected);
            		break;
            	}
            	
            	bytes_recv = recv(connected, recv_data, 1024, 0);
            	recv_data[bytes_recv] = '\0';

            	if (strcmp(recv_data,"\0\n") == 0){
            		send(connected,"0316",strlen("0316"),0);
            		fflush(stdout);
            		sleep(2);
        		}
            	else if (strncmp(recv_data,"EXIT\n",4) == 0){
             		close(connected);
              		printf("\nConexão com %s terminou. \n",contatos[par->num]);
              		fecharConexao(par->num);
             		fflush(stdout);
             		break;
            	}
            	else{
        			printf("Mensagem de %s: %s\n",inet_ntoa(client_addr.sin_addr),recv_data);
        			strcpy(recv_data,"\0\n");
        		}
          	}
        } 	
    }
   if (par->tipo == PRINCIPAL){
 		printf("Conexão Cliente Principal ON\n");
   	}
   	else{
   		printf("Conexão cliente Conexão ON\n");
   	}
   close(sock);
}
/*Essa é a thread do tipo cliente, os parametros são passados de uma unica vez em um ponteiro para void, segunindo
o padrão POSIX.*/
void *cliente(void *aux3){

	int sock, bytes_recv, i;
	char send_data[1024],recv_data[1024],server_name[512];
   	struct hostent *host;
   	struct sockaddr_in server_addr;
    int aux,first=0,aux2;
    char aux_data[1024];
    
    //transformndo o parametro em ponteiro para struct novamente.
    struct opCliente *par = (struct opCliente*) aux3;

   	host = gethostbyname(par->ip);

   	if (par->tipo == PRINCIPAL){
 		printf("Conexão Cliente Principal ON\n");
   	}
   	else{
   		printf("Conexão cliente Conexão ON\n");
   	}

   	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1){
      	perror("Erro de Socket");
      	exit(1);
   	}

   	server_addr.sin_family = AF_INET;
   	server_addr.sin_port = htons(par->porta);
   	server_addr.sin_addr = *((struct in_addr *)host->h_addr);
   	bzero(&(server_addr.sin_zero),8);

   	if (connect(sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1){
      	perror("Erro de conexao");
      	exit(1);
   	}

   	if(naoBloqueante(sock) != 0){
        printf("erro de bloqueante.");
    }

    fcntl(sock, F_SETFL, O_NONBLOCK);
    fcntl(sock, F_SETFL, O_ASYNC);

   	while(1){
   		/*Se o cliente for do tipo principal ele recebe a porta do servidor principal
   		e altera a variavel para disparar a criação do cliente do tipo conexão.*/
    	if (par->tipo == PRINCIPAL){

      		bytes_recv = recv(sock,recv_data,1024,0);
        	recv_data[bytes_recv] = '\0';
        	aux = atoi(recv_data);
        	sleep(2);
        	close(sock);
        	aux2 = conexaoLivre() - 1;
        	auxCriarCliente(1);
        	criarCliente(aux,par->ip,aux2);
        	fflush(stdin);
        	break;
      	}
      	/*Se o cliente for do tipo conexão ele verifica a todo loop se é preciso fechar ou enviar uma mensagem,
      	e ao receber a mensagem ele compara se for os numero 0316, ele não exibe na tela, caso não for ele retira
      	os 4 ultimos caracteres da mensagem recebida por ser esse numero padrão também. */ 
      	else if (par->tipo == CONEXAO){

			fcntl(sock, F_SETFL, O_NONBLOCK);
    		fcntl(sock, F_SETFL, O_ASYNC);        
   
        	if(first==0){
           		adicionarIP(par->ip,par->num);
           		first=1;
        	}
        
        	if(auxEnvio[par->num] == ON){
            	send(sock,bufferEnvio,strlen(bufferEnvio),0);
            	auxEnvio[par->num]=OFF;
        	}

        	if(auxFechar[par->num] == ON){
        		send(sock,"EXIT",strlen("EXIT"),0);
        		printf("Conexão com %s terminou\n",contatos[par->num]);
        		fecharConexao(par->num);
        		auxFechar[par->num];
        		close(sock);
        		break;
        	}

        	bytes_recv=recv(sock,recv_data,1024,0);
        	recv_data[bytes_recv] = '\0';
        
        	if (strcmp(recv_data,"\0\n") == 0 ){
            
        	}
        	else if (strncmp(recv_data,"0316\0",4) == 0){

        	}
      		else if (strncmp(recv_data,"EXIT\n",4) == 0){
            	close(sock);
            	printf("\nConexão com %s \n",contatos[par->num]);
            	fecharConexao(par->num);
            	fflush(stdout);
            	break;
        	}
        	else{
        		strncpy(aux_data,recv_data,bytes_recv-4);
            	printf("%s Enviou mensagem:  %s \n" ,inet_ntoa(server_addr.sin_addr),aux_data);
            	strcpy(recv_data,"\0\n");
        	}        
      	}
   	}
   	if (par->tipo == PRINCIPAL){
 		printf("Conexão Cliente Principal OFF\n");
   	}
   	else{
   		printf("Conexão cliente Conexão OFF\n");
   	}
   	
}
/*Essa thread que tem a finalidade de começar uma thread do tipo cliente, ela é uma auxiliar pois
antigamente essa funcionaldiade fica na thread cliente, mas como ela é volatil, acabava dando problemas,
ai foi feita essa thread auxiliar só com essa finalidade.*/
void *disparaCliente(){
	while(1){
	 	if (auxCliente == 1){
	 		sleep(2);
      		iniciarCliente(auxPorta,auxIP,CONEXAO,auxNum);
      		auxCriarCliente(0);
      	}
    }
}

/*Essa função tem a finalidade de disparar a thread do tipo servidor e ela também passa
a struct para um ponteiro de void.*/
void iniciarServidor(int porta,int tipo,int num){

  	struct opServidor *aux;
  	aux = (struct opServidor *) malloc(sizeof(struct opServidor));
  	aux->porta = porta;
  	aux->tipo = tipo;
  	aux->num = num;
  	pthread_t conexao;
  	int ret_conexao;
  	ret_conexao = pthread_create(&conexao,NULL,servidor,(void *)aux);
}
/*Função similar a de cima, só que para os clientes.*/
void iniciarCliente(int porta,char ip[BFMSG],int tipo,int num){

	struct opCliente *aux;
	aux = (struct opCliente *) malloc(sizeof(struct opCliente));
	aux->porta = porta;
	aux->tipo = tipo;
	aux->num = num;
	strcpy(aux->ip,ip);
	pthread_t conexao;
 	int ret_conexao;
    ret_conexao = pthread_create(&conexao,NULL,cliente,(void *)aux);
}
/*O menu basicamente inicializa os vetores, a thread servidor principal e as thread de disparars os clientes 
fica em um loop mostrando todas as opções possiveis.*/
int main (int argc,char **argv){

	int porta,i,aux,k,ver=-1,j;
  	char ip[BFMSG];
  	int ret_client,ret_cn;
  	pthread_t cn,cli;

  	for(i=0;i<MAXCONTATOS;i++){
    	numeroConexao[i]=OFF;
    	strcpy(contatos[i],"Sem Conexao");
    	auxEnvio[i]=OFF;
    	auxFechar[i]=OFF;
  	}  
  	/* Parte em que deve ser comentada quando for aplicado em dois computadores diferentes. */
  	printf("Entre com a porta:");
  	scanf("%d",&porta);
  	getchar();
	PORT = porta;

  	iniciarServidor(porta,PRINCIPAL,0);
  	ret_cn=pthread_create(&cn,NULL,disparaCliente,NULL);

  	while ( ver != 6 ) {

      	printf("1-Adicionar Contato\n2-Listar Contato\n3-Excluir Contato\n4-Enviar Mensagem\n5-Mensagem em grupo\n6-Sair\n");
      	scanf("%d",&ver);

      	if( ver == 1){
      		printf( "Porta de envio: \n");
        	scanf("%d",&porta);
        	printf("ip de destino\n");
        	scanf("%s", ip);
        	iniciarCliente(porta,ip,PRINCIPAL,0);
      	}
      	else if (ver == 2){
        	exibirContatos();
        	printf("\n");	
      	}
      	else if (ver == 3){
        	printf("Selecionar Contato a ser Excluido: (pelo numero)\n");
        	exibirContatos();
        	scanf("%d",&aux);
        	auxFechar[aux]=ON;
        	
      	}
      	else if(ver == 4){ 
        	printf("Selecionar Contato da Lista:(pelo numero)\n");
        	exibirContatos();
        	scanf("%d",&aux);
        	printf("Entre com a Mensagem:\n");
          fflush(stdin);
          scanf("%[\n]",bufferEnvio);
        	scanf("%[^\n]",bufferEnvio);
        	auxEnvio[aux]=ON;
      	}
      	else if(ver == 5){
      		printf("Entre com a mensagem destinada a todos: ");
      		scanf("%s",bufferEnvio);
      		j=0;
      		while(j != MAXCONTATOS){
      			if(numeroConexao[j]==ON){
      				auxEnvio[j]=ON;
      			}
      			j++;
      		}
      	}
      	else if(ver == 6){
      		printf("Fim do Menu\n");
      	}	
    }
	pthread_exit(&cn);
	printf("Fim do Programa.\n"); 
	return 0;  
}