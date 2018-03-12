/**
 *	Program: PDS
 *	Author: Adam Bezak xbezak01
 */

#include "pds-dhcpstarve.h"

volatile sig_atomic_t flag = 0;

int main(int argc, char **argv) {
  
  	int ch;
 	bool isInterface = false;
 	string interface;

	while ((ch = getopt(argc, argv, "i:")) != -1) { // kontrola argumentov
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
	
	// signal(SIGINT, sigCatch); 

	uint8_t mac[6];
	mac[0] = 0x08;
	mac[1] = 0x00;
	mac[2] = 0x27;
	mac[3] = 0x00;
	mac[4] = 0x00;
	mac[5] = 0x00;

	uint32_t xid = htonl(0x01010000);


	int socket;
	configureSocket(&socket, interface);
    dhcp_t *dhcpDiscover = new dhcp;
    dhcp_t *dhcpOffer = new dhcp;
    dhcp_t *dhcpRequest = new dhcp;

	getMacAddress(mac, &xid);

	cout << "mac:";
	 int i;
    for (i = 0; i < 6; ++i)
      printf(" %02x", (unsigned char) mac[i]);
    puts("\n");

    makeDiscover(dhcpDiscover, mac, &xid);

    *dhcpOffer = sendDiscoverAndReceiveOffer(dhcpDiscover, &socket);
    if (flag) {
		delete dhcpDiscover;
		delete dhcpOffer;
		delete dhcpRequest;
		return 0;
	}
  
    uint8_t dhcpServerId[4];
    memcpy(&dhcpServerId, &dhcpOffer->bp_options[5], 4);

    uint32_t offeredIp;
    memcpy(&offeredIp, &dhcpOffer->yiaddr, sizeof(uint32_t));

   	cout << "\n vonku";
    std::cout << std::setfill('0') << std::setw(8) << std::hex << &dhcpOffer->xid << '\n';

    printf("%s", inet_ntoa(*(struct in_addr *)&offeredIp));

    makeRequest(dhcpRequest, mac, dhcpServerId, &xid, &offeredIp);
    sendRequestAndReceiveAck(dhcpRequest, &socket);


    if ((close(socket)) == -1)      // close the socket
	err(1,"close() failed");
	printf("Closing socket ...\n");

    return 0;
}

void configureSocket(int *sock, string interface) {
	struct sockaddr_in   addr, addr2;   // address data structure
	int on = 1;                  // socket option

	memset(&addr,0,sizeof(addr));
	addr.sin_family=AF_INET;
	addr.sin_addr.s_addr=inet_addr("0.0.0.0");  // set the broadcast address
	addr.sin_port=htons(68);       // set the broadcast port

	memset(&addr2,0,sizeof(addr2));
	addr2.sin_family=AF_INET;
	addr2.sin_addr.s_addr=inet_addr("255.255.255.255");  // set the broadcast address
	addr2.sin_port=htons(67);       // set the broadcast port

	if ((*sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
	err(1,"socket failed()");

	if ((setsockopt(*sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) == -1)
	err(1,"setsockopt failead()");

	if ((setsockopt(*sock, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on))) == -1)
	err(1,"setsockopt failebd()");

	if ((setsockopt(*sock, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on))) == -1)
	err(1,"setsockopt failecd()");

	if ((setsockopt(*sock, SOL_SOCKET, SO_BINDTODEVICE, interface.c_str(), interface.length())) == -1)
	err(1,"setsockopt failedd()");

	if ((::bind(*sock, (struct sockaddr *) &addr, sizeof(addr))) < 0) {
		if ((close(*sock)) == -1)      // close the socket
		err(1,"close() failed");
		printf("Closing socket ...\n");
		err(1,"faiffl");
	}
}

// int getMacAddress(string interface, uint8_t *mac) {
//   struct ifreq s;
//   int fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);

//   strcpy(s.ifr_name, "eth0");
//   if (0 == ioctl(fd, SIOCGIFHWADDR, &s)) {
//   	memcpy((void *)mac, s.ifr_addr.sa_data, 6);
//     int i;
//     for (i = 0; i < 6; ++i)
//       printf(" %02x", (unsigned char) s.ifr_addr.sa_data[i]);
//     puts("\n");
//     return 0;
//   }
//   return 1;
// }

void makeDiscover(dhcp_t *dhcpDiscover, uint8_t mac[], uint32_t *xid) {
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
    memcpy(&dhcpDiscover->chaddr, &mac, sizeof(mac));
    dhcpDiscover->magic_cookie = htonl(0x63825363);
   	
   	uint8_t option = DHCP_OPTION_DISCOVER;
   	fill_dhcp_option(&dhcpDiscover->bp_options[0], MESSAGE_TYPE_DHCP, &option, sizeof(option));

   	uint32_t req_ip = htonl(0xc0a8010a);
   	fill_dhcp_option(&dhcpDiscover->bp_options[3], MESSAGE_TYPE_REQ_IP, (u_int8_t *)&req_ip, sizeof(req_ip));

   	uint8_t parameter_req_list[] = {MESSAGE_TYPE_REQ_SUBNET_MASK, MESSAGE_TYPE_ROUTER, MESSAGE_TYPE_DOMAIN_NAME, MESSAGE_TYPE_DNS};
   	fill_dhcp_option(&dhcpDiscover->bp_options[9], MESSAGE_TYPE_PARAMETER_REQ_LIST, (u_int8_t *)&parameter_req_list, sizeof(parameter_req_list));

   	option = 0;
   	fill_dhcp_option(&dhcpDiscover->bp_options[15], MESSAGE_TYPE_END, &option, 0);
}

void fill_dhcp_option(u_int8_t *packet, u_int8_t code, u_int8_t *data, u_int8_t len) {
    packet[0] = code;
    packet[1] = len;
    memcpy(&packet[2], data, len);
}

dhcp_t sendDiscoverAndReceiveOffer(dhcp_t *dhcpDiscover, int *socket) {
	int n;
	struct sockaddr_in   addr, addr2;   // address data structure
	socklen_t addrlen;

	memset(&addr,0,sizeof(addr));
	addr.sin_family=AF_INET;
	addr.sin_addr.s_addr=inet_addr("0.0.0.0");  // set the broadcast address
	addr.sin_port=htons(68);       // set the broadcast port

	memset(&addr2,0,sizeof(addr2));
	addr2.sin_family=AF_INET;
	addr2.sin_addr.s_addr=inet_addr("255.255.255.255");  // set the broadcast address
	addr2.sin_port=htons(67);       // set the broadcast port

	if (sendto(*socket, dhcpDiscover, sizeof(dhcp),0, (struct sockaddr *) &addr2, sizeof(addr2)) < 0)
		err(1,"sendto");

	dhcp_t *dhcpOffer = new dhcp;
	 // read data from the multicast socket and print them on stdout until "END." is received

	while ((n= recvfrom(*socket, dhcpOffer, sizeof(dhcp), 0, (struct sockaddr *) &addr, &addrlen)) >= 0) {
		if (flag) {
			cout << "dhcp som tu" << flush;
			return *dhcpOffer;
		}
	}

	if (dhcpOffer->bp_options[2] == (uint8_t)02) {
		printf("DHCP OFFER received\n");
		cout << "dhcp som tu" << flush;
    	printf("%s", inet_ntoa(*(struct in_addr *)&dhcpOffer->yiaddr));
		return *dhcpOffer;
	}

	// if ((close(*socket)) == -1)      // close the socket
	// err(1,"close() failed");
	// printf("Closing socket ...\n");
}

void makeRequest(dhcp_t *dhcpRequest, uint8_t mac[], uint8_t dhcpServerId[], uint32_t *xid, uint32_t *offeredIp) {
    dhcpRequest->opcode = 1;
    dhcpRequest->htype = 1;
    dhcpRequest->hlen = 6;
    dhcpRequest->hops = 0;
    dhcpRequest->xid = *xid;
    dhcpRequest->secs = 0;
    dhcpRequest->flags = htons(0x8000);
    dhcpRequest->ciaddr = 0;
    dhcpRequest->yiaddr = *offeredIp;
    dhcpRequest->siaddr = 0;
    dhcpRequest->giaddr = 0;
    memcpy(&dhcpRequest->chaddr, &mac, sizeof(mac));
    dhcpRequest->magic_cookie = htonl(0x63825363);
    cout << "\nmake request2\n";
   	cout << "\nmake request\n";
   	printf("%s\n", inet_ntoa(*(struct in_addr *)offeredIp));
   	uint8_t option = DHCP_OPTION_REQUEST;
   	fill_dhcp_option(&dhcpRequest->bp_options[0], MESSAGE_TYPE_DHCP, &option, sizeof(option));
   	fill_dhcp_option(&dhcpRequest->bp_options[3], MESSAGE_TYPE_REQ_IP, (u_int8_t *)offeredIp, sizeof(uint32_t));

   	dhcpRequest->bp_options[9] = 0x36;
   	dhcpRequest->bp_options[10] = 0x4;
   	memcpy(&dhcpRequest->bp_options[11], dhcpServerId, sizeof(uint32_t));
	
	option = 0;
   	fill_dhcp_option(&dhcpRequest->bp_options[19], MESSAGE_TYPE_END, &option, 0);
}

void sendRequestAndReceiveAck(dhcp_t *dhcpRequest, int *socket) {
	int n;
	struct sockaddr_in   addr, addr2;   // address data structure
	socklen_t addrlen;

	memset(&addr,0,sizeof(addr));
	addr.sin_family=AF_INET;
	addr.sin_addr.s_addr=inet_addr("0.0.0.0");  // set the broadcast address
	addr.sin_port=htons(68);       // set the broadcast port

	memset(&addr2,0,sizeof(addr2));
	addr2.sin_family=AF_INET;
	addr2.sin_addr.s_addr=inet_addr("255.255.255.255");  // set the broadcast address
	addr2.sin_port=htons(67);       // set the broadcast port

	if (sendto(*socket, dhcpRequest, sizeof(dhcp),0, (struct sockaddr *) &addr2, sizeof(addr2)) < 0)
		err(1,"sendto");

	dhcp_t *dhcpAck = new dhcp;
	 // read data from the multicast socket and print them on stdout until "END." is received

	while ((n= recvfrom(*socket, dhcpAck, sizeof(dhcp), 0, (struct sockaddr *) &addr, &addrlen)) >= 0) {
		if (flag) {
			cout << "som tu" << flush;
			return;
		}
	}
	if (dhcpAck->bp_options[2] == (uint8_t)05) {
		cout << "som tu" << flush;
		printf("DHCP ACK received\n");
		return;
	}
}

void getMacAddress(uint8_t *mac, uint32_t *xid) {
	if (mac[5] < (uint8_t)0xFF) {
		mac[5] = mac[5] + 1;
	} else if (mac[4] < (uint8_t)0xFF) {
		mac[4] = mac[4] + 1;
	} else if (mac[3] < (uint8_t)0xFF) {
		mac[3] = mac[3] + 1;
	}

	xid = xid + 1;
	// int result = getMacAddress(interface, mac);
}

void sigCatch(int sig) {
	flag = 1;
}

void print_error(int err) {
	cerr << errors[err] << endl;		//vypis na stderr
	exit(err);
}