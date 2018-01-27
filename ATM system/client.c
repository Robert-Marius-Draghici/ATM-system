#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFLEN 256

void error(char *msg) {
	perror(msg);
	exit(1);
}

int main(int argc, char *argv[]) {
	int atm_socket, unblock_service_socket, i, received, logged, sent;
	int last_card_number;
	struct sockaddr_in server_address;
	char buffer[BUFLEN];
	char input_command[200];

	char log_file_name[30];
	sprintf(log_file_name, "client-%d.log", getpid());
	FILE* client_log = fopen(log_file_name, "w");

	fd_set read_fds;	//multimea de citire folosita in select()
	fd_set tmp_fds;	//multime folosita temporar
	int fdmax;		//valoare maxima file descriptor din multimea read_fds

	//golim multimea de descriptori de citire (read_fds) si multimea tmp_fds
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	if(argc < 3) {
		fprintf(stderr, "Usage %s <IP_server> <port_server>\n", argv[0]);
		fprintf(client_log, "Usage %s <IP_server> <port_server>\n", argv[0]);
		exit(0);
	}

	atm_socket = socket(AF_INET, SOCK_STREAM, 0);
	if(atm_socket < 0) {
		error("-10 : Eroare la apel socket\n");
		fprintf(client_log, "atm_socket = socket(AF_INET, SOCK_STREAM, 0);\n");
		fprintf(client_log, "-10 : Eroare la apel socket\n");
	}

	unblock_service_socket = socket(PF_INET, SOCK_DGRAM, 0);
	if(unblock_service_socket < 0) {
		error("-10 : Eroare la apel socket\n");
		fprintf(client_log,
		        "unblock_service_socket = socket(PF_INET, SOCK_DGRAM, 0);\n");
		fprintf(client_log, "-10 : Eroare la apel socket\n");
	}

	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(atoi(argv[2]));
	inet_aton(argv[1], &server_address.sin_addr);

	FD_SET(atm_socket, &read_fds);
	FD_SET(unblock_service_socket, &read_fds);
	FD_SET(0, &read_fds);
	fdmax = unblock_service_socket;

	if(connect(atm_socket, (struct sockaddr*) &server_address,
	           sizeof(server_address)) < 0) {
		error("-10 : Eroare la apel connect\n");
		fprintf(client_log, "connect(atm_socket, (struct sockaddr*) &server_address,"
		        "sizeof(server_address))\n");
		fprintf(client_log, "-10 : Eroare la apel connect\n");
	}

	logged = 0;

	while(1) {
		tmp_fds = read_fds;
		if(select(fdmax + 1, &tmp_fds, NULL, NULL, NULL) == -1) {
			error("-10 : Eroare la apel select\n");
			fprintf(client_log, "select(fdmax + 1, &tmp_fds, NULL, NULL, NULL)\n");
			fprintf(client_log, "-10 : Eroare la apel select\n");
		}

		for(i = 0; i <= fdmax; i++) {
			if(FD_ISSET(i, &tmp_fds)) {

				if(i == 0) {
					//citesc de la tastatura
					memset(buffer, 0, BUFLEN);
					fgets(buffer, BUFLEN - 1, stdin);
					buffer[strlen(buffer) - 1] = '\0';
					if(strcmp(buffer, "quit") == 0) {
						fprintf(client_log, "quit");
						char* quit_message = "This client process is going to disconnect!";
						sent = send(atm_socket, quit_message, strlen(quit_message), 0);
						if(sent < 0) {
							error("-10 : Eroare la apel send\n");
							fprintf(client_log,
							        "send(atm_socket, quit_message, strlen(quit_message), 0);\n");
							fprintf(client_log, "-10 : Eroare la apel send\n");
						}
						fclose(client_log);
						close(atm_socket);
						close(unblock_service_socket);
						return 0;
					}
					strcpy(input_command, buffer);
					input_command[strlen(buffer) + 1] = '\0';
					/*
					 * Daca utilizatorul incearca sa se logheze, desi are deja
					 * o sesiune activa, acest lucru va genera o eroare.
					 */
					if(strstr(buffer, "login")) {
						char* temporary_buffer = strdup(buffer);
						strcpy(temporary_buffer, temporary_buffer + 6);
						char auxiliary[6];
						strncpy(auxiliary, temporary_buffer, 6);
						/*
 						 * Retin numarul de card corespunzator ultimei comenzi de 
 						 * login efectuate.
 						 */
						last_card_number = atoi(auxiliary);

						if(logged) {
							printf("-2 : Sesiune deja deschisa\n");
							fprintf(client_log, "%s\n", input_command);
							fprintf(client_log, "-2 : Sesiune deja deschisa\n");
						}
						else {
							sent = send(atm_socket, buffer, strlen(buffer), 0);
							if(sent < 0) {
								error("-10 : Eroare la apel send\n");
								fprintf(client_log, "send(atm_socket, buffer, strlen(buffer), 0);\n");
								fprintf(client_log, "-10 : Eroare la apel send\n");
							}
						}
						break;
					}

					if(strcmp(buffer, "unlock") == 0) {
						char unlock_request[13];
						sprintf(unlock_request, "unlock %d", last_card_number);
						sendto(unblock_service_socket, unlock_request, strlen(unlock_request), 0,
						       (const struct sockaddr *) &server_address, sizeof(server_address));

						memset(buffer, 0, BUFLEN);
						unsigned int server_address_length = sizeof(server_address);
						if(recvfrom(unblock_service_socket, buffer, sizeof(buffer), 0,
						            (struct sockaddr*) & (server_address), &server_address_length) == -1) {
							error("-10 : Eroare la apel recvfrom\n");
							fprintf(client_log,
							        "recvfrom(unblock_service_socket, buffer, sizeof(buffer), 0,"
							        "(struct sockaddr*) & (server_address), &server_address_length)\n");
							fprintf(client_log, "-10 : Eroare la apel recvfrom\n");
						}
						else {
							printf("%s\n", buffer);
							/*
							 * Pentru deblocarea cardului, utilizatorul trebuie sa trimita
							 * parola secreta la server prin socketul udp. Acest lucru
							 * se petrece doar daca respectivul card este blocat si conform
							 * enuntului doar in urma unei comenzi unlock facuta de
							 * utilizatorul care are respectivul card blocat.
							 */

							if(strcmp(buffer, "UNLOCK> Trimite parola secreta") == 0) {
								fprintf(client_log, "%s\n", input_command);
								fprintf(client_log, "%s\n", buffer);
								fgets(buffer, BUFLEN - 1, stdin);
								buffer[strlen(buffer) - 1] = '\0';
								char unlock_request[23];
								fprintf(client_log, "%s\n", buffer);
								sprintf(unlock_request, "%d %s", last_card_number, buffer);
								sendto(unblock_service_socket, unlock_request, strlen(unlock_request), 0,
								       (const struct sockaddr *) &server_address, sizeof(server_address));
							}

						}

						break;
					}

					/*
					 * Daca incerc sa introduc o alta comanda in afara de login
					 * desi nu sunt inca autentificat, atunci voi primi o
					 * eroare.
					 */
					if(strstr(buffer, "login") == NULL && !logged) {
						printf("-1 : Clientul nu este autentificat\n");
						fprintf(client_log, "%s\n", input_command);
						fprintf(client_log, "-1 : Clientul nu este autentificat\n");
						break;
					}

					//trimit mesaj la server
					sent = send(atm_socket, buffer, strlen(buffer), 0);
					if(sent < 0) {
						error("-10 : Eroare la apel send\n");
						fprintf(client_log, "send(atm_socket, buffer, strlen(buffer), 0);\n");
						fprintf(client_log, "-10 : Eroare la apel send\n");
					}
				}
				else if(i == unblock_service_socket) {
					memset(buffer, 0, BUFLEN);
					unsigned int server_address_length = sizeof(server_address);
					if(recvfrom(unblock_service_socket, buffer, sizeof(buffer), 0,
					            (struct sockaddr*) & (server_address), &server_address_length) == -1) {
						error("-10 : Eroare la apel recvfrom\n");
						fprintf(client_log,
						        "recvfrom(unblock_service_socket, buffer, sizeof(buffer), 0,"
						        "(struct sockaddr*) & (server_address), &server_address_length)\n");
						fprintf(client_log, "-10 : Eroare la apel recvfrom\n");
					}
					else {
						printf("%s\n", buffer);
						fprintf(client_log, "%s\n", buffer);
					}
				}
				else {
					// am primit date pe socketul cu care vorbesc cu serverul
					memset(buffer, 0, BUFLEN);
					if((received = recv(i, buffer, sizeof(buffer), 0)) <= 0) {
						if(received == 0) {
							//conexiunea s-a inchis
							printf("Serverul s-a inchis!\n");
							fprintf(client_log, "Serverul s-a inchis!\n");
						}
						else {
							error("-10 : Eroare la apel recv\n");
							fprintf(client_log, "recv(i, buffer, sizeof(buffer), 0);\n");
							fprintf(client_log, "-10 : Eroare la apel recv\n");
						}
						close(i);
						FD_CLR(i, &read_fds); // scoatem din multimea de citire socketul pe care
					}

					else  { //recv intoarce >0
						printf("%s\n", buffer);
						/*
						 * Daca in mesajul primit de la server se afla cuvantul
						 * Welcome, acesta indica faptul ca utilizatorul s-a
						 * conectat cu succes la bancomat si marcam acest lucru
						 * setand valoarea logged la 1. Astfel, daca in cadrul
						 * acestui proces client se va incerca logarea aceluiasi
						 * utilizator sau a altuia, acest lucru va fi prevenit.
						 */
						fprintf(client_log, "%s\n", input_command);
						fprintf(client_log, "%s\n", buffer);
						if(strstr(buffer, "Welcome"))
							logged = 1;

						if(strcmp(buffer, "ATM> Deconectare de la bancomat") == 0)
							logged = 0;

						char* quit_message = "The server is going to shut down!";
						if(strcmp(buffer, quit_message) == 0) {
							fclose(client_log);
							close(atm_socket);
							close(unblock_service_socket);
							exit(0);
						}
					}

				}

			}
		}
	}
	return 0;
}

