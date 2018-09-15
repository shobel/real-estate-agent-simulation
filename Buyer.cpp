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

#include <signal.h>
#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
using namespace std;

#define BACKLOG 10
#define MAXDATASIZE 1000
#define TCP_PORT_AGENT1 "4464"
#define TCP_PORT_AGENT2 "4564"
#define NUM_AGENTS 2
#define MAXBUFLEN 100
#define NUM_SELLERS 4
#define MSGSIZE 100

const char* buyerFiles[] = {"buyer1.txt", "buyer2.txt", "buyer3.txt", "buyer4.txt", "buyer5.txt"};
const char* buyerUDPPorts[] = {"22164", "22264", "22364", "22464", "22564"};
const char* buyerTCPPorts[] = {"4664", "4764", "4864", "4964", "5064"};

struct Listing {  
	string sellername; 
    int price;   
    int sqfootage;  
};

struct less_than_price{
    inline bool operator() (const Listing& struct1, const Listing& struct2){
        return (struct1.price < struct2.price);
    }
}; 

string getValueFromLine(string line){
	string delimiter = ":";
	int index = line.find(delimiter);
	string token = line.substr(line.find(delimiter)+2, line.length()-1);
	return token;
}

void sigchld_handler(int s) {
    int saved_errno = errno;
    while(waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}

void printListing(Listing listing, int i){
	printf("Buyer%d rankings:: seller: %s; price: %d; sqft:%d\n", i, listing.sellername.c_str(), listing.price, listing.sqfootage);
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa){
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int createUDPSocket(int buyerNumber){
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	const char* port;

	memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    port = buyerUDPPorts[buyerNumber];
	if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
       	return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("listener: socket");
            continue;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("listener: bind");
            continue;
        }
        break;
    }

    if (p == NULL) {
        fprintf(stderr, "listener: failed to bind socket\n");
        return -1;
    }

    freeaddrinfo(servinfo);
    //printf("Buyer%d listening for agents on port %s\n", buyerNumber, port);
    printf("The <Buyer%d> has UDP port %s and IP address for Phase 1 part 2\n", buyerNumber, port);
    return sockfd;
}

