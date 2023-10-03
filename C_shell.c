#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <pwd.h>
#include <time.h>
#include <sys/time.h>
#include <stddef.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

void cas(){					//funkcia na vypis aktualneho casu
	int sluzba_cas = 232;
	int hodnota;

	struct timespec *tp = malloc (sizeof(struct timespec));
	struct tm *info;
		
	asm volatile(			//inline asembler na zistenie aktualneho casu
		"push %%ecx \n\t"
		"push %%ebx \n\t"
		"push %%eax \n\t"
		"int $0x80 \n\t"
		:"=a" (hodnota)
		:"a" (sluzba_cas), "b" (CLOCK_REALTIME),"c" (tp)
		:"memory"
	);

	if(hodnota < 0){
		hodnota = clock_gettime(CLOCK_REALTIME, tp);		//pokial by sa nepodarilo zistit cas z inline asembleru pouzijem kniznicnu funkciu 
	}

	info = localtime(&tp->tv_sec);
	
	printf("%.2d:%.2d:%.2d ", info->tm_hour, info->tm_min, info->tm_sec);
}

uid_t zistiuid(){			//funkcia na zistenie UID pouzivatela
	uid_t uid;
	int sluzba_uid = 24;

	asm volatile(
		"int $0x80"
    		:"=a" (uid)
		:"0" (sluzba_uid)
	);
	
	return uid;
}
	
	
char *nazovos(){			//funkcia na zistenie nazvu stroja
	char *nazovOS;
	int max = 50, sluzba_os = 87, hodnota;	
	nazovOS = (char *) malloc (max * sizeof(char)); 

	asm volatile(
		"push %%ecx \n\t"
		"push %%ebx \n\t"
		"push %%eax \n\t"
		"int $0x80 \n\t"
		:"=a" (hodnota)
		:"a" (sluzba_os), "b" (nazovOS),"c" (max)
		:"memory"
	);

	return nazovOS;
}

char *citaj(){			//funkcia na precitanie vstupu
	char *riadok = NULL;
	int velkost = 256;
	int c, i = 0;
	
	riadok = (char *) malloc (velkost * sizeof(char));
		
	while(1){
		c = getchar();
		if (c == EOF || c == '\n'){		//citam do konca vstupu alebo do enteru
			riadok[i] = '\0';
			return riadok;
		}		
		else{
			riadok[i] = c;
		}
		i++; 
		
		if (i == velkost){
			velkost += 256;
			riadok = (char *) realloc (riadok, velkost * sizeof(char));
		}
	}
	return riadok;
}


struct prikaz{				//struktura na uchovanie dolezitych informacii o vsetkych suboroch
	char **argumenty;
	char separator;
	char *vstup;
	char *vystup;
};


struct prikaz *zistiprikazy(char *riadok, int *pocet_prikazov_ptr){			//funkcia na zistenie vsetkych prikazov zadanych pouzivatelom
	//Vo funkcii zistiprikazy rozoznavam specialne znaky, ktore interpetujem nasledovne
	//	'' - rusi vyznam specialnych znakov medzi nimi
	//	\ - rusi vyznam nasledovneho specialneho znaku 
	//	# - komentar - za nim sa uz nic neberie do uvahy
	//	> - vystup do nejakeho suboru
	//	< - vstup z nejakeho suboru
	//	; - oddelovac prikazov
	//	| - pipe, na presmerovanie vystupu jedneho prikazu na vstup dalsieho

	int j = 0, pocet = 0, pocet_prikazov = 0, n_arg = 10, n_prik = 10,  poz = 0, i; 	
	struct prikaz *prikazy;
	char apostrof = 0;

	prikazy = (struct prikaz*) malloc (n_prik * sizeof(struct prikaz));
	
	prikazy[pocet_prikazov].argumenty = (char **) malloc (n_arg * sizeof(char *));
	prikazy[0].vystup = NULL;
	prikazy[0].vstup = NULL;					//inicializacia vsetkych premennych aj struktur

