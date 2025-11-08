/* 
 * HOMEWORK #3 COMPLETED BY DEVIN DIAZ & KLAUDIO VULKA
 *
 * Client FTP program
 *
 * NOTE: Starting homework #2, add more comments here describing the overall function
 * performed by server ftp program
 * This includes, the list of ftp commands processed by server ftp.
 *
 */

#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* Diaz & Vulka reserved port number for control connection */
#define SERVER_FTP_PORT 2125 

#define DATA_FTP_PORT 2126 // Diaz & Vulka: Added data port constant


/* Error and OK codes */
#define OK 0
#define ER_INVALID_HOST_NAME -1
#define ER_CREATE_SOCKET_FAILED -2
#define ER_BIND_FAILED -3
#define ER_CONNECT_FAILED -4
#define ER_SEND_FAILED -5
#define ER_RECEIVE_FAILED -6

/* Function prototypes */
int clntConnect(char *serverName, int *s);
int svcInitServerData(int *s); // Diaz & Vulka: Adding prototype for svcInitServer but for data port
int sendMessage (int s, char *msg, int msgSize);
int receiveMessage(int s, char *buffer, int bufferSize, int *msgSize);

/* List of all global variables */
char userCmd[1024];	/* user typed ftp command line read from keyboard */
char cmd[1024];		/* ftp command extracted from userCmd */
char argument[1024];	/* argument extracted from userCmd */
char replyMsg[1024];    /* buffer to receive reply message from server */

/*
 * main
 *
 * Function connects to the ftp server using clntConnect function.
 * Reads one ftp command in one line from the keyboard into userCmd array.
 * Sends the user command to the server.
 * Receive reply message from the server.
 * On receiving reply to QUIT ftp command from the server,
 * close the control connection socket and exit from main
 *
 * Parameters
 * argc		- Count of number of arguments passed to main (input)
 * argv  	- Array of pointer to input parameters to main (input)
 *		   It is not required to pass any parameter to main
 *		   Can use it if needed.
 *
 * Return status
 *	OK	- Successful execution until QUIT command from client 
 *	N	- Failed status, value of N depends on the function called or cmd processed
 */
