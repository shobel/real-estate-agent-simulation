#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>

#include <sstream>
#include <signal.h>
#include <fstream>
#include <iostream>

using namespace std;

#define BACKLOG 10
#define TCP_PORT_AGENT1 "4464"
#define TCP_PORT_AGENT2 "4564"
#define MAXDATASIZE 1000
#define MSGSIZE 100 

const char* sellerFiles[] = {"sellerA.txt", "sellerB.txt", "sellerC.txt", "sellerD.txt"};
const char* sellerTCPPorts[] = {"4064", "4164", "4264", "4364"};

const char* ip = "127.0.0.1";

void sigchld_handler(int s) {
    int saved_errno = errno;
    while(waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}

void *get_in_addr(struct sockaddr *sa){
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

string getValueFromLine(string line){
	string delimiter = ":";
	int index = line.find(delimiter);
	string token = line.substr(line.find(delimiter)+2, line.length()-1);
	return token;
}

/* connect to agent and return socket fd */
int connectToAgent(int sellerNum){
	int sockfd, rv, agentNum; 
	const char* port;
	char s[INET_ADDRSTRLEN];
	struct addrinfo hints, *servinfo, *p;

  	if (sellerNum < 2){
    	port = TCP_PORT_AGENT1;
    	agentNum = 0;
    } else {
    	port = TCP_PORT_AGENT2;
    	agentNum = 1;
   	}

   	memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    // get address info of localhost (NULL node) and port 
    if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
       	return -1;
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
   		if ((sockfd = socket(p->ai_family, p->ai_socktype,
       		p->ai_protocol)) == -1) {
           	//perror("CLIENT: socket");
           	continue;
        }
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      		close(sockfd);
      		//perror("CLIENT: connect");
       		continue;
       	}
        break;
	}

	if (p == NULL) {
		printf("Seller%d: could not connect to agent on port %s\n", sellerNum, port);
       	return -1;
    }

    ip = inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
    const char* sellerport = sellerTCPPorts[sellerNum];
    printf("<Seller%d> has TCP port %s and IP address %s for Phase 1 part 1\n", sellerNum, sellerport, ip);
    printf("<Seller%d> is now connected to the <agent%d>\n", sellerNum, agentNum);
    freeaddrinfo(servinfo); // all done with this structure

    return sockfd;
}

int createTCPSocket(int sellerNumber){
	int sockfd, new_fd;  
    struct addrinfo hints, *servinfo, *p;
    const char* port;
    struct sigaction sa;
    int yes=1;
    char s[INET_ADDRSTRLEN];
    int rv;

	memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

  	port = sellerTCPPorts[sellerNumber];
    // get address info of localhost (NULL node) and port
    if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("SERVER: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("SERVER: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL)  {
        fprintf(stderr, "SERVER: failed to bind\n");
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    //printf("Seller%d: listening for agents on port %s\n", sellerNumber, port);
    return sockfd;
}

int main(int argc, char *argv[]){ 
    char buf[MAXDATASIZE];
    
    int numSellers = (sizeof(sellerFiles)/sizeof(*sellerFiles));
    for (int i = 0; i < numSellers; i++){
    	if (!fork()) {
    		int numbytes, sockfd, new_fd;
    		char s[INET_ADDRSTRLEN];
    		socklen_t sin_size;	
    		struct sockaddr_storage their_addr;

    		sockfd = connectToAgent(i);
    		if (sockfd == -1){
    			exit(1);
    		}
    		
    		//get data from file
    		ifstream infile;
    		string filename = sellerFiles[i];
    		infile.open(filename.c_str());

    		string line;
    		int listingPrice, squareFootage;
    		stringstream sstm;
			sstm << "seller" << i << ":";
			string result = sstm.str();
    		//string message = "seller" + to_string(i) + ":"; 
    		string message = result;
    		while (getline(infile, line)) {
    			if (line.length() == 0){
    				break;
    			}
    			message += getValueFromLine(line) + ":";
    		}
    		//message.pop_back();
    		message.erase(message.size() - 1);

    		infile.close();
			if (send(sockfd, message.c_str(), MSGSIZE, 0) == -1){
        		perror("error: client sending");
        		exit(1);
    		}   
    		printf("<Seller%d> has sent %s to the agent\n",i, message.c_str());
    		printf("End of Phase 1 part 1 for <Seller%d>\n", i);
    		printf("<Seller%d> has TCP port %s and IP address 127.0.0.1 for Phase 1 part 4\n", i, sellerTCPPorts[i]);

    		sockfd = createTCPSocket(i);
    		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
    		if (new_fd == -1) {
            	perror("accept");
            	continue;
        	}
        	inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);

    		if ((numbytes = recv(new_fd, buf, MAXDATASIZE, 0)) != -1) {
    			if (strcmp(buf, "NAK") == 0){
    				printf("NAK\n");
    			} else {
    				printf("<Seller%d> <%s> buy my house\n", i, buf);
    			}
        	}
        	printf("End of Phase 1 part 4 for <Seller%d>\n", i);
    		close(new_fd);
    		close(sockfd);

    		exit(0);
    	}
    }

    while(1){

    }
}