	while (riadok[poz] != '\0' && (riadok[poz] != '#' || (poz > 0 && riadok[poz - 1] == '\\'))){	//citam po koniec alebo po komentar pokial prednim nebol backslash 
		prikazy[pocet_prikazov].argumenty[pocet] = (char *) malloc (30 * sizeof(char *));
		i = 0;

		while (riadok[poz] == ' ')
			poz++;

		if (riadok[poz] == '\''){
			poz++;
			apostrof = 1;		
		}
		//argument prikazu ( koncim pokial sa tam nachadza specialny znak mimo apostrofov a pokial prednim nebol /)
		while (riadok[poz] != '\0' && (apostrof == 1 || (riadok[poz] != ' ' && ((riadok[poz] != '<' && riadok[poz] != '>' && riadok[poz] != '|' && riadok[poz] != ';' && riadok[poz] != '#') || (poz > 0  && riadok[poz - 1] == '\\'))))){
			if (riadok[poz] == '\''){
				apostrof = 0;
			}
			else{
				prikazy[pocet_prikazov].argumenty[pocet][i] = riadok[poz];
				i++;
			}
			poz++; 
		}
		prikazy[pocet_prikazov].argumenty[pocet][i] = '\0';
		
		while (riadok[poz] == ' ')
			poz++;

		pocet++;
		
		if (riadok[poz] == '<'){		//vstupny subor do prikazu
	
			prikazy[pocet_prikazov].vstup = (char *) malloc (256 * sizeof(char));
			poz++;
			while (riadok[poz] == ' ')
				poz++;
			j = 0;
			
			if (riadok[poz] == '\''){
				poz++;
				apostrof = 1;		
			}

			while (riadok[poz] != '\0' && (apostrof == 1 || (riadok[poz] != ' ' && ((riadok[poz] != '<' && riadok[poz] != '>' && riadok[poz] != '|' && riadok[poz] != ';' && riadok[poz] != '#') || (poz > 0  && riadok[poz - 1] == '\\'))))){
				if (riadok[poz] == '\''){
					apostrof = 0;
				}
				else{	
					prikazy[pocet_prikazov].vstup[j] = riadok[poz];
					j++;
				}
				poz++; 
			}	
		}

		while (riadok[poz] == ' ')
			poz++;
		
		if (riadok[poz] == '>'){		//vystupny subor do ktoreho sa zapisuje vystup

			prikazy[pocet_prikazov].vystup = (char *) malloc (256 * sizeof(char));
			poz++;
			while (riadok[poz] == ' ')
				poz++;
			j = 0;
			
			if (riadok[poz] == '\''){
				poz++;
				apostrof = 1;		
			}

			while (riadok[poz] != '\0' && (apostrof == 1 || (riadok[poz] != ' ' && ((riadok[poz] != '<' && riadok[poz] != '>' && riadok[poz] != '|' && riadok[poz] != ';' && riadok[poz] != '#') || (poz > 0  && riadok[poz - 1] == '\\'))))){

				if (riadok[poz] == '\''){
					apostrof = 0;
				}
				else{	
					prikazy[pocet_prikazov].vystup[j] = riadok[poz];
					j++;
				}
				poz++; 	
			}	
		} 

		while (riadok[poz] == ' ')
			poz++;
		
		if (riadok[poz] == '|' || riadok[poz] == ';'){		//separatory: | - "spojenie prikazov", ; - oddelenie prikazov
			prikazy[pocet_prikazov].separator = riadok[poz];
			pocet = 0;
			
			if (riadok[++poz] != '\0'){
				pocet_prikazov++;
				prikazy[pocet_prikazov].argumenty = (char **) malloc (n_arg * sizeof(char *));
				prikazy[pocet_prikazov].vstup = NULL;
				prikazy[pocet_prikazov].vystup = NULL;
			}
			else
				break;
		}

		while (riadok[poz] == ' ')
			poz++;

		if (pocet_prikazov >=  n_prik){
			n_prik += 10;
			prikazy = (struct prikaz*) realloc (prikazy, n_prik * sizeof(struct prikaz));
		}
	} 
	prikazy[pocet_prikazov].separator = ';';
	*pocet_prikazov_ptr = pocet_prikazov + 1;
	
	return prikazy;
}

