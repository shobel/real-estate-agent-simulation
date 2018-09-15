all: Seller Agent Buyer 

Seller: Seller.cpp
	g++ -o Seller Seller.cpp -lsocket -lnsl -lresolv

Agent: Agent.cpp
	g++ -o Agent Agent.cpp -lsocket -lnsl -lresolv

Buyer: Buyer.cpp
	g++ -o Buyer Buyer.cpp -lsocket -lnsl -lresolv

