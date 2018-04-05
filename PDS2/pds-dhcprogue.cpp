/**
 *	Program: PDS-2
 *	Author: Adam Bezak xbezak01
 */

#include "pds-dhcprogue.h"

volatile sig_atomic_t flag = 0;

int main(int argc, char **argv) {
  
  	int ch;
 	bool isInterface, isPool, isGateway, isDns, isDomain, isLeasetime = false;
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
				ipStringToNumber((pool.substr(0, pool.find(delimeter))).c_str(), &(inputStruct->startPool));
				ipStringToNumber((pool.substr(pool.find(delimeter) + 1, pool.length())).c_str(), &(inputStruct->endPool));
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
					return ERR_BADPARAMS;
				}
				break;
			default :
				print_error(ERR_BADPARAMS);
				return ERR_BADPARAMS;
				break;
		}
	}

	if ((isInterface && isPool && isGateway && isDns && isDomain && isLeasetime) == false) {
		print_error(ERR_BADPARAMS);
		return ERR_BADPARAMS;
	}

	/* Odchytenie SIG INT signalu */
	struct sigaction sigIntHandler;

	sigIntHandler.sa_handler = sigCatch;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;

	sigaction(SIGINT, &sigIntHandler, NULL);

	uint8_t mac[6];
	uint32_t interfaceBroadcastAddress;

	getMacAddress(inputStruct->interface, mac, &interfaceBroadcastAddress);
	uint32_t serverIp = configureSocket(&socket, inputStruct->interface);

	uint32_t offeredIp;
	memcpy(&offeredIp, &inputStruct->startPool, 4);
	offeredIp = htonl(offeredIp);
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
			if ((close(socket)) == -1) {
				print_error(SOCKET_ERR);
				return SOCKET_ERR;
			}
			cout << "Vycerpany adresny pool adries. \n";
			return NO_ERR;
		}

		int waitForDiscoverResult = waitForDiscover(&socket, &dhcpDiscover);

		if (waitForDiscoverResult == SIG_INT) {
			return NO_ERR;
		} else if (waitForDiscoverResult != NO_ERR) {
			print_error(waitForDiscoverResult);
			if ((close(socket)) == -1) {
				print_error(SOCKET_ERR);
				return SOCKET_ERR;
			}
			return waitForDiscoverResult;
		}

		makeOffer(&dhcpOffer, &dhcpDiscover, mac, interfaceBroadcastAddress, offeredIp, htonl(serverIp), inputStruct);

		int sendOfferAndReceiveRequestResult = sendOfferAndReceiveRequest(&socket, &dhcpOffer, &dhcpRequest);
		if (waitForDiscoverResult == SIG_INT) {
			return NO_ERR;
		} else if (sendOfferAndReceiveRequestResult != NO_ERR) {
			print_error(sendOfferAndReceiveRequestResult);
			if ((close(socket)) == -1) {
				print_error(SOCKET_ERR);
				return (SOCKET_ERR);				
			}
			return sendOfferAndReceiveRequestResult;
		}

		makeAck(&dhcpAck, &dhcpRequest, mac, interfaceBroadcastAddress, offeredIp, htonl(serverIp), inputStruct);

		int sendAckResult = sendAck(&socket, &dhcpAck);
		if (sendAckResult != NO_ERR) {
			print_error(sendAckResult);
			if ((close(socket)) == -1) {
				print_error(SOCKET_ERR);
				return (SOCKET_ERR);				
			}
			return sendAckResult;
		}

		offeredIp = incrementIpAddress(offeredIp);
	}

	if ((close(socket)) == -1) {
		print_error(SOCKET_ERR);
		return (SOCKET_ERR);
	}

    return NO_ERR;
}

int sendAck(int *socket, Dhcp *dhcpAck) {
	int n;
	struct sockaddr_in   addrIn, addrOut;
	socklen_t addrlen;

	memset(&addrOut,0,sizeof(addrOut));
	addrOut.sin_family=AF_INET;
	addrOut.sin_addr.s_addr=inet_addr("255.255.255.255");
	addrOut.sin_port=htons(68);

	/* Odoslanie struktury na dany socket */
	if (sendto(*socket, dhcpAck, sizeof(*dhcpAck),0, (struct sockaddr *) &addrOut, sizeof(addrOut)) < 0)
		return SEND_ERR;

#ifdef DEBUG
	cout << "\nACK send\n";
#endif
	return NO_ERR;
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
    // konverzia vstupu na ip adresu
    in_addr_t address = lastUsedIp;

    // pricitanie adresy
    address = ntohl(address);
    address += 1;
    address = htonl(address);

    return address;
}

