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

	dhcp_t *dhcpDiscover = new dhcp;
	int socket;

	configureSocket(&socket, inputStruct->interface);

	*dhcpDiscover = waitForDiscover(&socket);

	if ((close(socket)) == -1)      // close the socket
	err(1,"close() failed");
	printf("Closing socket ...\n");
    return 0;
}

dhcp_t waitForDiscover(int *socket) {
	int n;
	struct sockaddr_in   addrIn, addrOut;
	socklen_t addrlen;

	// memset(&addrIn,0,sizeof(addrIn));
	// addrIn.sin_family=AF_INET;
	// addrIn.sin_addr.s_addr=inet_addr("255.255.255.255");
	// addrIn.sin_port=htons(67);
	
	dhcp_t *dhcpDiscover = new dhcp;
	cout << "tutuddtt \n" << flush;
		/* Citanie dat zo socketu */
	while ((n= recvfrom(*socket, dhcpDiscover, sizeof(dhcp), 0, (struct sockaddr *) &addrIn, &addrlen)) >= 0) {
		if (flag) {
			cout << "data";
			return *dhcpDiscover;
		}
	}

	cout << "received \n" << flush;

	if (dhcpDiscover->bp_options[2] == (uint8_t)DHCP_OPTION_DISCOVER) {
		printf("DHCP DISCOVER received\n");
		return *dhcpDiscover;
	}
}

void configureSocket(int *sock, string interface) {
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
}

void sigCatch(int sig) {
	cout << "\nSIGINT CATCH\n";
	flag = 1;
}

void print_error(int err) {
	cerr << errors[err] << endl;		//vypis na stderr
	exit(err);
}