int connectToAgent(string agent, int buyernum){
	int sockfd, rv; 
	const char* port;
	char s[INET_ADDRSTRLEN];
	struct addrinfo hints, *servinfo, *p;

  	if (agent == "agent1"){
    	port = TCP_PORT_AGENT1;
    } else {
    	port = TCP_PORT_AGENT2;
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
		printf("Buyer%d could not connect to agent on port %s\n", buyernum, port);
       	return -1;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
    //printf("Buyer%d connected to agent on port %s\n", buyernum, port);
    freeaddrinfo(servinfo); // all done with this structure

    return sockfd;
}

int createTCPSocket(int buyerNumber){
	int sockfd;  
    struct addrinfo hints, *servinfo, *p;
    const char* port;
    struct sigaction sa;
    int yes=1;
    char s[INET_ADDRSTRLEN];
    int rv;

	memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

  	port = buyerTCPPorts[buyerNumber];
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

    printf("The <Buyer%d> has TCP port %s and IP address %s for Phase 1 part 3\n", buyerNumber, port, "127.0.0.1");
    return sockfd;
}

int main(void){

	int numBuyers = (sizeof(buyerUDPPorts)/sizeof(*buyerUDPPorts));
    for (int i = 0; i < numBuyers; i++){
    	if (!fork()){
    		int budget, sqfootagemin;
    		int numbytes, udpsockfd, tcpsockfd, new_fd;
    		struct sockaddr_storage their_addr;
   			char buf[MAXBUFLEN];
    		socklen_t addr_len, sin_size;
    		char s[INET6_ADDRSTRLEN];
    		string replyTo;

    		udpsockfd = createUDPSocket(i);
    		addr_len = sizeof their_addr;

    		int expectedResponses = 0;
    		int agentsReceived = 0;
    		while (agentsReceived < NUM_AGENTS){
    			vector<Listing> listings;
    			vector<Listing> acceptableListings;

	    		if ((numbytes = recvfrom(udpsockfd, buf, MAXBUFLEN-1 , 0, (struct sockaddr *)&their_addr, &addr_len)) == -1) {
	        		perror("recvfrom");
	        		exit(1);
	    		}

	    		//printf("Buyer%d got packet from %s\n", i, inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s));
	    		inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);

	    		buf[numbytes] = '\0';
	    		char *line;
	  			char *token;
	  			char buff[256];
	  			for (line = strtok(buf, ";"); line != NULL; line = strtok(line + strlen(line) + 1, "\n")) {
	      			strncpy(buff, line, sizeof(buf));
	      			int counter = 0;
	      			Listing listing;
	      			for (token = strtok(buff, ":"); token != NULL; token = strtok(token + strlen(token) + 1, ":")){
	      				string tokenString(token);
	        			if (counter == 0){
	        				listing.sellername = tokenString;
	        			} else if (counter == 1){
	        				istringstream iss(tokenString);
	        				iss >> listing.price;
	        				iss.clear(); iss.str("");
	        			} else if (counter == 2){
	        				istringstream iss(tokenString);
	        				iss >> listing.sqfootage;
	        				iss.clear(); iss.str("");
	        			}
	        			counter++;
	        		}
	        		if (listing.sellername == "seller0" || listing.sellername == "seller1"){
	        			replyTo = "agent1";
	        		} else {
	        			replyTo = "agent2";
	        		}
	        		listings.push_back(listing);
	  			}
	  			printf("<Buyer%d> received house information from <%s>\n", i, replyTo.c_str());
	  			printf("End of Phase 1 part 2 for <Buyer%d>\n", i);

	 		  	//get data from file
	    		ifstream infile;
	    		string filename = buyerFiles[i];
	    		infile.open(filename.c_str());
	    		//printf("reading file %s\n", filename.c_str());
	    		string fileline;
	    		int counter = 0;
	    		while (getline(infile, fileline)) {
	    			if (fileline.length() == 0){
	    				break;
	    			}
	    			if (counter == 0){
	    				istringstream iss(getValueFromLine(fileline));
	        			iss >> sqfootagemin;
	    			} else if (counter == 1){
	    				istringstream iss(getValueFromLine(fileline));
	        			iss >> budget;
	    			}
	    			counter++;
	    		}
	    		infile.close();

	    		//printf("Buyer%d budget of %d and sqft min of %d\n", i, budget, sqfootagemin);

				Listing l;
				int found = 0;
				int x;
	    		for(x=0; x < listings.size(); x++){
	    			l = listings[x];
	    			//printf("Buyer%d: %d <= %d ? and %d >= %d ?\n", i, l.price, budget, l.sqfootage, sqfootagemin);
	    			if (l.price <= budget && l.sqfootage >= sqfootagemin){
	    				//printf("Buyer%d: %d <= %d ? and %d >= %d ?\n", i, l.price, budget, l.sqfootage, sqfootagemin);
	    				acceptableListings.push_back(l);
	    				found++;
	    			}
				}
				//printf("Buyer%d found %d acceptable listings\n", i, found);
				expectedResponses+= found;

				//printf("Buyer%d sorting %zu acceptable listings\n", i, acceptableListings.size());
				sort(acceptableListings.begin(), acceptableListings.end(), less_than_price());
				for(x=0; x < acceptableListings.size(); x++){
	    			l = acceptableListings[x];
	    			//printListing(l, i);
				}

				tcpsockfd = connectToAgent(replyTo, i);
				stringstream sstm;
				sstm << "buyer" << i << ":" << budget;
				string result = sstm.str();
				string messageToAgent = result;
				if (acceptableListings.size() > 0){
					for(x=0; x < acceptableListings.size(); x++){
	    				l = acceptableListings[x];
	    				messageToAgent += ":" + l.sellername;
					}
				} 
				if (send(tcpsockfd, messageToAgent.c_str(), MSGSIZE, 0) == -1){
	        		perror("error: client sending");
	        		exit(1);
	    		}
	    		printf("<Buyer%d> has sent %s to <%s>\n",i , messageToAgent.c_str(), replyTo.c_str());
	    		agentsReceived++;

	    	}
	    	printf("End of Phase 1 part 3 for <Buyer%d>\n", i);
    		close(udpsockfd);

    		//printf("The <Buyer%d> has TCP port %s and IP address %s for Phase 1 part 4\n", i, buyerTCPPorts[i], "127.0.0.1");
    		int sockfd = createTCPSocket(i);
    		sin_size = sizeof their_addr;
    		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
    		if (new_fd == -1) {
            	printf("<Buyer%d> accept error\n", i);
            	continue;
        	}
        	inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);

        	//printf("Buyer%d expects %d responses\n", i, expectedResponses);
        	int r = 0;
        	while (r < expectedResponses){
    			if ((numbytes = recv(new_fd, buf, MAXDATASIZE, 0)) != -1) {
    				if (strcmp(buf, "NAK") == 0){
    					printf("NAK\n");
    				} else {
    					printf("<Buyer%d> will buy house from <%s>\n", i, buf);
    				}
        			//printf("Buyer%d received %s from agent\n", i, buf);
        			r++;
        		}
        		buf[numbytes] = '\0';
        		if (numbytes == 0){
        			//printf("Buyer%d received nothing from agent\n", i);
        		}
        		//usleep(1000000);
    		}
    		printf("End of Phase 1 for part 4 for <Buyer%d>\n", i);
    		close(sockfd);
    		close(new_fd);

    		exit(0);	
    	}
    }  
    while(1){

    }
}
