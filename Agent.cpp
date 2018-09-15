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
#include <iostream>
#include <vector>
using namespace std;

#define BACKLOG 10
#define MAXDATASIZE 1000
#define MSGSIZE 100
#define NUM_AGENTS 2
#define NUM_SELLERS_PER_AGENT 2
#define TCP_PORT_AGENT1 "4464"
#define TCP_PORT_AGENT2 "4564"
const char* sellerTCPPorts[] = {"4064", "4164", "4264", "4364"};
const char* buyerTCPPorts[] = {"4664", "4764", "4864", "4964", "5064"};
const char* buyerUDPPorts[] = {"22164", "22264", "22364", "22464", "22564"};

struct Listing {  
	string sellername; 
    int price;   
    int sqfootage;  
}; 

struct BuyerInfo {
	string buyername;
	int budget;
	vector<string> sellers;
	string buyFrom;
};

struct higher_budget{
    inline bool operator() (const BuyerInfo& struct1, const BuyerInfo& struct2){
        return (struct1.budget > struct2.budget);
    }
}; 

void printListing(Listing listing){
	cout << "seller: " + listing.sellername + "\n";
  	cout << "price: ";
  	cout << listing.price;
  	cout << "\n";
  	cout << "sqfootage: ";
  	cout << listing.sqfootage;
  	cout << "\n";
  	cout << "\n";
}

BuyerInfo messageToBuyer(char* message){
	BuyerInfo buyer;
	buyer.buyFrom = "";

   	const char d[2] = ":";
   	char *token;
	token = strtok(message, d);
   
   	int counter = 0;
   	while( token != NULL ) {    
    	string strtoken(token);
    	if (counter == 0){
    		buyer.buyername = strtoken;
    	} else if (counter == 1){
    		istringstream iss(strtoken);  
			iss >> buyer.budget;
			iss.clear(); iss.str("");
    	} else if (counter >= 2){
    		buyer.sellers.push_back(strtoken);
    	}
    	token = strtok(NULL, d);
    	counter++;
   	}   
   return buyer;
}

string messageFromListings(vector<Listing> listings){
	string message = "";
	for(int i=0; i < listings.size(); i++){
		Listing listing = listings[i];
		stringstream sstm;
		sstm << listing.sellername << ":" << listing.price << ":" << listing.sqfootage << ";";
		string result = sstm.str();
    	message += result;
	}
	//message.pop_back();
	message.erase(message.size() - 1);
	return message;
}

Listing listingFromMessage(char* message){
	Listing listing;
   	const char d[2] = ":";
   	char *token;
	token = strtok(message, d);
   
   	int counter = 0;
   	while( token != NULL ) {    
    	string strtoken(token);
    	if (counter == 0){
    		listing.sellername = strtoken;
    	} else if (counter == 1){
    		istringstream iss(strtoken);  
			iss >> listing.price;
			iss.clear(); iss.str("");
    	} else if (counter == 2){
    		istringstream iss(strtoken);
    		iss >> listing.sqfootage;
    		iss.clear(); iss.str("");
    	}
    	token = strtok(NULL, d);
    	counter++;
   	}   
   return listing;
}

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

const char* getSellerPort(string sellername){
	size_t index = sellername.find_first_of("1234567890");
	int x = sellername[index] - 48;
	return sellerTCPPorts[x];
}

const char* getBuyerPort(string buyername){
	size_t index = buyername.find_first_of("1234567890");
	int x = buyername[index] - 48;
	return buyerTCPPorts[x];
}

int tcpConnectTo(const char* port){
	int sockfd, rv; 
	char s[INET_ADDRSTRLEN];
	struct addrinfo hints, *servinfo, *p;

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
		printf("Agent could not connect on port %s\n", port);
       	return -1;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
    //printf("Agent: connected on port %s\n", port);
    freeaddrinfo(servinfo); // all done with this structure

    return sockfd;
}

int createTCPSocket(int agentNumber){
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

  	if (agentNumber == 0){
    	port = TCP_PORT_AGENT1;
    } else {
    	port = TCP_PORT_AGENT2;
   	}
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

    printf("<Agent%d> has TCP port %s and IP address %s for Phase 1 part 1\n", agentNumber, port, "127.0.0.1");
    return sockfd;
}

