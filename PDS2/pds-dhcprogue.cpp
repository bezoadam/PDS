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
	string pool, delimeter = "-";

	while ((ch = getopt(argc, argv, "i:p:g:n:d:l:")) != -1) { // kontrola argumentov
		switch (ch) {
			case 'i' :
				inputStruct->interface = optarg;
				isInterface = true;
				break;
			case 'p' :
				pool = optarg;
				inputStruct->startPool = pool.substr(0, pool.find(delimeter));
				inputStruct->endPool = pool.substr(pool.find(delimeter) + 1, pool.length());
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
	/* Pociatocny random transaction id */
	// uint32_t xid = htonl(0x01010000);

	// getMacAddress(mac);

	dhcp_t *dhcpDiscover = new dhcp;
	int socket;

	uint32_t serverIp = configureSocket(&socket, inputStruct->interface);

	cout << "\noffer 1";
	*dhcpDiscover = waitForDiscover(&socket);
	cout << "\noffer 2";
	dhcp_t *dhcpOffer = new dhcp;

	uint32_t offeredIp;
	ipStringToNumber("192.168.1.11", &offeredIp);
	
	// int i = 0;
	// for(i = 0; i<300; i++) {
	// 	printf("%02x ", (unsigned char)dhcpDiscover->bp_options[i]);
	// }
	// std::cout << std::setfill('0') << std::setw(8) << std::hex << ip << '\n';
	makeOffer(dhcpOffer, dhcpDiscover, mac, htonl(offeredIp), htonl(serverIp), inputStruct);

	dhcp_t *dhcpRequest = new dhcp;

	*dhcpRequest = sendOfferAndReceiveRequest(&socket, dhcpOffer, serverIp);
	

	if ((close(socket)) == -1)      // close the socket
	err(1,"close() failed");
	printf("Closing socket ...\n");
    return 0;
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

int getMacAddress(string interface, uint8_t *mac) {
  struct ifreq s;
  int fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);

  strcpy(s.ifr_name, "eth1");
  if (0 == ioctl(fd, SIOCGIFHWADDR, &s)) {
  	memcpy((void *)mac, s.ifr_addr.sa_data, 6);
    int i;
    for (i = 0; i < 6; ++i)
      printf(" %02x", (unsigned char) s.ifr_addr.sa_data[i]);
    puts("\n");
    return 0;
  }
  return 1;
}

dhcp_t waitForDiscover(int *socket) {
	int n;
	struct sockaddr_in   addrIn, addrOut;
	socklen_t addrlen;
	
	dhcp_t *dhcpDiscover = new dhcp;
		/* Citanie dat zo socketu */
	while ((n= recvfrom(*socket, dhcpDiscover, sizeof(dhcp), 0, (struct sockaddr *) &addrIn, &addrlen)) >= 0) {
		if (flag) {
			cout << "data";
			return *dhcpDiscover;
		}
	}

	cout << "received \n";

	if (dhcpDiscover->bp_options[2] == (uint8_t)DHCP_OPTION_DISCOVER) {
		printf("DHCP DISCOVER received\n");
		return *dhcpDiscover;
	}
}
void makeOffer(dhcp_t *dhcpOffer, dhcp_t *dhcpDiscover, uint8_t mac[], uint32_t offeredIp, uint32_t serverIp, input_t *input) {
	dhcpOffer->opcode = 2;
	dhcpOffer->htype = 1;
	dhcpOffer->hlen = 6;
	dhcpOffer->hops = 0;
	memcpy(&dhcpOffer->xid, &dhcpDiscover->xid, 4);
	dhcpOffer->secs = 0;
	dhcpOffer->flags = htons(0x8000);
	dhcpOffer->ciaddr = 0;
	dhcpOffer->yiaddr = offeredIp;
	dhcpOffer->siaddr = serverIp;
	dhcpOffer->giaddr = 0;
	memcpy(dhcpOffer->chaddr, dhcpDiscover->chaddr, DHCP_CHADDR_LEN);
	dhcpOffer->magic_cookie = htonl(0x63825363);
	uint8_t option = DHCP_OPTION_OFFER;
	fillDhcpOptions(&dhcpOffer->bp_options[0], MESSAGE_TYPE_DHCP, &option, sizeof(option));

	uint32_t subnetMask;
	ipStringToNumber("255.255.255.0", &subnetMask);
	subnetMask = htonl(subnetMask);
	fillDhcpOptions(&dhcpOffer->bp_options[3], MESSAGE_TYPE_REQ_SUBNET_MASK, (u_int8_t *)&subnetMask, sizeof(uint32_t));

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

dhcp_t sendOfferAndReceiveRequest(int *socket, dhcp_t *dhcpOffer, uint32_t serverIp) {
	int n;
	struct sockaddr_in   addrIn, addrOut;
	socklen_t addrlen;

	memset(&addrOut,0,sizeof(addrOut));
	addrOut.sin_family=AF_INET;
	addrOut.sin_addr.s_addr=inet_addr("255.255.255.255");
	addrOut.sin_port=htons(68);

	/* Odoslanie struktury na dany socket */
	if (sendto(*socket, dhcpOffer, sizeof(dhcp),0, (struct sockaddr *) &addrOut, sizeof(addrOut)) < 0)
		err(1,"sendto");

	dhcp_t *dhcpRequest = new dhcp;

	cout << "\nwaiting for receive\n";
	/* Citanie dat zo socketu */
	while ((n= recvfrom(*socket, dhcpRequest, sizeof(dhcp), 0, (struct sockaddr *) &addrIn, &addrlen)) >= 0) {
		if (flag) {
			return *dhcpRequest;
		}
	}

	if (dhcpRequest->bp_options[2] == (uint8_t)DHCP_OPTION_REQUEST) {
		printf("DHCP REQUEST received\n");
    	printf("%s\n", inet_ntoa(*(struct in_addr *)&dhcpRequest->yiaddr));
		return *dhcpOffer;
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