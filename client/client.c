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

	int auth_sockfd,session_sockfd,sessionnum;
	char buffer[MAXLINE];
	char *hello = "Hello from client";
	struct sockaddr_in	 auth_serveraddr,cliaddr,session_serveraddr;
	
    //get passuser from commandline
    char *input = argv[1];
    char *mode = argv[2];
    char *filename = argv[3];
    char *auth = strtok(input,"@");
   
    //get ip and port from input
    char *ipport = strtok (NULL, "@");
    char *givenip = strtok(ipport,":");
    printf("ip = %s\n",givenip);
    char *givenport = strtok(NULL,":");
    printf("port = %s\n",givenport);

    //prepare auth structure 
    char *givenusername = strtok(auth,":");
    char *givenpassword = strtok(NULL,":");
    printf("username: %s password: %s\n",givenusername,givenpassword);

    char username[32];
    strcpy(username,givenusername);
    char password[32];
    strcpy(password,givenpassword);

    Auth givenAuth = {01,"",0,"",0};
    strcpy(givenAuth.username, username); 
    strcpy(givenAuth.password, password);

    
	// Creating socket file descriptor
	if ( (auth_sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
		perror("socket creation failed");
		exit(EXIT_FAILURE);
	}
	
	memset(&auth_serveraddr, 0, sizeof(auth_serveraddr));
		
	// Filling server information
	auth_serveraddr.sin_family = AF_INET;
	auth_serveraddr.sin_port = htons(atoi(givenport));
	auth_serveraddr.sin_addr.s_addr = inet_addr(givenip);

	cliaddr.sin_family = AF_INET;
    cliaddr.sin_addr.s_addr= htonl(INADDR_ANY);
    cliaddr.sin_port=htons(0); //source port for outgoing packets
    bind(auth_sockfd,(struct sockaddr *)&cliaddr,sizeof(cliaddr));

    //print random port number
    struct sockaddr_in sin;
    socklen_t lens = sizeof(sin);
    if (getsockname(auth_sockfd, (struct sockaddr *)&sin, &lens) != -1)
    printf("Client using and lisenting on port  %d\n", ntohs(sin.sin_port));

	int n, len;
	//send auth to client,	
	sendto(auth_sockfd, &givenAuth, sizeof(givenAuth),
		MSG_CONFIRM, (const struct sockaddr *) &auth_serveraddr,
			sizeof(auth_serveraddr));
	printf("Username password sent to server.\n");


    //receive response from server
    //temp sockaddr to get new port
    struct sockaddr_in client;
    len = sizeof(client);
	n = recvfrom(auth_sockfd, (char *)buffer, MAXLINE,
				MSG_WAITALL, (struct sockaddr *) &client,
				&len);

    //get new port and set to server point
    int port = ntohs(client.sin_port);
    auth_serveraddr.sin_port = htons(port);
	
    Ack authAck;
    memcpy(&authAck, buffer, sizeof(Ack));
    bool authenticated = false;

    if(authAck.opcode == 05){
        printf("autheticated by server, session num: %i\n",authAck.session);
        authenticated = true;
    }
    else{
       
        printf("password or username incorrect, exiting...\n");
        close(auth_sockfd);
	    return 0;
    }

    printf("Session Server using and lisenting on port %d\n", htons(auth_serveraddr.sin_port));

    //send command to server, based onuser
    Command command = {0,authAck.session,"",0};
    sessionnum = authAck.session;
    strcpy(command.filename, filename); 
    //printf("%s\n",filename);

    if(strcmp(mode,"read")==0){
        command.opcode = 02;
        //printf("%s\n",mode);
    }
    else if(strcmp(mode,"write")==0){
        command.opcode = 03;
        //printf("%s\n",mode);
    }
	else{
        printf("mode not supported, exiting ... ");
        close(auth_sockfd);
	    return 0;
    }
    sendto(auth_sockfd, &command, sizeof(command),
		MSG_CONFIRM, (const struct sockaddr *) &auth_serveraddr,
			sizeof(auth_serveraddr));

    if(command.opcode == 02){

        FILE* filePointer;
		if(!(filePointer = fopen(command.filename, "wb"))){
            printf("file doesnt exist\n");
            return 0;
        }
        /*
        struct timeval tv;
        tv.tv_sec = 5; //timeout 5 seconds
        tv.tv_usec = 0;
        if(setsockopt(auth_sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
            printf("Cannot Set SO_SNDTIMEO for socket\n");
        */

        int dlen;
        int inside = 0;
        int count = 0;
        while(dlen = recvfrom(auth_sockfd, (char *)buffer, MAXLINE,
				MSG_WAITALL, (struct sockaddr *) &client,
				&len) > 0){

            inside = 1;
            Data received;
            memcpy(&received, buffer, sizeof(Data));

            if(received.opcode == 6){
                Error error;
                memcpy(&error, buffer, sizeof(Error));

                printf("error from server, %s ,exiting...\n ",error.message);
                return 0;
            }
            if(received.block == 0){
                break;
            }
            //printf("size of data %i\n",received.session);
            if(showRe == true && count < reTimes){
                
            //printf("block %i segment %i: %s\n",received.block,received.segment,received.data);

                count++;
                continue;
            }
            fwrite(received.data,received.session,sizeof(char),filePointer);
            Ack ackData = {04,authAck.session,received.block,received.segment};
            sendto(auth_sockfd, &ackData, sizeof(ackData),
		        MSG_CONFIRM, (const struct sockaddr *) &auth_serveraddr,
			    sizeof(auth_serveraddr));
        }
        if(inside == 0){
            printf("file not found, exiting...\n");
            return 0;
        }
        printf("file recieved\n");
        fclose(filePointer);
    }

    if(command.opcode==03){

        while(1){

            recvfrom(auth_sockfd, (char *)buffer, MAXLINE,
                    MSG_WAITALL, (struct sockaddr *) &auth_serveraddr,
                    &len);
            Ack ackF;
            memcpy(&ackF, buffer, sizeof(Ack));

            if(ackF.opcode == 5){
                break;
            }
        }

        FILE* filePointer;
        if(!(filePointer = fopen(command.filename, "rb"))){
            printf("file does not exist, exiting ... \n");
            Error authError = {06,"",0};
            sendto(auth_sockfd, &authError, sizeof(authError),
                MSG_CONFIRM, (const struct sockaddr *) &auth_serveraddr,
                len);
            return 0;
        }
        //printf("sent234\n");
        int block = 1;
        int segment = 1;
        int retrans = 0;

        struct timeval tv;
        tv.tv_sec = 5; //timeout 5 seconds
        tv.tv_usec = 0;
        if(setsockopt(auth_sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
            printf("Cannot Set SO_SNDTIMEO for socket\n");
        
        fseek(filePointer, 0, SEEK_END);
        int file_size = ftell(filePointer);
        fseek(filePointer, 0, SEEK_SET);

        //printf("file %i %s\n",file_size,command.filename);
        
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
            Data dataS = {04,sessionnum,block,segment,""};

            dlen = fread(dataS.data, 1, read,filePointer);
        
            //printf("reaminging %i\n",remaining);
            remaining = remaining - 1024;
           
            dataS.session = dlen;

            int counter = 0;
            while(1){
                if(counter == 3){
                    break;
                }
    
                sendto(auth_sockfd, &dataS, sizeof(Data),
                    MSG_CONFIRM, (const struct sockaddr *) &auth_serveraddr,
                    len);

                //sleep(0.9);
                
                int rlen = recvfrom(auth_sockfd, (char *)buffer, MAXLINE,
                    MSG_WAITALL, ( struct sockaddr *) &auth_serveraddr,
                    &len);
                
                Ack dataAck;
                memcpy(&dataAck, buffer, sizeof(dataAck));
                if(rlen <=0){
                    counter ++;
                    printf("not acknowledged, resend data and wait... \n");
                    continue;
                }

                if(dataAck.block == block && dataAck.segment == segment){
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
                printf("packet acknowledge missed 3+ times, exiting...\n");
                Error authError = {06,"",0};
                sendto(auth_sockfd, &authError, sizeof(authError),
                    MSG_CONFIRM, (const struct sockaddr *) &auth_serveraddr,
                        len);
                return 0;
            }
        }
        Data data = {04,0,0,segment,""};
        sendto(auth_sockfd, &data, sizeof(Data),
            MSG_CONFIRM, (const struct sockaddr *) &auth_serveraddr,
                len);
        printf("file transfer done\n");

        fclose(filePointer);
    }
    //close(auth_sockfd);
	return 0;
}
//./client user:p3ass@127.0.0.1:8080 read snooze.mp3

//./client user:pass@127.0.0.1:8080 read 4KB.txt
//./client user:pass@127.0.0.1:8080 read 4KB.txt3
//./client user:pass@127.0.0.1:8080 read 1bit.txt
// ./client user:pass@127.0.0.1:8080 read 1KB.txt
//./client user:pass@127.0.0.1:8080 read test.txt

//./client user:pass@127.0.0.1:8080 read hare.jpg
//./client user:pass@127.0.0.1:8080 read snooze.mp3
//./client user:pass@127.0.0.1:8080 read snowfall.mp4

//./client user:pass@127.0.0.1:8080 write 1bit.txt
//./client user:pass@127.0.0.1:8080 write test.txt
//./client user:pass@127.0.0.1:8080 write hare.jpg
//./client user:pass@127.0.0.1:8080 write snooze.mp3
//./client user:pass@127.0.0.1:8080 write 4KB.txt