int main(void){
    int new_fd;  
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    char s[INET_ADDRSTRLEN];

    int numbytes;
    char buf[MAXDATASIZE];

    for (int i = 0; i < NUM_AGENTS; i++){
    	if (!fork()) {
    		vector<Listing> listings;
    		vector<BuyerInfo> buyers;
    		int tcpsockfd = createTCPSocket(i);
    		if (tcpsockfd == -1) {
    			printf("Agent%d is not listening\n", i);
    		}

    		int sellersReceived = 0;
    		while (sellersReceived < NUM_SELLERS_PER_AGENT){
    			new_fd = accept(tcpsockfd, (struct sockaddr *)&their_addr, &sin_size);
    			if (new_fd == -1) {
            		perror("accept");
            		continue;
        		}
        		inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
        		//printf("Agent%d: got connection from seller\n", i);

        		if ((numbytes = recv(new_fd, buf, MAXDATASIZE, 0)) != -1) {
        			//printf("Received %s\n", buf);
        		}
        		Listing listing = listingFromMessage(buf);
        		printf("<Agent%d> received house information from <%s>\n", i, listing.sellername.c_str());
        		printf("End of Phase 1 part 1 for <Agent%d>\n", i);
        		listings.push_back(listing);
        		
        		close(new_fd);
        		sellersReceived++;
        	}

    		//printf("Agent%d got listings from all sellers\n", i);
    		//usleep(5000000);
			
			//for(int x=0; x < listings.size(); x++){
    			//Listing listing = listings[x];
    			//printListing(listing);
    			//usleep(10000);
			//}

    		const char* portval;
    		if (i == 1){
    			portval = TCP_PORT_AGENT1;
    		} else {
    			portval = TCP_PORT_AGENT2;
    		}
    		printf("<Agent%d> has TCP port %s and IP address %s for Phase 1 part 2\n", i, portval, "127.0.0.1");

    		//send info to all the buyers
		    int numbytes;
    		int numBuyers = (sizeof(buyerUDPPorts)/sizeof(*buyerUDPPorts));
    		for (int b = 0; b < numBuyers; b++){    			

    			int sockfd, rv, udpbytes;
    			struct addrinfo hints, *servinfo, *p;
    			const char* port;

    			memset(&hints, 0, sizeof hints);
    			hints.ai_family = AF_UNSPEC;
    			hints.ai_socktype = SOCK_DGRAM;
    			port = buyerUDPPorts[b];
    			
				char hostname[1023];
    			hostname[1023] = '\0';
        		gethostname(hostname, 1023);
    			if ((rv = getaddrinfo(hostname, port, &hints, &servinfo)) != 0) {
        			fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        			return 1;
    			}

    			// loop through all the results and make a socket
    			for(p = servinfo; p != NULL; p = p->ai_next) {
        			if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            			perror("talker: socket");
        	    		continue;
        			}
        			break;
    			}

    			if (p == NULL) {
        			fprintf(stderr, "talker: failed to create socket\n");
        			exit(1);
    			}

    			//usleep(5000000);
    			string message = messageFromListings(listings);
    			if ((udpbytes = sendto(sockfd, message.c_str(), strlen(message.c_str()), 0, p->ai_addr, p->ai_addrlen)) == -1) {
        			perror("talker: sendto");
        			exit(1);
    			}

    			printf("<Agent%d> has sent %s to <Buyer%d> on port %s\n", i, message.c_str(), b, port);
    			printf("End of Phase 1 part 2 for <Agent%d>\n", i);
   			 	//close(sockfd);
    		}

    		printf("<Agent%d> has TCP port %s and IP address %s for Phase 1 part 3\n", i, portval, "127.0.0.1");

    		int buyersReceived = 0;
    		while (buyersReceived < numBuyers){
    			new_fd = accept(tcpsockfd, (struct sockaddr *)&their_addr, &sin_size);
    			if (new_fd == -1) {
            		perror("accept");
            		continue;
        		}
        		inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
        		//printf("Agent%d: got connection from buyer\n", i);

        		if ((numbytes = recv(new_fd, buf, MAXDATASIZE, 0)) != -1) {
        			//printf("Agent%d received %s\n", i, buf);
        		}
        		BuyerInfo buyer = messageToBuyer(buf);
        		printf("<Agent%d> receives the offer from <%s>\n", i, buyer.buyername.c_str());
        		buyers.push_back(buyer);
        		buyersReceived++;

        		close(new_fd);
        	}
        	sort(buyers.begin(), buyers.end(), higher_budget());
        	printf("End of Phase 1 part 3 for <Agent%d>\n", i);
        	printf("<Agent%d> has TCP port %s and IP address %s for Phase 1 part 4\n", i, portval, "127.0.0.1");

        	//for each seller's listing, find the richest buyer and respond to parties
        	int finalsock;
        	Listing l;
        	BuyerInfo* b;
        	int x;
        	for (x=0; x<listings.size(); x++){ //loops through sellers (2)
        		l = listings[x];
        		//printf("looking at %s\n", l.sellername.c_str());
        		BuyerInfo richestBuyer;
        		richestBuyer.buyername = "";
        		for (int y=0; y<buyers.size(); y++){ //loop through buyers (5)
        			b = &buyers[y];
        			for (int z=0; z<b->sellers.size(); z++){ //loop through buyer's seller list (2)
        				if (l.sellername == b->sellers[z]) {
        					//if buyer wants to buy from this seller, find if they are the richest yet
        					//printf("setting %s to NAK\n", b->buyername.c_str());
        					b->buyFrom = "NAK"; //we need to know to respond to this buyer. If they win, this value will be replaced with the seller name
        					if (richestBuyer.buyername == ""){
        						//printf("%s is now richest buyer\n", b->buyername.c_str());
        						richestBuyer = *b;
        						b->buyFrom = l.sellername;
        					} else if (b->budget > richestBuyer.budget && b->buyFrom == "NAK"){
        						richestBuyer = *b;
        						//printf("%s is now richest buyer\n", b->buyername.c_str());
        					}
        				}
        			}
        		}
        	
        		if (richestBuyer.buyername == ""){
        			//no buyer wants this seller, send NAK to seller
        			finalsock = tcpConnectTo(getSellerPort(l.sellername));
        			if (send(finalsock, "NAK", MSGSIZE, 0) == -1){
	        			perror("error: client sending");
	        			exit(1);
	    			}
        			printf("The <Agent%d> has send the result to <%s>\n", i, l.sellername.c_str());
        		} else {
        			//1 or more buyers wants to buy, the richest wins
        			//respond to seller with the buyers name
        			//respond to buy with the sellers name
        			richestBuyer.buyFrom = l.sellername;
        			finalsock = tcpConnectTo(getSellerPort(l.sellername));
        			if (send(finalsock, richestBuyer.buyername.c_str(), MSGSIZE, 0) == -1){
	        			perror("error: client sending");
	        			exit(1);
	    			}
	    			printf("The <Agent%d> has send the result to <%s>\n", i, l.sellername.c_str());
        			//printf("Sent %s to %s\n", richestBuyer.buyername.c_str(), l.sellername.c_str());
        			
        			finalsock = tcpConnectTo(getBuyerPort(richestBuyer.buyername));
        			if (send(finalsock, l.sellername.c_str(), MSGSIZE, 0) == -1){
	        			perror("error: client sending");
	        			exit(1);
	    			}
	    			printf("The <Agent%d> has send the result to <%s>\n", i, richestBuyer.buyername.c_str());
        			//printf("Sent %s to %s\n", l.sellername.c_str(), richestBuyer.buyername.c_str());
        		}
        	}

        	//loop through buyers again and respond to each one if their buyFrom is still "NAK"
        	BuyerInfo buyerForResponse;
        	for (x=0; x<buyers.size(); x++){ 
        		buyerForResponse = buyers[x];
        		if (buyerForResponse.buyFrom == "NAK"){
        			finalsock = tcpConnectTo(getBuyerPort(buyerForResponse.buyername));
        			if (send(finalsock, "NAK", MSGSIZE, 0) == -1){
	        			perror("error: client sending");
	        			exit(1);
	    			}
        			//printf("Sent NAK to %s\n", buyerForResponse.buyername.c_str());
        			printf("The <Agent%d> has send the result to <%s>\n", i, buyerForResponse.buyername.c_str());
				}
        	}

        	printf("End of Phase 1 part 4 for <Agent%d>\n", i);
        	close(finalsock);
        	close(tcpsockfd);
        	exit(0);
    	}
    }

    while(1){

    }
    return 0;
}
