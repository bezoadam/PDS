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


    dhcp_t *dhcpDiscover = new dhcp;

    cout << "test" << flush;

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
    // dhcpDiscover->bp_sname = 0;
    // memset(&dhcpDiscover->bp_sname, 0, sizeof(uint8_t) * 64);
	// cout << "tedst" << flush;
    // dhcpDiscover->bp_file = 0;
    // memset(dhcpDiscover->bp_file, 0, sizeof(uint8_t) * 128);
    dhcpDiscover->magic_cookie = htonl(0x63825363);
   	
   	uint8_t option = DHCP_OPTION_DISCOVER;
   	fill_dhcp_option(&dhcpDiscover->bp_options[0], MESSAGE_TYPE_DHCP, &option, sizeof(option));

   	uint32_t req_ip = htonl(0xc0a8010a);
   	fill_dhcp_option(&dhcpDiscover->bp_options[3], MESSAGE_TYPE_REQ_IP, (u_int8_t *)&req_ip, sizeof(req_ip));

   	uint8_t parameter_req_list[] = {MESSAGE_TYPE_REQ_SUBNET_MASK, MESSAGE_TYPE_ROUTER, MESSAGE_TYPE_DOMAIN_NAME, MESSAGE_TYPE_DNS};
   	fill_dhcp_option(&dhcpDiscover->bp_options[9], MESSAGE_TYPE_PARAMETER_REQ_LIST, (u_int8_t *)&parameter_req_list, sizeof(parameter_req_list));

   	option = 0;
   	fill_dhcp_option(&dhcpDiscover->bp_options[15], MESSAGE_TYPE_END, &option, 0);

    sendDiscover(dhcpDiscover);

    return 0;
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

void fill_dhcp_option(u_int8_t *packet, u_int8_t code, u_int8_t *data, u_int8_t len) {
    packet[0] = code;
    packet[1] = len;
    memcpy(&packet[2], data, len);
}

void sendDiscover(dhcp_t *dhcpDiscover) {
	int  fd;                     // UDP socket
	struct sockaddr_in   addr, addr2;   // address data structure
	char  buffer[1024]; 
	int on;                      // socket option
	int n;

	// if (argc != 3)                          // three parameters required
	// err(1,"Usage: %s <broacast IP address> <port>",argv[0]);

	memset(&addr,0,sizeof(addr));
	addr.sin_family=AF_INET;
	addr.sin_addr.s_addr=inet_addr("192.168.1.1");  // set the broadcast address
	addr.sin_port=htons(68);       // set the broadcast port

	memset(&addr2,0,sizeof(addr2));
	addr2.sin_family=AF_INET;
	addr2.sin_addr.s_addr=inet_addr("255.255.255.255");  // set the broadcast address
	addr2.sin_port=htons(67);       // set the broadcast port

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
	err(1,"socket failed()");

	on = 1;                                   // set socket to send broadcast messages
	if ((setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) == -1)
	err(1,"setsockopt failed()");

	if ((setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on))) == -1)
	err(1,"setsockopt failed()");

	if ((setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on))) == -1)
	err(1,"setsockopt failed()");

	if ((::bind(fd, (struct sockaddr *) &addr, sizeof(addr))) < 0) {
		if ((close(fd)) == -1)      // close the socket
		err(1,"close() failed");
		printf("Closing socket ...\n");
		err(1,"faiffl");
	}
	int i = 0;
	for (i = 0; i < 300; ++i)
      printf(" %02x", (unsigned char) dhcpDiscover->bp_options[i]);
    puts("\n");
	
	cout << sizeof(&dhcpDiscover->chaddr);
	cout << sizeof(&dhcpDiscover->bp_options);
	cout << sizeof(dhcpDiscover);
	if (sendto(fd, dhcpDiscover, sizeof(dhcp),0, (struct sockaddr *) &addr2, sizeof(addr2)) < 0)
		err(1,"sendto");
	// while((n=read(STDIN_FILENO,buffer,1024)) > 0){                           // read the command line 
	// if (sendto(fd,buffer,n-1,0,(struct sockaddr *) &addr, sizeof(addr)) < 0) // send data without EOL
	//   err(1,"sendto");
	// printf("Sending \"%.*s\" to %s, port %d\n",n-1,buffer,"127.255.255.255",atoi("67"));
	// } // until EOF (CTRL-D)


	if ((close(fd)) == -1)      // close the socket
	err(1,"close() failed");
	printf("Closing socket ...\n");

	exit(0);
}

void print_error(int err) {
	cerr << errors[err] << endl;		//vypis na stderr
	exit(err);
}