int main(int argc, char *argv[]) {
	/* List of local varibale */

	int ccSocket;	/* Control connection socket - to be used in all client communication */
	int msgSize;	/* size of the reply message received from the server */
	int status = OK;

	printf("Started execution of client ftp\n");

	/* Connect to server ftp */
	printf("Calling clntConnect to connect to the server\n");

	status=clntConnect("127.0.0.1", &ccSocket);
	if(status != 0)
	{
		printf("Connection to server failed, exiting main. \n");
		return (status);
	}

	int dataListenSocket = -1;   /* Diaz & Vulka: client's data-listen socket */
	
	/* Diaz & Vulka: After successful clntConnect(..,&ccSocket) */
	status = svcInitServerData(&dataListenSocket);
	if (status != OK) {
		printf("Data-listen init failed, exiting.\n");
		close(ccSocket);
		return status;
	}

	// Diaz & Vulka: Prompts user for their username, until valid username entered
	do {
		printf("Username: ");
		fgets(userCmd, sizeof(userCmd), stdin);
		userCmd[strcspn(userCmd, "\n")] = 0;
		snprintf(cmd, sizeof(cmd), "user %.*s", (int)(sizeof(cmd) - 6), userCmd);
		status = sendMessage(ccSocket, cmd, strlen(cmd) + 1);
		if(status != OK) break;
		status = receiveMessage(ccSocket, replyMsg, sizeof(replyMsg), &msgSize);
		if(status != OK) break;
	} while(strncmp(replyMsg, "331", 3) != 0);

	// Diaz & Vulka: If username entry is successful, password prompt is shown until correct password is entered
	do {
		printf("Password: ");
		fgets(userCmd, sizeof(userCmd), stdin);
		userCmd[strcspn(userCmd, "\n")] = 0;
		snprintf(cmd, sizeof(cmd), "pass %.*s", (int)(sizeof(cmd) - 6), userCmd);
		status = sendMessage(ccSocket, cmd, strlen(cmd) + 1);
		if(status != OK) break;
		status = receiveMessage(ccSocket, replyMsg, sizeof(replyMsg), &msgSize);
		if(status != OK) break;
	} while(strncmp(replyMsg, "230", 3) != 0);

	/* 
	 * Read an ftp command with argument, if any, in one line from user into userCmd.
	 * Copy ftp command part into ftpCmd and the argument into arg array.
 	 * Send the line read (both ftp cmd part and the argument part) in userCmd to server.
	 * Receive reply message from the server.
	 * until quit command is typed by the user.
	 */

	do {
		printf("my ftp> ");

		// Diaz & Vulka: Capture command provided by the user that will be sent to the server
		if (!fgets(userCmd, sizeof(userCmd), stdin)) break; 
		userCmd[strcspn(userCmd, "\n")] = 0; 
		
		/* send the userCmd to the server (send raw line BEFORE tokenizing) */
		status = sendMessage(ccSocket, userCmd, strlen(userCmd)+1);
		if (status != OK) {
			fprintf(stderr, "sendMessage(control) failed\n");
			break;
		}

		/* local parse copy for flow control */
		char lineCopy[sizeof userCmd];
		strncpy(lineCopy, userCmd, sizeof(lineCopy)-1);
		lineCopy[sizeof(lineCopy)-1] = '\0';
		char *tok = strtok(lineCopy, " ");
		char *fname = NULL;

		int repliesAlreadyRead = 0;  /* <-- if we consume replies inside a branch, skip generic read later */

		if (tok) {
			/* -------------------- SEND -------------------- */
			if (strcmp(tok, "send") == 0) {
				fname = strtok(NULL, " ");
				if (!fname) {
					printf("send: missing filename\n");
				} else {
					/* For symmetry/robustness: read first reply (should be 150) */
					int firstSize = 0;
					if (receiveMessage(ccSocket, replyMsg, sizeof(replyMsg), &firstSize) != OK) {
						fprintf(stderr, "receiveMessage(control) pre-send failed\n");
						break;
					}
					if (firstSize > 0) printf("%s\n", replyMsg);

					if (firstSize <= 0 || replyMsg[0] != '1') {
						/* Not preliminary -> do not accept(), just return to prompt */
						repliesAlreadyRead = 1; /* we already consumed server reply for this cmd */
					} else {
						/* wait for server to connect the data socket and upload the file */
						int dcSocket = accept(dataListenSocket, NULL, NULL);
						if (dcSocket < 0) {
							perror("accept (data)");
							/* try to read final reply (if any) so channel isn't stuck */
							int tmp = 0;
							if (receiveMessage(ccSocket, replyMsg, sizeof(replyMsg), &tmp) == OK && tmp > 0)
								printf("%s\n", replyMsg);
							repliesAlreadyRead = 1;
						} else {
							FILE *fp = fopen(fname, "r");  /* ASCII mode */
							if (!fp) {
								perror("fopen (send)");
								close(dcSocket);
								/* server may still send a final reply; read it */
								int tmp = 0;
								if (receiveMessage(ccSocket, replyMsg, sizeof(replyMsg), &tmp) == OK && tmp > 0)
									printf("%s\n", replyMsg);
								repliesAlreadyRead = 1;
							} else {
								char buffer[100];
								size_t n;
								int ok = 1;
								while ((n = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
									if (sendMessage(dcSocket, buffer, (int)n) != OK) {
										perror("sendMessage(data)");
										ok = 0;
										break;
									}
								}
								if (ferror(fp)) { perror("fread"); ok = 0; }
								fclose(fp);
								close(dcSocket);

								/* drain replies until final (non-1xx) */
								int done = 0;
								while (!done) {
									int sz = 0;
									if (receiveMessage(ccSocket, replyMsg, sizeof(replyMsg), &sz) != OK) {
										fprintf(stderr, "receiveMessage(control) post-send failed\n");
										break;
									}
									if (sz <= 0) break;
									printf("%s\n", replyMsg);
									if (replyMsg[0] != '1') done = 1;
								}
								repliesAlreadyRead = 1;
							}
						}
					}
				}
			}
			/* -------------------- RECV -------------------- */
			else if (strcmp(tok, "recv") == 0) {
				fname = strtok(NULL, " ");
				if (!fname) {
					printf("Usage: recv <text-file>\n");
				} else {
					int firstSize = 0;

					/* 1) Read the first control reply BEFORE opening data */
					if (receiveMessage(ccSocket, replyMsg, sizeof(replyMsg), &firstSize) != OK) {
						fprintf(stderr, "receiveMessage(control) after 'recv' failed\n");
						break;
					}
					if (firstSize > 0) printf("%s\n", replyMsg);

					/* If not preliminary (doesn't start with '1'), don't accept() – just return to prompt */
					if (firstSize <= 0 || replyMsg[0] != '1') {
						repliesAlreadyRead = 1; /* we consumed what server sent for this cmd */
					} else {
						/* 2) Preliminary was OK (150...) → accept data and receive file */
						int dcSocket = accept(dataListenSocket, NULL, NULL);
						if (dcSocket < 0) {
							perror("accept(data)");
							/* Try to read a final reply if server sent one, then return to prompt */
							int tmp = 0;
							if (receiveMessage(ccSocket, replyMsg, sizeof(replyMsg), &tmp) == OK && tmp > 0)
								printf("%s\n", replyMsg);
							repliesAlreadyRead = 1;
						} else {
							FILE *fp = fopen(fname, "w");  /* ASCII mode */
							if (!fp) {
								perror("fopen (recv)");
								close(dcSocket);
								/* Drain final reply so control channel isn't left hanging */
								int tmp = 0;
								if (receiveMessage(ccSocket, replyMsg, sizeof(replyMsg), &tmp) == OK && tmp > 0)
									printf("%s\n", replyMsg);
								repliesAlreadyRead = 1;
							} else {
								char buf[100];
								int got = 0;
								int ok = 1;
								do {
									if (receiveMessage(dcSocket, buf, sizeof(buf), &got) != OK) {
										fprintf(stderr, "receiveMessage(data) failed during download\n");
										ok = 0;
										break;
									}
									if (got > 0) {
										size_t w = fwrite(buf, 1, (size_t)got, fp);
										if (w != (size_t)got) {
											perror("fwrite");
											ok = 0;
											break;
										}
									}
								} while (got > 0);

								fclose(fp);
								close(dcSocket);

								/* 3) Drain control replies until final (non-1xx) so we don’t hang */
								int done = 0;
								while (!done) {
									int sz = 0;
									if (receiveMessage(ccSocket, replyMsg, sizeof(replyMsg), &sz) != OK) {
										fprintf(stderr, "receiveMessage(control) after data failed\n");
										break;
									}
									if (sz <= 0) break;
									printf("%s\n", replyMsg);
									if (replyMsg[0] != '1') done = 1;   /* final reply (e.g., 226/4xx/5xx) */
								}
								repliesAlreadyRead = 1;
							}
						}
					}
				}
			}
		}

		/* Tokenize original command (for loop control on 'quit') */
		char *token = strtok(userCmd, " "); 
		if(token != NULL) {			
			strcpy(cmd, token);
			token = strtok(NULL, " ");
			if(token != NULL) {
				strcpy(argument, token);
			}
			else {
				argument[0] = '\0';
			}
		} else {
			cmd[0] = '\0';
			argument[0] = '\0';
		}

		/* Receive generic reply from server ONLY if we didn't already consume replies */
		if (!repliesAlreadyRead) {
			status = receiveMessage(ccSocket, replyMsg, sizeof(replyMsg), &msgSize);
			if(status != OK) {
				break;
			}
			/* already printed inside receiveMessage */
		}

	} while (strcmp(cmd, "quit") != 0);

	printf("Closing control connection \n");
	close(ccSocket);  /* close control connection socket */

	printf("Exiting client main \n");

	return (status);

}  /* end main() */


/*
 * clntConnect
 *
 * Function to create a socket, bind local client IP address and port to the socket
 * and connect to the server
 *
 * Parameters
 * serverName	- IP address of server in dot notation (input)
 * s		- Control connection socket number (output)
 *
 * Return status
 *	OK			- Successfully connected to the server
 *	ER_INVALID_HOST_NAME	- Invalid server name
 *	ER_CREATE_SOCKET_FAILED	- Cannot create socket
 *	ER_BIND_FAILED		- bind failed
 *	ER_CONNECT_FAILED	- connect failed
 */


int clntConnect (
	char *serverName, /* server IP address in dot notation (input) */
	int *s 		  /* control connection socket number (output) */
	)
{
	int sock;	/* local variable to keep socket number */

	struct sockaddr_in clientAddress;  	/* local client IP address */
	struct sockaddr_in serverAddress;	/* server IP address */
	struct hostent	   *serverIPstructure;	/* host entry having server IP address in binary */


	/* Get IP address os server in binary from server name (IP in dot natation) */
	if((serverIPstructure = gethostbyname(serverName)) == NULL)
	{
		printf("%s is unknown server. \n", serverName);
		return (ER_INVALID_HOST_NAME);  /* error return */
	}

	/* Create control connection socket */
	if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("cannot create socket ");
		return (ER_CREATE_SOCKET_FAILED);	/* error return */
	}

	/* initialize client address structure memory to zero */
	memset((char *) &clientAddress, 0, sizeof(clientAddress));

	/* Set local client IP address, and port in the address structure */
	clientAddress.sin_family = AF_INET;	/* Internet protocol family */
	clientAddress.sin_addr.s_addr = htonl(INADDR_ANY);  /* INADDR_ANY is 0, which means */
						 /* let the system fill client IP address */
	clientAddress.sin_port = 0;  /* With port set to 0, system will allocate a free port */
			  /* from 1024 to (64K -1) */

	/* Associate the socket with local client IP address and port */
	if(bind(sock,(struct sockaddr *)&clientAddress,sizeof(clientAddress))<0)
	{
		perror("cannot bind");
		close(sock);
		return(ER_BIND_FAILED);	/* bind failed */
	}


	/* Initialize serverAddress memory to 0 */
	memset((char *) &serverAddress, 0, sizeof(serverAddress));

	/* Set ftp server ftp address in serverAddress */
	serverAddress.sin_family = AF_INET;
	memcpy((char *) &serverAddress.sin_addr, serverIPstructure->h_addr, 
			serverIPstructure->h_length);
	serverAddress.sin_port = htons(SERVER_FTP_PORT);

	/* Connect to the server */
	if (connect(sock, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) < 0)
	{
		perror("Cannot connect to server ");
		close (sock); 	/* close the control connection socket */
		return(ER_CONNECT_FAILED);  	/* error return */
	}


	/* Store listen socket number to be returned in output parameter 's' */
	*s=sock;

	return(OK); /* successful return */
}  // end of clntConnect() */

