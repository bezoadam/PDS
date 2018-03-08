/**
 *	Program: PDS
 *	Author: Adam Bezak xbezak01
 */
#include <iostream>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <net/if.h>
#include <sys/socket.h>   
#include <arpa/inet.h>    
#include <netinet/in.h>   
#include <netdb.h>        
#include <err.h>

#include <sys/ioctl.h>
using namespace std;

#define DHCP_CHADDR_LEN 16
#define DHCP_SNAME_LEN  64
#define DHCP_FILE_LEN   128

enum {
	NO_ERR = 0,		//0
	ERR_BADPARAMS,	//1
};

const char *errors[] = {
	"Ziadna chyba.",
	"Chyba vo vstupnych parametroch"
};


typedef u_int32_t ip4_t;

typedef struct dhcp {
    uint8_t    opcode;
    uint8_t    htype;
    uint8_t    hlen;
    uint8_t    hops;
    uint32_t   xid;
    uint16_t   secs;
    uint16_t   flags;
    ip4_t       ciaddr;
    ip4_t       yiaddr;
    ip4_t       siaddr;
    ip4_t       giaddr;
    uint8_t    chaddr[DHCP_CHADDR_LEN];
    char        bp_sname[DHCP_SNAME_LEN];
    char        bp_file[DHCP_FILE_LEN];
    uint32_t    magic_cookie;
    u_int8_t    bp_options[300];
} dhcp_t;

int getMacAddress(string interface, uint8_t *mac);

void sendDiscover();

/*
*	Funkcia na vypis error statusov
*	@param err cislo erroru
*	@return void
*/
void print_error(int err);