/**
 *	Program: PDS
 *	Author: Adam Bezak xbezak01
 */

#include "pds-dhcpstarve.h"

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
	
	uint8_t mac[6];
	getMacAddress(mac);

	int socket;
	configureSocket(&socket, interface);

    dhcp_t *dhcpDiscover = new dhcp;

    makeDiscover(dhcpDiscover, mac);

    dhcp_t *dhcpOffer = new dhcp;
    *dhcpOffer = sendDiscoverAndReceiveOffer(dhcpDiscover, &socket);
    cout << "test2" << flush;
    dhcp_t *dhcpRequest = new dhcp;
  
    uint8_t dhcpServerId[5];
    memcpy(&dhcpServerId, &dhcpOffer->bp_options[3], 6);

    makeRequest(dhcpRequest, mac, dhcpServerId);
    sendRequestAndReceiveAck(dhcpRequest, &socket);

    cout << "test" << flush;
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

void makeDiscover(dhcp_t *dhcpDiscover, uint8_t mac[]) {
    dhcpDiscover->opcode = 1;
    dhcpDiscover->htype = 1;
    dhcpDiscover->hlen = 6;
    dhcpDiscover->hops = 0;
    dhcpDiscover->xid = htonl(1000);
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
		if (dhcpOffer->bp_options[2] == (uint8_t)02) {
			printf("DHCP OFFER received\n");
			return *dhcpOffer;
		}
	}

	// if ((close(*socket)) == -1)      // close the socket
	// err(1,"close() failed");
	// printf("Closing socket ...\n");
}

void makeRequest(dhcp_t *dhcpRequest, uint8_t mac[], uint8_t dhcpServerId[]) {
    dhcpRequest->opcode = 1;
    dhcpRequest->htype = 1;
    dhcpRequest->hlen = 6;
    dhcpRequest->hops = 0;
    dhcpRequest->xid = htonl(1000);
    dhcpRequest->secs = 0;
    dhcpRequest->flags = htons(0x8000);
    dhcpRequest->ciaddr = 0;
    dhcpRequest->yiaddr = 0;
    dhcpRequest->siaddr = 0;
    dhcpRequest->giaddr = 0;
    memcpy(&dhcpRequest->chaddr, &mac, sizeof(mac));
    dhcpRequest->magic_cookie = htonl(0x63825363);
   	
   	uint8_t option = DHCP_OPTION_REQUEST;
   	fill_dhcp_option(&dhcpRequest->bp_options[0], MESSAGE_TYPE_DHCP, &option, sizeof(option));

   	uint32_t req_ip = htonl(0xc0a8010a);
   	fill_dhcp_option(&dhcpRequest->bp_options[3], MESSAGE_TYPE_REQ_IP, (u_int8_t *)&req_ip, sizeof(req_ip));

   	memcpy(&dhcpRequest->bp_options[9], &dhcpServerId, sizeof(dhcpServerId));

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
		if (dhcpAck->bp_options[2] == (uint8_t)05) {
			printf("DHCP ACK received\n");
			break;
		}
	}
}

void getMacAddress(uint8_t *mac) {
	mac[0] = 0x00;
	mac[1] = 0x1A;
	mac[2] = 0x80;
	mac[3] = 0x80;
	mac[4] = 0x2C;
	mac[5] = 0xC3;
	// int result = getMacAddress(interface, mac);
	cout << "mac:";
	 int i;
    for (i = 0; i < 6; ++i)
      printf(" %02x", (unsigned char) mac[i]);
    puts("\n");
}

void print_error(int err) {
	cerr << errors[err] << endl;		//vypis na stderr
	exit(err);
}