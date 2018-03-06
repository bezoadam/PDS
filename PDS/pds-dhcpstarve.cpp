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
	int result = getMacAddress(interface, mac);
	cout << "mac:";
	 int i;
    for (i = 0; i < 6; ++i)
      printf(" %02x", (unsigned char) mac[i]);
    puts("\n");

    return 0;
}

int getMacAddress(string interface, uint8_t *mac) {
  struct ifreq s;
  int fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);

  strcpy(s.ifr_name, "eth0");
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

void sendDiscover() {
	int  fd;                     // UDP socket
	struct sockaddr_in   addr;   // address data structure
	char  buffer[1024]; 
	int on;                      // socket option
	int n;

	// if (argc != 3)                          // three parameters required
	// err(1,"Usage: %s <broacast IP address> <port>",argv[0]);

	if ((fd=socket(AF_INET,SOCK_DGRAM,0)) < 0) // create a UDP socket for broadcast
	err(1,"Socket() failed");

	memset(&addr,0,sizeof(addr));
	addr.sin_family=AF_INET;
	addr.sin_addr.s_addr=inet_addr("127.255.255.255");  // set the broadcast address
	addr.sin_port=htons(atoi("67"));       // set the broadcast port

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
	err(1,"socket failed()");

	on = 1;                                   // set socket to send broadcast messages
	if ((setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on))) == -1)
	err(1,"setsockopt failed()");

	while((n=read(STDIN_FILENO,buffer,1024)) > 0){                           // read the command line 
	if (sendto(fd,buffer,n-1,0,(struct sockaddr *) &addr, sizeof(addr)) < 0) // send data without EOL
	  err(1,"sendto");
	printf("Sending \"%.*s\" to %s, port %d\n",n-1,buffer,"127.255.255.255",atoi("67"));
	} // until EOF (CTRL-D)


	if ((close(fd)) == -1)      // close the socket
	err(1,"close() failed");
	printf("Closing socket ...\n");

	exit(0);
}

void print_error(int err) {
	cerr << errors[err] << endl;		//vypis na stderr
	exit(err);
}