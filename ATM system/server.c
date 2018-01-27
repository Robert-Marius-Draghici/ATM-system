#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFLEN 256

typedef struct {
	char first_name[12]; //prenume
	char last_name[12]; //nume
	int card_number;
	int pin;
	char secret_password[16];
	double sold;
	int locked;
	int login_attempts;
} ATMUser;

typedef struct {
	int socket;
	int client_index;
} SocketIndexMap;

void error(char *msg) {
	perror(msg);
	exit(1);
}

int check_card_number(ATMUser atm_users[], int N, int card_number) {
	int i;

	for(i = 0; i < N; i++) {
		if(atm_users[i].card_number == card_number)
			return i;
	}

	return -1;
}

int getIndex(SocketIndexMap map[], int N, int socket) {
	int i;

	for(i = 0; i < N; i++) {
		if(map[i].socket == socket)
			return i;
	}

	return -1;
}

int main(int argc, char *argv[]) {
	int listening_socket, client_socket, unblock_service_socket, received, i, j;
	unsigned int client_address_length;

	struct sockaddr_in server_address, client_address;
	char buffer[BUFLEN];

	FILE* users_data_file = fopen(argv[2], "r");
	int N;
	fscanf(users_data_file, "%d", &N);


	ATMUser atm_users[N];
	SocketIndexMap map[N];
	int logged_users[N];

	for(i = 0; i < N; i++) {
		fscanf(users_data_file, "%s", atm_users[i].last_name);
		fscanf(users_data_file, "%s", atm_users[i].first_name);
		fscanf(users_data_file, "%d", &atm_users[i].card_number);
		fscanf(users_data_file, "%d", &atm_users[i].pin);
		fscanf(users_data_file, "%s", atm_users[i].secret_password);
		fscanf(users_data_file, "%lf", &atm_users[i].sold);
		atm_users[i].locked = 0;
		atm_users[i].login_attempts = 0;
		map[i].client_index = i;
	}

	fd_set read_fds;	//multimea de citire folosita in select()
	fd_set tmp_fds;	//multime folosita temporar
	int fdmax;		//valoare maxima file descriptor din multimea read_fds

	if(argc < 3) {
		fprintf(stderr, "Usage : %s <port_server> <users_data_file> \n", argv[0]);
		exit(1);
	}

	//golim multimea de descriptori de citire (read_fds) si multimea tmp_fds
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	listening_socket = socket(PF_INET, SOCK_STREAM, 0);
	if(listening_socket < 0)
		error("-10 : Eroare la apel socket\n");

	unblock_service_socket = socket(PF_INET, SOCK_DGRAM, 0);
	if(unblock_service_socket < 0)
		error("-10 : Eroare la apel socket\n");

	memset((char *) &server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = INADDR_ANY;	// foloseste adresa IP a masinii
	server_address.sin_port = htons(atoi(argv[1]));

	if(bind(listening_socket, (struct sockaddr *) &server_address,
	        sizeof(struct sockaddr)) < 0)
		error("-10 : Eroare la apel bind\n");

	if(bind(unblock_service_socket, (struct sockaddr *) &server_address,
	        sizeof(struct sockaddr)) < 0)
		error("-10 : Eroare la apel bind\n");

	if(listen(listening_socket, 5) == -1)
		error("-10 : Eroare la apel listen\n");

	//adaugam noul file descriptor (socketul pe care se asculta conexiuni) in multimea read_fds
	FD_SET(listening_socket, &read_fds);
	//adaugam file descriptorul specific socketului UDP in multimea read_fds
	FD_SET(unblock_service_socket, &read_fds);
	//adaugam file descriptorul specific stdin (pentru citirea de la tastatura)
	FD_SET(0, &read_fds);
	fdmax = unblock_service_socket;

	// main loop
	while(1) {
		tmp_fds = read_fds;

		if(select(fdmax + 1, &tmp_fds, NULL, NULL, NULL) == -1)
			error("-10 : Eroare la apel select\n");


		for(i = 0; i <= fdmax; i++) {
			if(FD_ISSET(i, &tmp_fds)) {

				if(i == 0) {
					memset(buffer, 0, BUFLEN);
					fgets(buffer, BUFLEN - 1, stdin);
					buffer[strlen(buffer) - 1] = '\0';
					if(strcmp(buffer, "quit") == 0) {
						char* quit_message = "The server is going to shut down!";
						/*
						 * Ultimul socket adaugat in read_fds inainte de a introduce
						 * socketii TCP ai clientilor a fost socketul UDP, deci
						 * daca doresc sa inchid socketii TCP, pornesc de la
						 * valoarea socketului UDP + 1.
						 */
						for(j = unblock_service_socket + 1; j <= fdmax; j++) {
							if(FD_ISSET(j, &read_fds)) {
								if(send(j, quit_message, strlen(quit_message), 0) < 0)
									error("-10 : Eroare la apel send\n");
								close(j);
								FD_CLR(j, &read_fds);
							}
						}
						fclose(users_data_file);
						close(unblock_service_socket);
						close(listening_socket);						
						exit(0);
					}
				}
				else if(i == listening_socket) {
					// a venit ceva pe socketul inactiv(cel cu listen) = o noua conexiune
					// actiunea serverului: accept()
					client_address_length = sizeof(client_address);
					if((client_socket = accept(listening_socket, (struct sockaddr *)&client_address,
					                           &client_address_length)) == -1)
						error("-10 : Eroare la apel accept\n");
					else {
						//adaug noul socket intors de accept() la multimea descriptorilor de citire
						FD_SET(client_socket, &read_fds);
						if(client_socket > fdmax)
							fdmax = client_socket;
					}
					printf("Noua conexiune de la %s, port %d, socket_client %d\n ",
					       inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port),
					       client_socket);
				}
				//am primit o cerere de deblocare pe socketul UDP
				else if(i == unblock_service_socket) {
					memset(&client_address, 0, sizeof(client_address));
					client_address_length = sizeof(client_address);
					memset(buffer, 0, BUFLEN);
					if(recvfrom(unblock_service_socket, buffer, sizeof(buffer), 0,
					            (struct sockaddr*) & (client_address), &client_address_length) == -1)
						error("-10 : Eroare la apel recvfrom\n");
					else {
						char* token =  strtok(buffer, " ");
						char* command_tokens[3];
						int index = 0;
						while(token != NULL) {
							command_tokens[index] = malloc(strlen(token) * sizeof(char));
							strcpy(command_tokens[index++], token);
							token = strtok(NULL, " ");
						}

						char server_response[200];
						int current_client_index;
						int received_card_number;
						char received_secret_password[16];

						if(strcmp(command_tokens[0], "unlock") == 0) {
							received_card_number = atoi(command_tokens[1]);
							current_client_index = check_card_number(atm_users, N, received_card_number);

							if(current_client_index == -1) {
								sprintf(server_response, "UNLOCK> -4 : Numar card inexistent");
								sendto(unblock_service_socket, server_response, strlen(server_response), 0,
								       (const struct sockaddr *) &client_address, sizeof(client_address));
								continue;
							}
							else if(atm_users[current_client_index].locked == 0) {
								sprintf(server_response, "UNLOCK> -6 : Operatie esuata");
								sendto(unblock_service_socket, server_response, strlen(server_response), 0,
								       (const struct sockaddr *) &client_address, sizeof(client_address));
							}
							else {
								sprintf(server_response, "UNLOCK> Trimite parola secreta");
								sendto(unblock_service_socket, server_response, strlen(server_response), 0,
								       (const struct sockaddr *) &client_address, sizeof(client_address));
							}
						}
						else {
							received_card_number = atoi(command_tokens[0]);
							strcpy(received_secret_password, command_tokens[1]);
							current_client_index = check_card_number(atm_users, N, received_card_number);

							if(strcmp(atm_users[current_client_index].secret_password,
							          received_secret_password) == 0) {
								atm_users[current_client_index].locked = 0;
								sprintf(server_response, "UNLOCK> Client deblocat");
								sendto(unblock_service_socket, server_response, strlen(server_response), 0,
								       (const struct sockaddr *) &client_address, sizeof(client_address));
							}
							else {
								sprintf(server_response, "UNLOCK> -7 : Deblocare esuata");
								sendto(unblock_service_socket, server_response, strlen(server_response), 0,
								       (const struct sockaddr *) &client_address, sizeof(client_address));
							}
						}
					}
				}
				else {
					// am primit date pe unul din socketii cu care vorbesc cu clientii
					//actiunea serverului: recv()
					memset(buffer, 0, BUFLEN);
					if((received = recv(i, buffer, sizeof(buffer), 0)) <= 0) {
						if(received == 0) {
							//conexiunea s-a inchis neasteptat din cauza unei posibile erori
							printf("selectserver: socket %d hung up\n", i);
						}
						else
							error("-10 : Eroare la apel recv\n");
						close(i);
						FD_CLR(i, &read_fds); // scoatem din multimea de citire socketul pe care
					}

					else { //recv intoarce > 0
						printf("Am primit de la clientul de pe socketul %d, mesajul: %s\n", i, buffer);
						char* quit_message = "This client process is going to disconnect!";
						if(strcmp(buffer, quit_message) == 0) {
							close(i);
							FD_CLR(i, &read_fds);
							/*
							 * Daca exista un utilizator logat in cadrul procesului
							 * client ce tocmai a fost inchis, atunci acesta este
							 * delogat automat de catre server.
							 */
							if(logged_users[getIndex(map, N, i)] == 1)
								logged_users[getIndex(map, N, i)] = 0;
							break;
						}

						/*
						 * Impart comanda primita de la tastatura in cuvinte, 
						 * pentru a putea decide ce actiune sa intreprinda
						 * serverul in functie de primul cuvant si anume
						 * numele comenzii.
						 */
						char* token =  strtok(buffer, " ");
						/*
						 * In enunt am observat ca toate comenzile au cel mult
						 * 2 argumente, deci o comanda nu e formata din mai mult
						 * de 3 cuvinte.
						 */
						char* command_tokens[3];
						int index = 0;
						while(token != NULL) {
							command_tokens[index] = malloc(strlen(token) * sizeof(char));
							strcpy(command_tokens[index++], token);
							token = strtok(NULL, " ");
						}

						char server_response[200];
						int current_client_index;

						if(strcmp(command_tokens[0], "login") == 0) {
							int received_card_number = atoi(command_tokens[1]);
							int received_pin = atoi(command_tokens[2]);
							current_client_index = check_card_number(atm_users, N, received_card_number);
							if(current_client_index == -1) {
								sprintf(server_response, "ATM> -4 : Numar card inexistent");
								send(i, server_response, strlen(server_response), 0);
								continue;
							}
							else if(logged_users[current_client_index] == 1) {
								sprintf(server_response, "ATM> -2 : Sesiune deja deschisa");
								send(i, server_response, strlen(server_response), 0);
								atm_users[current_client_index].login_attempts = 0;
							}
							else if(atm_users[current_client_index].pin != received_pin) {

								if(atm_users[current_client_index].login_attempts < 3)
									atm_users[current_client_index].login_attempts++;

								if(atm_users[current_client_index].login_attempts == 3) {
									sprintf(server_response, "ATM> -5 : Card blocat");
									send(i, server_response, strlen(server_response), 0);
									atm_users[current_client_index].locked = 1;
									continue;
								}
								sprintf(server_response, "ATM> -3 : Pin gresit");
								send(i, server_response, strlen(server_response), 0);
							}
							else if(atm_users[current_client_index].locked) {
								sprintf(server_response, "ATM> -5 : Card blocat");
								send(i, server_response, strlen(server_response), 0);
							}
							else {
								/*
								 * In multimea logged_users asociez fiecarui
								 * utilizator valoarea 0 daca nu este logat
								 * sau 1 daca este logat.
								 */

								logged_users[current_client_index] = 1;
								sprintf(server_response, "ATM> Welcome %s %s",
								        atm_users[current_client_index].last_name,
								        atm_users[current_client_index].first_name);
								send(i, server_response, strlen(server_response), 0);
								atm_users[current_client_index].login_attempts = 0;
								map[current_client_index].socket = i;

							}
							command_tokens[1] = NULL;
							command_tokens[2] = NULL;

						}


						int client_index = getIndex(map, N, i);
						if(strcmp(command_tokens[0], "logout") == 0) {
							sprintf(server_response, "ATM> Deconectare de la bancomat");
							send(i, server_response, strlen(server_response), 0);
							logged_users[client_index] = 0;
						}

						if(strcmp(command_tokens[0], "listsold") == 0) {
							sprintf(server_response, "ATM> %.2lf", atm_users[client_index].sold);
							send(i, server_response, strlen(server_response), 0);
						}

						if(strcmp(command_tokens[0], "getmoney") == 0) {
							int to_withdraw = atoi(command_tokens[1]);

							if(to_withdraw % 10 != 0) {
								sprintf(server_response, "ATM> -9 : Suma nu este multiplu de 10");
								send(i, server_response, strlen(server_response), 0);
							}
							else if(to_withdraw > atm_users[current_client_index].sold) {
								sprintf(server_response, "ATM> -8 : Fonduri insuficiente");
								send(i, server_response, strlen(server_response), 0);
							}
							else {
								sprintf(server_response, "ATM> Suma %d retrasa cu succes", to_withdraw);
								send(i, server_response, strlen(server_response), 0);
								atm_users[client_index].sold -= to_withdraw;
							}
						}

						if(strcmp(command_tokens[0], "putmoney") == 0) {
							double to_deposit = strtod(command_tokens[1], NULL);
							sprintf(server_response, "ATM> Suma depusa cu succes");
							send(i, server_response, strlen(server_response), 0);
							atm_users[client_index].sold += to_deposit;
						}


					}
				}
			}
		}
	}
}
