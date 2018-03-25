/**
 *	Program: PDS-2
 *	Author: Adam Bezak xbezak01
 */

#include "pds-dhcprogue.h"

volatile sig_atomic_t flag = 0;

int main(int argc, char **argv) {
  
  	int ch;
 	bool isInterface, isPool, isGateway, isDns, isDomain, isLeasetime = false;
 	int resultStart, resultEnd;
 	input_t *inputStruct = new input;
 	int socket;
	string pool, delimeter = "-";

	while ((ch = getopt(argc, argv, "i:p:g:n:d:l:")) != -1) { // kontrola argumentov
		switch (ch) {
			case 'i' :
				inputStruct->interface = optarg;
				isInterface = true;
				break;
			case 'p' :
				pool = optarg;
				// inputStruct->startPool = pool.substr(0, pool.find(delimeter));
				// inputStruct->endPool = pool.substr(pool.find(delimeter) + 1, pool.length());
				resultStart = ipStringToNumber((pool.substr(0, pool.find(delimeter))).c_str(), &(inputStruct->startPool));
				resultEnd = ipStringToNumber((pool.substr(pool.find(delimeter) + 1, pool.length())).c_str(), &(inputStruct->endPool));
				isPool = true;
				break;
			case 'g' :
				inputStruct->gateway = optarg;
				isGateway = true;
				break;
			case 'n' :
				inputStruct->dns = optarg;
				isDns = true;
				break;
			case 'd':
				inputStruct->domain = optarg;
				isDomain = true;
				break;
			case 'l':
				char *t;
				inputStruct->leasetime = strtol(optarg, &t, 10);
				isLeasetime = true;
				if (*t != '\0') {
					print_error(ERR_BADPARAMS);
					exit(ERR_BADPARAMS);
				}
				break;
			default :
				print_error(ERR_BADPARAMS);
				exit(ERR_BADPARAMS);
				break;
		}
	}

	if ((isInterface && isPool && isGateway && isDns && isDomain && isLeasetime) == false) {
		print_error(ERR_BADPARAMS);
		exit(ERR_BADPARAMS);
	}

	/* Odchytenie SIG INT signalu */
	struct sigaction sigIntHandler;

	sigIntHandler.sa_handler = sigCatch;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;

	sigaction(SIGINT, &sigIntHandler, NULL);

	uint8_t mac[6];
	uint32_t interfaceBroadcastAddress;
	/* Pociatocny random transaction id */
	// uint32_t xid = htonl(0x01010000);

	getMacAddress(inputStruct->interface, mac, &interfaceBroadcastAddress);
	uint32_t serverIp = configureSocket(&socket, inputStruct->interface);

	uint32_t offeredIp;
	memcpy(&offeredIp, &inputStruct->startPool, 4);
	offeredIp = htonl(offeredIp);
	// ipStringToNumber(inputStruct->startPool.c_str(), &offeredIp);
	Dhcp dhcpDiscover;
	Dhcp dhcpOffer;
	Dhcp dhcpRequest;
	Dhcp dhcpAck;

	while(1) {
		memset(&dhcpDiscover,0, sizeof(dhcpDiscover));
		memset(&dhcpOffer,0, sizeof(dhcpOffer));
		memset(&dhcpRequest,0, sizeof(dhcpRequest));
		memset(&dhcpAck,0, sizeof(dhcpAck));
		if (offeredIp == htonl(inputStruct->endPool)) {
			cout << "\n takto pochop uz je konec \n";
			return 1;
		}
		waitForDiscover(&socket, &dhcpDiscover);
		if (flag) {
			return 1;
		}

		// uint32_t offeredIp;
		// ipStringToNumber("192.168.1.11", &offeredIp);
		
		// int i = 0;
		// for(i = 0; i<300; i++) {
		// 	printf("%02x ", (unsigned char)dhcpDiscover->bp_options[i]);
		// }
		// std::cout << std::setfill('0') << std::setw(8) << std::hex << ip << '\n';
		makeOffer(&dhcpOffer, &dhcpDiscover, mac, interfaceBroadcastAddress, offeredIp, htonl(serverIp), inputStruct);


		sendOfferAndReceiveRequest(&socket, &dhcpOffer, &dhcpRequest);
		if (flag) {

			return 1;
		}		
		

		makeAck(&dhcpAck, &dhcpRequest, mac, interfaceBroadcastAddress, offeredIp, htonl(serverIp), inputStruct);

		sendAck(&socket, &dhcpAck);

		offeredIp = incrementIpAddress(offeredIp);

		// while(1) {
		// }	
	}


	if ((close(socket)) == -1)      // close the socket
	err(1,"close() failed");
	printf("Closing socket ...\n");
    return 0;
}