int spustenie(struct prikaz *prikazy, int start, int pocet_prikazov, int klient){	//vykonanie nie internych prikazov
	pid_t pid, wpid;
	int status, i, j, pom, subor;
	int **polep;	
	polep = (int **) malloc (pocet_prikazov * sizeof(int *));

	for (i = 0; i < start + pocet_prikazov; i++){				//vytvorenie pipe
		polep[i] = (int *) malloc (2 * sizeof(int));
		if (pipe(polep[i]) < 0){
			perror("Chyba pri pipeovani");
			exit(EXIT_FAILURE);
		}
	}

	pom = 0;
	for (i = start; i < start + pocet_prikazov; i++){		//pipovanie, vykonavanie, presmerovanie , etc.
		pid = fork();										//forkovanie na viac procesov (dieta , rodic)
		if (pid == 0){
			if (i != start){
				dup2(polep[i - 1][0], 0);
				
			}
			if (prikazy[i].separator == '|'){			 //pipovanie
				dup2(polep[i][1],1);
				pom++;
							
			}
			if (prikazy[i].vystup != NULL || (klient > 0 && i == start + pocet_prikazov - 1)){		//vystup
				if (prikazy[i].vystup != NULL){
					if (prikazy[i].vystup[0] == '&'){
						memmove(prikazy[i].vystup, prikazy[i].vystup + 1, strlen(prikazy[i].vystup));			
						subor = atoi(prikazy[i].vystup);
				
					}
					else{
						subor = open(prikazy[i].vystup, O_CREAT | O_TRUNC | O_WRONLY, 0644);
					}
				}	
				else{
					subor = klient;
				}
				if (subor < 0){
					printf("Nemozno otvorit subor\n");
					exit(EXIT_FAILURE);
				}
		
				dup2(subor, 1);
			}
			if (prikazy[i].vstup != NULL){				//vstup
				subor = open(prikazy[i].vstup, O_RDONLY, 0);
				if (subor < 0){
					printf("Nemozno otvorit subor\n");
					exit(EXIT_FAILURE);
				}
				dup2(subor, 0);
				
			}	
			
			for (j = 0; j < start + pocet_prikazov; j++){		//uzatvorenie vsetkych pipe
				close(polep[j][0]);
				close(polep[j][1]);
			}
			
			if (execvp(prikazy[i].argumenty[0], prikazy[i].argumenty) == -1){		//vykonanie prikazu
				perror("Chyba pri vykonavani");
				exit(EXIT_FAILURE);
			}
		}
		else if (pid < 0){
			perror("Chyba pri forkovani");
			exit(EXIT_FAILURE);
		}
		
	}
	
	for (j = 0; j < start + pocet_prikazov; j++){			//uzavretie vsetkych pipe
		close(polep[j][0]);
		close(polep[j][1]);
	}	
	for (j = start; j < start + pocet_prikazov; j++){		//cakanie rodica na vsetky deti
		do {
      			wpid = waitpid(pid, &status, WUNTRACED);
    		}while (!WIFEXITED(status) && !WIFSIGNALED(status));
	}

	return 1;
}
int server2(char *cesta, int cass, int port);

int help(char **prikazy){							//interny prikaz help, taktie funguje pri prepinaci -h
	printf("Autor: Marian Jarnik\n");
	printf("Ucelom programu je vytvorit jednoduchy shell na spracovanie zakladnych prikazov a systemovych volani.\n");
	printf("Shell interpretuje specialne znaky: #, ;, <, >, |, \\ a ''.\n");
	printf("Prikazy je mozne zadat zo standardneho vstupu a tiez aj zo spojeni reprezentovanymi socketmi.\n");
	printf("Program podporuje lokalne aj TCP sockety, podla toho ci pouzivatel pouzije prepinace -u alebo -p a -i.\n");
	printf("Z volitelnych uloh program podporuje 2, 3, 4, 5, 8, 9, 10, 11, 18, 19.\n");
	printf("Program sa pouziva ako shell na ovladanie pocitaca.\n");	

	return 1;
}

int listenprikaz(char **argv){				//interny prikaz listen s prepinacmi otvori server
	char *cesta = NULL;
	int port = 0, cass = 0, i;

	for (i = 1; i < 10; i++){
		if (argv[i] == NULL){
			break;
		}
		if (strcmp(argv[i], "-u") == 0 && argv[i + 1] != NULL && cesta == NULL){
			cesta = (char *) malloc (50 * sizeof(char));
			strcpy(cesta, argv[i + 1]);
		}
		else if (strcmp(argv[i], "-p") == 0 && argv[i + 1] != NULL){
			port = atoi(argv[i + 1]);
		}
		else if (strcmp(argv[i], "-i") == 0 && argv[i + 1] != NULL && cesta == NULL){
			cesta = (char *) malloc (50 * sizeof(char));
			strcpy(cesta, argv[i + 1]);
		}
		else if (strcmp(argv[i], "-t") == 0 && argv[i + 1] != NULL){
			cass = atoi(argv[i + 1]);
		}
	}

	if (port > 0 || cesta != NULL){
		if (server2(cesta, cass, port) == 0){
			return 0;
		}
	}
	else{
		printf("Zadajte v tvare 'listen -u cesta / -i adresa / -p port / -t cas '\n");	
	}

	return 1;
}

