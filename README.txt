Full name: Samuel Hobel 
USCID: 8231651764

In this assignment, I have written 3 components that communicate with each other over TCP and UDP sockets.
Each component is responsible for sending and receiving messages, storing information in data structures,
and doing some processing.

Seller.cpp: This file forks 4 sellers that send their house listing info to the two agents. Later they 
receive information about which buyers want to buy their house.
Agent.cpp: This file forks 2 agents that are responsible for receiving the information from the sellers,
relaying that information to the buyers, then receiving the desired houses from the buyers, deciding which
buyers can buy which houses and relaying that information to both the buyers and sellers.
Buyers.cpp: This file forks 5 buyers, who listen over UDP for messages from the agents. Then they go through
the messages and decide which houses they can buy and send that information back to the agents over TCP.
Then finally they listen on their TCP port for whether they were able to buy the house(s) or not.

Running the program: 
Make the code with the command `make all`, then starting the files in the following order seems to work best: ./Agent ./Buyer ./Seller

Messaging formats: There were some minor typos in the format listed that I changed in my output and also added a small amount of additional info in a few of the messages, such as which buyer/seller is outputting the message and on what port they are sending or receiving.

Reused code: code borrowed from Beej's socket programming guide.