void sendAck(int *socket, Dhcp *dhcpAck) {
	int n;
	struct sockaddr_in   addrIn, addrOut;
	socklen_t addrlen;

	memset(&addrOut,0,sizeof(addrOut));
	addrOut.sin_family=AF_INET;
	addrOut.sin_addr.s_addr=inet_addr("255.255.255.255");
	addrOut.sin_port=htons(68);

	/* Odoslanie struktury na dany socket */
	if (sendto(*socket, dhcpAck, sizeof(*dhcpAck),0, (struct sockaddr *) &addrOut, sizeof(addrOut)) < 0)
		err(1,"sendto");

	cout << "\nACK send\n";
}

void makeAck(Dhcp *dhcpAck, Dhcp *dhcpRequest, uint8_t mac[], uint32_t interfaceBroadcastAddress, uint32_t offeredIp, uint32_t serverIp, input_t *input) {
	dhcpAck->opcode = 2;
	dhcpAck->htype = 1;
	dhcpAck->hlen = 6;
	dhcpAck->hops = 0;
	memcpy(&dhcpAck->xid, &dhcpRequest->xid, 4);
	dhcpAck->secs = 0;
	dhcpAck->flags = htons(0x8000);
	dhcpAck->ciaddr = 0;
	memcpy(&dhcpAck->yiaddr, &offeredIp, 4);
	dhcpAck->siaddr = serverIp;
	dhcpAck->giaddr = 0;
	memcpy(dhcpAck->chaddr, dhcpRequest->chaddr, DHCP_CHADDR_LEN);
	dhcpAck->magic_cookie = htonl(0x63825363);
	uint8_t option = DHCP_OPTION_ACK;
	fillDhcpOptions(&dhcpAck->bp_options[0], MESSAGE_TYPE_DHCP, &option, sizeof(option));

	fillDhcpOptions(&dhcpAck->bp_options[3], MESSAGE_TYPE_REQ_SUBNET_MASK, (u_int8_t *)&interfaceBroadcastAddress, sizeof(uint32_t));

	uint32_t gateway;
	ipStringToNumber(input->gateway.c_str(), &gateway);
	gateway = htonl(gateway);
	fillDhcpOptions(&dhcpAck->bp_options[9], MESSAGE_TYPE_ROUTER, (u_int8_t *)&gateway, sizeof(uint32_t));

	uint32_t leaseTime = htonl(uint32_t(input->leasetime));
	fillDhcpOptions(&dhcpAck->bp_options[15], MESSAGE_TYPE_LEASE_TIME, (u_int8_t *)&leaseTime, sizeof(uint32_t));

	fillDhcpOptions(&dhcpAck->bp_options[21], MESSAGE_TYPE_DHCP_SERVER, (u_int8_t *)&serverIp, sizeof(uint32_t));

	uint32_t dnsServerIp;
	ipStringToNumber(input->dns.c_str(), &dnsServerIp);
	dnsServerIp = htonl(dnsServerIp);
	fillDhcpOptions(&dhcpAck->bp_options[27], MESSAGE_TYPE_DNS, (u_int8_t *)&dnsServerIp, sizeof(uint32_t));

	option = 0;
   	fillDhcpOptions(&dhcpAck->bp_options[33], MESSAGE_TYPE_END, &option, 0);	
}

uint32_t incrementIpAddress(uint32_t lastUsedIp) {
    // convert the input IP address to an integer
    in_addr_t address = lastUsedIp;

    // add one to the value (making sure to get the correct byte orders)
    address = ntohl(address);
    address += 1;
    address = htonl(address);

    return address;
}

/**
 * Convert human readable IPv4 address to UINT32
 * @param pDottedQuad   Input C string e.g. "192.168.0.1"
 * @param pIpAddr       Output IP address as UINT32
 * return 1 on success, else 0
 */
int ipStringToNumber(const char *pDottedQuad, unsigned int *pIpAddr) {
	unsigned int            byte3;
	unsigned int            byte2;
	unsigned int            byte1;
	unsigned int            byte0;
	char              dummyString[2];

	/* The dummy string with specifier %1s searches for a non-whitespace char
	* after the last number. If it is found, the result of sscanf will be 5
	* instead of 4, indicating an erroneous format of the ip-address.
	*/
	if (sscanf (pDottedQuad, "%u.%u.%u.%u%1s",
	              &byte3, &byte2, &byte1, &byte0, dummyString) == 4)
	{
	  if (    (byte3 < 256)
	       && (byte2 < 256)
	       && (byte1 < 256)
	       && (byte0 < 256)
	     )
	  {
	     *pIpAddr  =   (byte3 << 24)
	                 + (byte2 << 16)
	                 + (byte1 << 8)
	                 +  byte0;

	     return 1;
	  }
	}

	return 0;
}