int quit(char **prikazy){			//interny prikaz na ukoncenie programu
	return 0;
}

int halt(char **prikazy){
	return 0;
}

char *nazvyfunkcii[] = {			//vsetky interne prikazy
	"listen",
	"help",
	"quit",
	"halt"
};

int (*funkcie[]) (char **) = {
	&listenprikaz,
	&help,
	&quit,
	&halt
};

int vykonaj(struct prikaz *prikazy, int pocet_prikazov, int klient){		//zistenie ci su prikazy interne alebo externe a nasledne vykonanie prikazov
	int i, j;
	int start = 0;	
	
	for (i = 0; i < pocet_prikazov; i++){
		if (prikazy[i].separator == ';'){					//; oddeli vsetky prikazy 
			for (j = 0; j < 4; j++){
				if (strcmp(prikazy[start].argumenty[0], nazvyfunkcii[j]) == 0){
					if (((*funkcie[j]) (prikazy[start].argumenty)) == 0){
						return 0; 
					}
					else{
						break;
					}
				}
			}
			if (j == 4){					// j == 4 len ked to nie je interny prikaz pretoze interny prikaz halt by program ukoncil
				spustenie(prikazy, start, i + 1 - start, klient);
				start = i + 1;
			}
		}
		
	}
	return 1;
}