/* Diaz & Vulka: Listen for incoming data connections from the server on DATA_FTP_PORT */
int svcInitServerData(int *s) {
    int sock, qlen = 1;
    struct sockaddr_in svcAddr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("cannot create data listen socket");
        return ER_CREATE_SOCKET_FAILED;
    }
    memset((char *)&svcAddr, 0, sizeof(svcAddr));
    svcAddr.sin_family = AF_INET;
    svcAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    svcAddr.sin_port = htons(DATA_FTP_PORT);  

    if (bind(sock, (struct sockaddr *)&svcAddr, sizeof(svcAddr)) < 0) {
        perror("cannot bind data listen socket");
        close(sock);
        return ER_BIND_FAILED;
    }
    listen(sock, qlen);
    *s = sock;
    return OK;
}

/*
 * sendMessage
 *
 * Function to send specified number of octet (bytes) to client ftp
 *
 * Parameters
 * s		- Socket to be used to send msg to client (input)
 * msg  	- Pointer to character arrary containing msg to be sent (input)
 * msgSize	- Number of bytes, including NULL, in the msg to be sent to client (input)
 *
 * Return status
 *	OK		- Msg successfully sent
 *	ER_SEND_FAILED	- Sending msg failed
 */
int sendMessage(
	int s, 		/* socket to be used to send msg to client */
	char *msg, 	/*buffer having the message data */
	int msgSize 	/*size of the message/data in bytes */
	)
{
	int i;

	/* Print the message to be sent byte by byte as character */
	for(i=0;i<msgSize;i++)
	{
		printf("%c",msg[i]);
	}
	printf("\n");

	if((send(s,msg,msgSize,0)) < 0) /* socket interface call to transmit */
	{
		perror("unable to send ");
		return(ER_SEND_FAILED);
	}

	return(OK); /* successful send */
}