void getMacAddress(string interface, uint8_t *mac, uint32_t *broadcastAddress) {
  struct ifreq s;
  int fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);

  strcpy(s.ifr_name, "eth1");
  if (0 == ioctl(fd, SIOCGIFHWADDR, &s)) {
  	memcpy((void *)mac, s.ifr_addr.sa_data, 6);
    int i;
    for (i = 0; i < 6; ++i)
      printf(" %02x", (unsigned char) s.ifr_addr.sa_data[i]);
    puts("\n");
  }

  if (ioctl(fd, SIOCGIFNETMASK, &s) == 0) {
  	uint32_t bc;
  	memcpy(broadcastAddress, &((struct sockaddr_in *)&s.ifr_netmask)->sin_addr, sizeof(uint32_t));
  	int i = 0;
    // for (i = 0; i < 4; ++i)
    //   printf(" %sx", (unsigned char) bc[i]);
    puts("\n");
    printf("f %d %d %d %d", INT_TO_ADDR(*broadcastAddress));
    printf("bc %s\n", inet_ntoa(((struct sockaddr_in *)&s.ifr_netmask)->sin_addr));
  }
}

void waitForDiscover(int *socket, Dhcp *dhcpDiscover) {
	int n;
	struct sockaddr_in   addrIn, addrOut;
	socklen_t addrlen;
	

	cout << "\n waiting for discover \n" << flush;
	/* Citanie dat zo socketu */
	cout << "\n zasek \n" << flush;

	while (1) {
		if (recvfrom(*socket, dhcpDiscover, sizeof(*dhcpDiscover), 0, (struct sockaddr *) &addrIn, &addrlen) < 0) {
			cout << n << flush;
			cout << "som tu" << flush;
			flag = 1;
			return;
		}
		printf("f %d %d %d %d", INT_TO_ADDR(dhcpDiscover->yiaddr));
		cout << "received \n" << flush;

		if (dhcpDiscover->bp_options[2] == (uint8_t)DHCP_OPTION_DISCOVER) {
			printf("DHCP DISCOVER received\n");
			return;
		}
	}
	// while (1) {
	// }
}
void makeOffer(Dhcp *dhcpOffer, Dhcp *dhcpDiscover, uint8_t mac[], uint32_t interfaceBroadcastAddress, uint32_t offeredIp, uint32_t serverIp, input_t *input) {
	dhcpOffer->opcode = 2;
	dhcpOffer->htype = 1;
	dhcpOffer->hlen = 6;
	dhcpOffer->hops = 0;
	memcpy(&dhcpOffer->xid, &dhcpDiscover->xid, 4);
	dhcpOffer->secs = 0;
	dhcpOffer->flags = htons(0x8000);
	dhcpOffer->ciaddr = 0;
	// dhcpOffer->yiaddr = offeredIp;
	memcpy(&dhcpOffer->yiaddr, &offeredIp, 4);
	dhcpOffer->siaddr = serverIp;
	dhcpOffer->giaddr = 0;
	memcpy(dhcpOffer->chaddr, dhcpDiscover->chaddr, DHCP_CHADDR_LEN);
	dhcpOffer->magic_cookie = htonl(0x63825363);
	uint8_t option = DHCP_OPTION_OFFER;
	fillDhcpOptions(&dhcpOffer->bp_options[0], MESSAGE_TYPE_DHCP, &option, sizeof(option));

	printf("offermakef %d %d %d %d\n", INT_TO_ADDR(interfaceBroadcastAddress));
	fillDhcpOptions(&dhcpOffer->bp_options[3], MESSAGE_TYPE_REQ_SUBNET_MASK, (u_int8_t *)&interfaceBroadcastAddress, sizeof(uint32_t));

	uint32_t gateway;
	ipStringToNumber(input->gateway.c_str(), &gateway);
	gateway = htonl(gateway);
	fillDhcpOptions(&dhcpOffer->bp_options[9], MESSAGE_TYPE_ROUTER, (u_int8_t *)&gateway, sizeof(uint32_t));

	uint32_t leaseTime = htonl(uint32_t(input->leasetime));
	fillDhcpOptions(&dhcpOffer->bp_options[15], MESSAGE_TYPE_LEASE_TIME, (u_int8_t *)&leaseTime, sizeof(uint32_t));

	fillDhcpOptions(&dhcpOffer->bp_options[21], MESSAGE_TYPE_DHCP_SERVER, (u_int8_t *)&serverIp, sizeof(uint32_t));

	uint32_t dnsServerIp;
	ipStringToNumber(input->dns.c_str(), &dnsServerIp);
	dnsServerIp = htonl(dnsServerIp);
	fillDhcpOptions(&dhcpOffer->bp_options[27], MESSAGE_TYPE_DNS, (u_int8_t *)&dnsServerIp, sizeof(uint32_t));

	option = 0;
   	fillDhcpOptions(&dhcpOffer->bp_options[33], MESSAGE_TYPE_END, &option, 0);
}