int server2(char *cesta, int cass, int port){		//server
	int i, s, r, max_clienti = 20, sock, max_sock;
	fd_set set;
	char prikaz[256];
	struct sockaddr *adresa;
	socklen_t velkost_adr;	
	char *riadok;
	struct prikaz *prikazy;
	int clienti[20];
	struct timeval *tv;

	if (cass > 0){							//pokial bol zadany prepinac -t program bude cakat len tolko sekund kolko bolo za prepinacom zadanych
		tv = (struct timeval *) malloc (sizeof(struct timeval));
		tv->tv_sec = cass;
		tv->tv_usec = 0;
	}
	else{
		tv = NULL;
	}
	
	for (i = 0; i < max_clienti; i++)
		clienti[i] = -1;

	if (port == 0){						//ak je port nulovy je to lokalny socket
		struct sockaddr_un adrun;
		memset(&adrun, 0, sizeof(adrun));
		adrun.sun_family = AF_LOCAL;
		strcpy(adrun.sun_path, cesta);

		adresa = (struct sockaddr *) &adrun;

		if ((s = socket(PF_LOCAL, SOCK_STREAM, 0)) == -1){
			perror("socket");
			exit(EXIT_FAILURE);
		}
		velkost_adr = sizeof(adrun);
		unlink(cesta);
	}
	else {								//ak je nenulovy je to TCP socket
		struct sockaddr_in adrin;	
		memset(&adrin, 0, sizeof(adrin));
		adrin.sin_family = AF_INET;
		
		if (cesta == NULL)
			adrin.sin_addr.s_addr = htonl(INADDR_ANY);
		else
			adrin.sin_addr.s_addr = inet_addr(cesta);
		

		adrin.sin_port = htons(port);

		adresa = (struct sockaddr *) &adrin;	
		if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1){
			perror("socket");
			exit(EXIT_FAILURE);
		}
		velkost_adr = sizeof(adrin);
	}

	if (bind(s, adresa, velkost_adr) != 0){
		printf("Chyba pri bind\n");
		exit(EXIT_FAILURE);
	}

	if (listen(s, 5) != 0){
		printf("Chyba v listen\n");
		exit(EXIT_FAILURE);
	}
	
	int pocet_prikazov;
	
	while(1){
		FD_ZERO(&set);
		FD_SET(0, &set);
		FD_SET(s, &set);
		
		max_sock = s;
		
		for (i = 0; i < max_clienti; i++){			//FD_SET vsetkeho (socket serveru, standardny vstup, clienti)
			sock = clienti[i];

			if (sock > 0)
				FD_SET(sock, &set);
			
			if (sock > max_sock)
				max_sock = sock;
		}
	
		if (select(max_sock + 1, &set, NULL, NULL, tv) <= 0)		//pokial prejde cas alebo nastane chyba server skonci
			break;
		
		if (FD_ISSET(s, &set)){										//na server socket sa niekto pripaja
			if ((sock = accept(s, adresa, &velkost_adr)) < 0){
				perror("Chyba pri akceptovani");
				exit(EXIT_FAILURE);
			}
			printf("Na server sa prave pripojil 1 klient\n");
			for (i = 0; i < max_clienti; i++){
				if (clienti[i] == -1){
					clienti[i] = sock;
					break;
				}
			}	
		}	

		if (FD_ISSET(0, &set)){										//nieco zadane na standardnom vstupe 
			riadok = citaj();
			prikazy = zistiprikazy(riadok, &pocet_prikazov);

			if (strcmp(prikazy[0].argumenty[0], "stat") == 0){		//vykonanie stat (vsetci klienti pripoejny na server)
				for (i = 0; i < max_clienti; i++)
					if (clienti[i] > 0){
						if (port == 0){
							printf("Socket: %d\n", clienti[i]);
						}
						else{
							struct sockaddr_in *ad = (struct sockaddr_in *)adresa;
							getpeername(clienti[i], adresa, &velkost_adr);
							printf("Socket: %d, ip: %s, port: %d\n", clienti[i], inet_ntoa(ad->sin_addr) , ntohs(ad->sin_port));
						}
					}
			}
			else if (strcmp(prikazy[0].argumenty[0], "abort") == 0){	//vykonanie abort (zrusenie spojenia s klientom n)
				int x = atoi(prikazy[0].argumenty[1]);
				for (i = 0; i < max_clienti; i++){
					if (clienti[i] == x){
						clienti[i] = -1;
						close(x);
						break;
					}
				}
			}
			else if (strcmp(prikazy[0].argumenty[0], "close") == 0){	//zavretie socketu
				break;	
			}
			else if (strcmp(prikazy[0].argumenty[0], "halt") == 0){		//ukoncenie celeho programu
				return 0;	
			}
			else if (vykonaj(prikazy, pocet_prikazov, 0) == 0)
				break;
		}

		for (i = 0; i < max_clienti; i++){
			sock = clienti[i];
			if (sock > 0 && FD_ISSET(sock, &set)){						//odpojenie klienta zo strany klienta
				if ((r = read(sock, prikaz, 256)) == 0){
					printf("Odpojil som clienta\n");
					close(sock);
					clienti[i] = -1;
				}
				else{
					prikazy = zistiprikazy(prikaz, &pocet_prikazov);	//vykonanie prikazov (vystup ide klientovi - robi sa fo funkcii spustenie)
					
					vykonaj(prikazy, pocet_prikazov, sock);
					write(1, "Vykonal som prikazy\n", 20);
				}	

			}	
		
		}
		
	}
	for (i = 0; i < max_clienti; i++){								//zatvorenie vsetkych socketov
		sock = clienti[i];
		if (sock > 0)
			close(sock);
	}
	close(s);

	return 1;
}


