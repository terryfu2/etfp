#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <time.h>
	
//#define PORT	 8080
#define MAXLINE 1055

typedef struct {
    uint16_t opcode;
    char username[32];
    uint8_t username_len;
    char password[32];
    uint8_t password_len;

}Auth;

typedef struct {
    uint16_t opcode;
    char message[512];
    uint8_t message_len;

}Error;

typedef struct {
    uint16_t opcode;
    uint16_t session;
    uint16_t block;
    uint8_t segment;

}Ack;
typedef struct {
    uint16_t opcode;
    uint16_t session;
    char filename[255];
    uint8_t filename_len;

}Command;

typedef struct {
    uint16_t opcode;
    uint16_t session;
    uint16_t block;
    uint8_t segment;
    uint8_t data[1024];

}Data;

// Driver code
int main(int argc, char *argv[]) {

    bool showRe = false;
    int reTimes = 2;
    
    int len, n;
	int auth_sockfd, session_sockfd;
	char buffer[MAXLINE];
	//char *error = "ERROR from server";
	struct sockaddr_in auth_serveraddr, cliaddr, session_serveraddr;
	int sessionnum;

    char *auth = argv[1];
    int i = 0;
    char *p = strtok (auth, ":");
    char *array[3];

    while (p != NULL)
    {
        array[i++] = p;
        p = strtok (NULL, ":");
    }
    printf("Username: %s\n",array[0]);
    printf("Password: %s\n",array[1]);

    char *input = argv[2];
	int givenPort = atoi(input);   

    char *workingDir = argv[3];
	// Creating socket file descriptor
    while(1){
        if ( (auth_sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
            perror("socket creation failed");
            exit(EXIT_FAILURE);
        }
            
        memset(&auth_serveraddr, 0, sizeof(auth_serveraddr));
        memset(&cliaddr, 0, sizeof(cliaddr));
            
        // Filling server information
        auth_serveraddr.sin_family = AF_INET; // IPv4
        auth_serveraddr.sin_addr.s_addr = INADDR_ANY;
        auth_serveraddr.sin_port = htons(givenPort);
            
        // Bind the socket with the server address
        if ( bind(auth_sockfd, (const struct sockaddr *)&auth_serveraddr,
                sizeof(auth_serveraddr)) < 0 )
        {
            perror("bind failed");
            exit(EXIT_FAILURE);
        }
        //get port info and print

        printf("Auth Server lisenting on port %i\n",givenPort);	
        //main fun
        bool authenticated = false;

        while(!authenticated){
            
        
            
            len = sizeof(cliaddr); 
            //get auth and compare to stored
            n = recvfrom(auth_sockfd, (char *)buffer, MAXLINE,
                        MSG_WAITALL, ( struct sockaddr *) &cliaddr,
                        &len);

            
            Auth recievedCredentials;
            memcpy(&recievedCredentials, buffer, sizeof(Auth));


            if(recievedCredentials.opcode!=01){
                continue;
            }
            if(strcmp(array[0],recievedCredentials.username)==0 && strcmp(array[1],recievedCredentials.password)==0){
                printf("Authentication Successful\n");
                authenticated = true;
            }
            //password,username isnt correct, send error
            else{
                printf("Not Authenticated\n");
                Error authError = {06,"",0};
                char *error = "incorrect password or username";
                strcpy(authError.message,error);
                sendto(auth_sockfd, &authError, sizeof(authError),
                    MSG_CONFIRM, (const struct sockaddr *) &cliaddr,
                        len);
                close(auth_sockfd);
                break;
            }

            
        }
        if(authenticated){
                //create sessions socket, server
            close(auth_sockfd);
            if ( (session_sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
                perror("socket creation failed");
                exit(EXIT_FAILURE);
            }
            memset(&session_serveraddr, 0, sizeof(session_serveraddr));
            // Filling server information
            session_serveraddr.sin_family = AF_INET; // IPv4
            session_serveraddr.sin_addr.s_addr = INADDR_ANY;
            session_serveraddr.sin_port = htons(0);
                
            // Bind the socket with the server address
            if ( bind(session_sockfd, (const struct sockaddr *)&session_serveraddr,
                    sizeof(session_serveraddr)) < 0 )
            {
                perror("bind failed");
                exit(EXIT_FAILURE);
            }

            //print random port number
            struct sockaddr_in sin;
            socklen_t lens = sizeof(sin);
            if (getsockname(session_sockfd, (struct sockaddr *)&sin, &lens) != -1)
            printf("Session Server using and lisenting on port  %d\n", ntohs(sin.sin_port));

            //generate session number and send
            srand(time(0));
            sessionnum = (rand() % 65535) + 1;
            printf("session number: %i\n",sessionnum);

            Ack sessionAck = {05,sessionnum,0,0};
            sendto(session_sockfd, &sessionAck, sizeof(sessionAck),
                MSG_CONFIRM, (const struct sockaddr *) &cliaddr,
                    len);
            
            recvfrom(session_sockfd, (char *)buffer, MAXLINE,
                        MSG_WAITALL, ( struct sockaddr *) &cliaddr,
                        &len);

            Command command;
            memcpy(&command, buffer, sizeof(Command));

            if(command.session !=sessionnum){
                printf("Recieved wrong session num\n");
                Error authError = {06,"",0};
                char *error = "Recieved wrong session num";
                strcpy(authError.message,error);
                sendto(auth_sockfd, &authError, sizeof(authError),
                    MSG_CONFIRM, (const struct sockaddr *) &cliaddr,
                        len);
            }


            if(command.opcode == 02){

                FILE* filePointer;
               
                char dest[124];
                strcpy( dest, workingDir );
                strcat( dest, "/" );
                strcat(dest,command.filename);
                if(!(filePointer = fopen(dest, "rb"))){
                    printf("file does not exist\n");
                    Error authError = {06,"",0};
                    char *error = "file does not exist";
                    strcpy(authError.message,error);
                    sendto(auth_sockfd, &authError, sizeof(authError),
                        MSG_CONFIRM, (const struct sockaddr *) &cliaddr,
                            len);
                    authenticated = false;
                    continue;
                }

                int block = 1;
                int segment = 1;
                int retrans = 0;

                struct timeval tv;
                tv.tv_sec = 5; //timeout 5 seconds
                tv.tv_usec = 0;
                if(setsockopt(session_sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
                    printf("Cannot Set SO_SNDTIMEO for socket\n");
                
                fseek(filePointer, 0, SEEK_END);
                int file_size = ftell(filePointer);
                fseek(filePointer, 0, SEEK_SET);

                //printf("file %i\n",file_size);
                
                int dlen;
                
                int remaining = file_size;
                int read = 1024;

                while(1) {
                    //printf("send : %s\n", buff);
                    if(remaining < 1024){
                        read = remaining;
                        //printf("asdfasdf\n");
                    }
                    if(remaining <= 0){
                        break;
                    }
                    
                    //printf("read %i\n",read);
                    uint8_t dataB[read];
                    Data dataS = {04,0,block,segment,""};

                    memset(dataB,0,read);
                    dlen = fread(dataS.data, 1, read,filePointer);
                    dataB[dlen] = '\0';
                    

                    remaining = remaining - 1024;
                    
                    dataS.session = dlen;

                    int counter = 0;
                    while(1){
                        if(counter == 3){
                            break;
                        }
                        sendto(session_sockfd, &dataS, sizeof(Data),
                            MSG_CONFIRM, (const struct sockaddr *) &cliaddr,
                            len);

                        //sleep(0.9);
                        
                        int rlen = recvfrom(session_sockfd, (char *)buffer, MAXLINE,
                            MSG_WAITALL, ( struct sockaddr *) &cliaddr,
                            &len);
                        
                        if(rlen <=0){
                            counter ++;
                            printf("not acknowledged, resend data and wait... \n");
                            continue;
                        }
                        Ack dataAck;
                        memcpy(&dataAck, buffer, sizeof(dataAck));

                        if(dataAck.block == block && dataAck.segment == segment && dataAck.session == sessionnum){
                            retrans = 0;
                            segment = segment + 1;
                            if(segment == 9){
                                block += 1;
                                segment = 1;
                            }
                            printf("ack recieved for %i %i\n",dataAck.block,dataAck.segment);
                        }
                        else{
                            printf("ack not recieved, retrying...\n");
                            retrans = retrans + 1;
                        }
                        break;
                    }
                    if(counter >= 3){
                        printf("packet acknowledge missed 3+ times, terminating connection...\n");
                        Error authError = {06,"",0};
                        char *error = "packet acknowledge missed 3+ times";
                        strcpy(authError.message,error);
                        sendto(auth_sockfd, &authError, sizeof(authError),
                            MSG_CONFIRM, (const struct sockaddr *) &cliaddr,
                                len);
                        break;
                    }
                }
                Data data = {04,0,0,segment,""};
                sendto(session_sockfd, &data, sizeof(Data),
                    MSG_CONFIRM, (const struct sockaddr *) &cliaddr,
                        len);
                printf("client terminated\n");

                fclose(filePointer);
                
            }
            
            if(command.opcode == 03){
                Ack ackF = {05,0,1,0};
                sendto(session_sockfd, &ackF, sizeof(Ack),
                    MSG_CONFIRM, (const struct sockaddr *) &cliaddr,
                        len);
                
                FILE* filePointer;
                char dest[124];
                strcpy( dest, workingDir );
                strcat( dest, "/" );
                strcat(dest,command.filename);
               
                if(!( filePointer = fopen(dest, "wb"))){
                    printf("file does not exist\n");
                    Error authError = {06,"",0};
                    char *error = "file does not exist";

                    strcpy(authError.message,error);
                    sendto(auth_sockfd, &authError, sizeof(authError),
                        MSG_CONFIRM, (const struct sockaddr *) &cliaddr,
                            len);
                    authenticated = false;
                    continue;
                }
                struct timeval tv;
                tv.tv_sec = 5; //timeout 5 seconds
                tv.tv_usec = 0;
                if(setsockopt(session_sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
                    printf("Cannot Set SO_SNDTIMEO for socket\n");

                int dlen;
                int packeterror = 0;
                int count = 0;
                while(dlen = recvfrom(session_sockfd, (char *)buffer, MAXLINE,
                        MSG_WAITALL, (struct sockaddr *) &cliaddr,
                        &len) ){

                    if(count == 3){
                        break;
                    }
                    
                    Data received;
                    memcpy(&received, buffer, sizeof(Data));

                    if(received.opcode == 6){
                        printf("client error, terminating connection\n");
                        packeterror = 1;
                        break;
                    }
                    if(received.block == 0){
                        break;
                    }
                    if(showRe == true && count < reTimes){
                
                    //printf("block %i segment %i: %s\n",received.block,received.segment,received.data);

                        count++;
                        continue;
                    }
                    //printf("size of data %i\n",received.session);
                    fwrite(received.data,sizeof(char),received.session,filePointer);
                    //printf("block %i segment %i: %s\n",received.block,received.segment,received.data);
                    
                    Ack ackData = {04,0,received.block,received.segment};
                    sendto(session_sockfd, &ackData, sizeof(ackData),
                        MSG_CONFIRM, (const struct sockaddr *) &cliaddr,
                        len);
                    
                    if(dlen < 1032){
                        break;
                    }
                }
                
                fclose(filePointer);

            }
        }
    }
	return 0;
}
//./server user:pass 8080 ../testcases/text
//./server user:pass 8080 ../testcases/binaries
// ./server user:pass 8080 ../testcases/test