void sendOfferAndReceiveRequest(int *socket, Dhcp *dhcpOffer, Dhcp *dhcpRequest) {
	int n;
	struct sockaddr_in   addrIn, addrOut;
	socklen_t addrlen;

	memset(&addrOut,0,sizeof(addrOut));
	addrOut.sin_family=AF_INET;
	addrOut.sin_addr.s_addr=inet_addr("255.255.255.255");
	addrOut.sin_port=htons(68);

	int i;
	for(i = 0; i<300; i++) {
		printf("%02x ", (unsigned char)dhcpOffer->bp_options[i]);
	}
	printf("offermakef %d %d %d %d\n", INT_TO_ADDR(dhcpOffer->yiaddr));

	/* Odoslanie struktury na dany socket */
	if (sendto(*socket, dhcpOffer, sizeof(*dhcpOffer),0, (struct sockaddr *) &addrOut, sizeof(addrOut)) < 0)
		err(1,"sendto");

	cout << "\nwaiting for receive\n";

	while(1) {
		/* Citanie dat zo socketu */
		if (recvfrom(*socket, dhcpRequest, sizeof(*dhcpRequest), 0, (struct sockaddr *) &addrIn, &addrlen) < 0) {
			flag = 1;
			return;
		}

		if (dhcpRequest->bp_options[2] == (uint8_t)DHCP_OPTION_REQUEST) {
			printf("DHCP REQUEST received\n");
	    	printf("%s\n", inet_ntoa(*(struct in_addr *)&dhcpRequest->yiaddr));
			return;
		}	
	}
}

uint32_t configureSocket(int *sock, string interface) {
	struct sockaddr_in   addrIn;   // Datova struktura adries
	struct ifreq ifr;

	int on = 1; // socket option

	/* Vytvorenie socketu */
	if ((*sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
	err(1,"socket failed()");

	/* I want to get an IPv4 IP address */
 	ifr.ifr_addr.sa_family = AF_INET;

 	/* I want IP address attached to "eth0" */
 	strncpy(ifr.ifr_name, interface.c_str(), IFNAMSIZ-1);

 	ioctl(*sock, SIOCGIFADDR, &ifr);

 	/* display result */
	printf("%s\n", inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));

	memset(&addrIn,0,sizeof(addrIn));
	addrIn.sin_family=AF_INET;
	addrIn.sin_addr.s_addr=inet_addr("255.255.255.255");
	addrIn.sin_port=htons(67);

	/* Povolenie reusovania lokalnych adries na porte */
	if ((setsockopt(*sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) == -1)
	err(1,"setsockopt failead()");

	/* Povolenie odosielania broadcastu socketom */
	if ((setsockopt(*sock, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on))) == -1)
	err(1,"setsockopt failebd()");

	/* Povolenie pozuivat viacero portov na jednom sockete */
	if ((setsockopt(*sock, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on))) == -1)
	err(1,"setsockopt failecd()");

	/* Nabinovanie socketu na rozhranie */
	if ((setsockopt(*sock, SOL_SOCKET, SO_BINDTODEVICE, interface.c_str(), interface.length())) == -1)
	err(1,"setsockopt failedd()");

	if ((::bind(*sock, (struct sockaddr *) &addrIn, sizeof(addrIn))) < 0) {
		if ((close(*sock)) == -1)      // close the socket
		err(1,"close() failed");
		err(1,"fail");
	}

	uint32_t dhcpServerIp;
	ipStringToNumber(inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr), &dhcpServerIp);
	return dhcpServerIp;
}

void fillDhcpOptions(uint8_t *packetOptionPart, uint8_t code, uint8_t *data, u_int8_t len) {
    packetOptionPart[0] = code;
    packetOptionPart[1] = len;
    memcpy(&packetOptionPart[2], data, len);
}

void sigCatch(int sig) {
	cout << "\nSIGINT CATCH\n";
	flag = 1;
}

void print_error(int err) {
	cerr << errors[err] << endl;		//vypis na stderr
	exit(err);
}