/*
 * receiveMessage
 *
 * Function to receive message from client ftp
 *
 * Parameters
 * s		- Socket to be used to receive msg from client (input)
 * buffer  	- Pointer to character arrary to store received msg (input/output)
 * bufferSize	- Maximum size of the array, "buffer" in octent/byte (input)
 *		    This is the maximum number of bytes that will be stored in buffer
 * msgSize	- Actual # of bytes received and stored in buffer in octet/byes (output)
 *
 * Return status
 *	OK			- Msg successfully received
 *	ER_RECEIVE_FAILED	- Receiving msg failed
 */

int receiveMessage (
	int s, 		/* socket */
	char *buffer, 	/* buffer to store received msg */
	int bufferSize, /* how large the buffer is in octet */
	int *msgSize 	/* size of the received msg in octet */
	)
{
	int i;

	*msgSize=recv(s,buffer,bufferSize,0); /* socket interface call to receive msg */

	if(*msgSize<0)
	{
		perror("unable to receive");
		return(ER_RECEIVE_FAILED);
	}

	/* Print the received msg byte by byte */
	for(i=0;i<*msgSize;i++)
	{
		printf("%c", buffer[i]);
	}
	printf("\n");

	return (OK);
}


/*
 * clntExtractReplyCode
 *
 * Function to extract the three digit reply code 
 * from the server reply message received.
 * It is assumed that the reply message string is of the following format
 *      ddd  text
 * where ddd is the three digit reply code followed by or or more space.
 *
 * Parameters
 *	buffer	  - Pointer to an array containing the reply message (input)
 *	replyCode - reply code number (output)
 *
 * Return status
 *	OK	- Successful (returns always success code
 */

int clntExtractReplyCode (
	char	*buffer,    /* Pointer to an array containing the reply message (input) */
	int	*replyCode  /* reply code (output) */
	)
{
	/* extract the codefrom the server reply message */
   sscanf(buffer, "%d", replyCode);

   return (OK);
}  // end of clntExtractReplyCode()