void ipStringToNumber(const char *pDottedQuad, unsigned int *pIpAddr) {
	unsigned int byte3, byte2, byte1, byte0;
	char dummyString[2];

	if (sscanf (pDottedQuad, "%u.%u.%u.%u%1s", &byte3, &byte2, &byte1, &byte0, dummyString) == 4) {
		if ((byte3 < 256) && (byte2 < 256) && (byte1 < 256) && (byte0 < 256)) {
			*pIpAddr = (byte3 << 24) + (byte2 << 16) + (byte1 << 8) +  byte0;
		}
	}
}

void getMacAddress(string interface, uint8_t *mac, uint32_t *broadcastAddress) {
  struct ifreq s;
  int fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);

  strcpy(s.ifr_name, interface.c_str());
  if (0 == ioctl(fd, SIOCGIFHWADDR, &s)) {
  	memcpy((void *)mac, s.ifr_addr.sa_data, 6);
  }

  if (ioctl(fd, SIOCGIFNETMASK, &s) == 0) {
  	uint32_t bc;
  	memcpy(broadcastAddress, &((struct sockaddr_in *)&s.ifr_netmask)->sin_addr, sizeof(uint32_t));
  }
}

int waitForDiscover(int *socket, Dhcp *dhcpDiscover) {
	int n;
	struct sockaddr_in   addrIn, addrOut;
	socklen_t addrlen;
	addrlen = sizeof(addrIn);

#ifdef DEBUG
	cout << "\n waiting for discover \n" << flush;
#endif
	/* Citanie dat zo socketu */

	while (1) {
		if (recvfrom(*socket, dhcpDiscover, sizeof(*dhcpDiscover), 0, (struct sockaddr *) &addrIn, &addrlen) < 0) {
			if (flag == 1)
				return SIG_INT;
			return RECV_ERR;
		}

		if (dhcpDiscover->bp_options[2] == (uint8_t)DHCP_OPTION_DISCOVER) {
#ifdef DEBUG
			printf("DHCP DISCOVER received\n");
#endif
			return NO_ERR;
		}
	}

	return OTHER_ERR;
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
	memcpy(&dhcpOffer->yiaddr, &offeredIp, 4);
	dhcpOffer->siaddr = serverIp;
	dhcpOffer->giaddr = 0;
	memcpy(dhcpOffer->chaddr, dhcpDiscover->chaddr, DHCP_CHADDR_LEN);
	dhcpOffer->magic_cookie = htonl(0x63825363);
	uint8_t option = DHCP_OPTION_OFFER;
	fillDhcpOptions(&dhcpOffer->bp_options[0], MESSAGE_TYPE_DHCP, &option, sizeof(option));

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

int sendOfferAndReceiveRequest(int *socket, Dhcp *dhcpOffer, Dhcp *dhcpRequest) {
	int n;
	struct sockaddr_in   addrIn, addrOut;
	socklen_t addrlen;
	addrlen = sizeof(addrIn);

	memset(&addrOut,0,sizeof(addrOut));
	addrOut.sin_family=AF_INET;
	addrOut.sin_addr.s_addr=inet_addr("255.255.255.255");
	addrOut.sin_port=htons(68);

	/* Odoslanie struktury na dany socket */
	if (sendto(*socket, dhcpOffer, sizeof(*dhcpOffer),0, (struct sockaddr *) &addrOut, sizeof(addrOut)) < 0)
		return SEND_ERR;

	while(1) {
		/* Citanie dat zo socketu */
		if (recvfrom(*socket, dhcpRequest, sizeof(*dhcpRequest), 0, (struct sockaddr *) &addrIn, &addrlen) < 0) {
			if (flag == 1)
				return SIG_INT;
			return RECV_ERR;
		}

		if (dhcpRequest->bp_options[2] == (uint8_t)DHCP_OPTION_REQUEST) {

#ifdef DEBUG
			printf("DHCP REQUEST received\n");
	    	printf("%s\n", inet_ntoa(*(struct in_addr *)&dhcpRequest->yiaddr));
#endif

			return NO_ERR;
		}	
	}

	return OTHER_ERR;
}

uint32_t configureSocket(int *sock, string interface) {
	struct sockaddr_in   addrIn;   // Datova struktura adries
	struct ifreq ifr;

	int on = 1; // socket option

	/* Vytvorenie socketu */
	if ((*sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
	err(1,"socket creating failed");

	/* I want to get an IPv4 IP address */
 	ifr.ifr_addr.sa_family = AF_INET;

 	/* I want IP address attached to "eth0" */
 	strncpy(ifr.ifr_name, interface.c_str(), IFNAMSIZ-1);

 	ioctl(*sock, SIOCGIFADDR, &ifr);

#ifdef DEBUG
 	/* display result */
	printf("%s\n", inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
#endif

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

	struct timeval tv;
	tv.tv_sec = 3600;
	tv.tv_usec = 0;
	if (setsockopt(*sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
		std::cerr << "ERROR setsockopt rcvtime"<<std::endl;
	}

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
	flag = 1;
}

void print_error(int err) {
	cerr << errors[err] << endl;		//vypis na stderr
}