int client2(char* cesta, int cass, int port){			//client
	int s, r;
	fd_set set;
	char *riadok;
	struct sockaddr *adresa;
	struct timeval *tv;
	socklen_t velkost_adr;

	if (cass > 0){									//cas rovnako ako server
		tv = (struct timeval *) malloc (sizeof(struct timeval));
		tv->tv_sec = cass;
		tv->tv_usec = 0;
	}
	else{
		tv = NULL;
	}

	if (port == 0){									//taktiez aj rozdiel medzi TCP a LOKAL
		struct sockaddr_un adrun;
		memset(&adrun, 0, sizeof(adrun));
		adrun.sun_family = AF_LOCAL;
		strcpy(adrun.sun_path, cesta);

		adresa = (struct sockaddr *) &adrun;

		if ((s = socket(PF_LOCAL, SOCK_STREAM, 0)) == -1){
			perror("socket");
			exit(EXIT_FAILURE);
		}
		velkost_adr = sizeof(adrun);
	}
	else {
		struct sockaddr_in adrin;	
		memset(&adrin, 0, sizeof(adrin));
		adrin.sin_family = AF_INET;

		if (cesta == NULL)
			adrin.sin_addr.s_addr = inet_addr("127.0.0.1");
		else
			adrin.sin_addr.s_addr = inet_addr(cesta);

		adrin.sin_port = htons(port);

		adresa = (struct sockaddr *) &adrin;	
		if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1){
			perror("socket");
			exit(EXIT_FAILURE);
		}
		velkost_adr = sizeof(adrin);
	}

	if (connect(s, adresa, velkost_adr) != 0){
		printf("Nepodarilo sa pripojit na server\n");
		exit(EXIT_FAILURE);
	}
	else{
		printf("Pripojenie na server bolo uspesne\n");
	}

	FD_ZERO(&set);
	FD_SET(0, &set);
	FD_SET(s, &set);
	

	while(1){
		if (cass > 0){
			tv->tv_sec = cass;
			tv->tv_usec = 0;
		}
		int p = select(s+1, &set, NULL, NULL, tv);
		if (p > 0){
			if (FD_ISSET(0, &set)){
				riadok = citaj();
				if (strcmp(riadok, "quit") == 0){
					break;
				}
				else if (strcmp(riadok, "halt") == 0){
					close(s);
					return 0;
				}
				write(s, riadok, 256);
			}
			if (FD_ISSET(s, &set)){
				r = read(s, riadok, 1);
				if (r == 0){
					write(1, "Spojenie so serverom bolo uzatvorene\n", 38);
					break;
				}	
				write(1, riadok, r);
			}
			FD_ZERO(&set);
			FD_SET(0, &set);
			FD_SET(s, &set);
		}
		else{
			break;
		}
	}	
	close(s);

	return 1;
}


int main(int argc, char *argv[]){
	int koniec = 1, i, port = 0, cass = 0, client = 0, pocet_prikazov, uzivatel;
	char *riadok;
	char *cesta = NULL;
	struct prikaz *prikazy;
	uzivatel = zistiuid();
	char *meno = (char *) malloc (50 * sizeof(char));
	strcpy(meno, getpwuid(uzivatel)->pw_name);
	char *nazovOS = nazovos();


	for (i = 1; i < argc; i++){								//zistovanie vstupu (server/client a ich argumenty, alebo vypis helpu)
		if (strcmp(argv[i], "-u") == 0 && argv[i + 1] != NULL && cesta == NULL){
			cesta = (char *) malloc (50 * sizeof(char));
			strcpy(cesta, argv[i + 1]);
		}
		else if (strcmp(argv[i], "-p") == 0 && argv[i + 1] != NULL){
			port = atoi(argv[i + 1]);
		}
		else if (strcmp(argv[i], "-i") == 0 && argv[i + 1] != NULL && cesta == NULL){
			cesta = (char *) malloc (50 * sizeof(char));
			strcpy(cesta, argv[i + 1]);
		}
		else if (strcmp(argv[i], "-t") == 0 && argv[i + 1] != NULL){
			cass = atoi(argv[i + 1]);
		}
		else if (strcmp(argv[i], "-h") == 0){
			help(argv);
			return 0;
		}
		else if (strcmp(argv[i], "-c") == 0){
			client = 1;
		}
	}

	if (argc > 1 && (cesta != NULL || port > 0)){
		if (client == 1){
			if (client2(cesta, cass, port) == 0){
				return 0;
			}
		}
		else{
			if (server2(cesta, cass, port) == 0){
				return 0;
			}
		}
	}

	do{							//hlavny cyklus (precitaj vstup, zistiprikazy, vykonaj prikazy)
		cas();
		printf("%s@%s#> ", meno, nazovOS);
		riadok = citaj();
		prikazy = zistiprikazy(riadok, &pocet_prikazov);
		koniec = vykonaj(prikazy, pocet_prikazov, 0);	

		
	}while (koniec);

	return 0;
}
/*Ako som pisal v dokumentacii aj v helpe v programe su implementovane tieto ulohy z volitelnej casti
	2, 3, 4, 5, 8, 9, 10, 11, 18, 19 - blizsie info je v dokumentacii programu*/
