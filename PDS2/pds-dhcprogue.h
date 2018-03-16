/**
 *  Program: PDS
 *  Author: Adam Bezak xbezak01
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
#include <signal.h>
#include <inttypes.h>
#include <iostream>
#include <iomanip>
#include <sys/ioctl.h>

using namespace std;

#define MESSAGE_TYPE_REQ_SUBNET_MASK        1
#define MESSAGE_TYPE_ROUTER                 3
#define MESSAGE_TYPE_DNS                    6
#define MESSAGE_TYPE_DOMAIN_NAME            15
#define MESSAGE_TYPE_REQ_IP                 50
#define MESSAGE_TYPE_DHCP                   53
#define MESSAGE_TYPE_PARAMETER_REQ_LIST     55
#define MESSAGE_TYPE_END                    255

#define DHCP_OPTION_DISCOVER               1
#define DHCP_OPTION_OFFER                  2
#define DHCP_OPTION_REQUEST                3
#define DHCP_OPTION_ACK                    5

#define DHCP_CHADDR_LEN  16
#define DHCP_SNAME_LEN   64
#define DHCP_FILE_LEN    128
#define DHCP_OPTIONS_LEN 600

enum {
    NO_ERR = 0,     //0
    ERR_BADPARAMS,  //1
};

const char *errors[] = {
    "Ziadna chyba.",
    "Chyba vo vstupnych parametroch"
};

typedef struct input {
    string interface;
    string startPool;
    string endPool;
    string gateway;
    string dns;
    string domain;
    int leasetime;
} input_t;

/**
    Struktura DHCP packetu.
*/
typedef struct dhcp {
    uint8_t    opcode;
    uint8_t    htype;
    uint8_t    hlen;
    uint8_t    hops;
    uint32_t   xid;
    uint16_t   secs;
    uint16_t   flags;
    uint32_t   ciaddr;
    uint32_t   yiaddr;
    uint32_t   siaddr;
    uint32_t   giaddr;
    uint8_t    chaddr[DHCP_CHADDR_LEN];
    uint8_t    bp_sname[DHCP_SNAME_LEN];
    uint8_t    bp_file[DHCP_FILE_LEN];
    uint32_t   magic_cookie;
    uint8_t    bp_options[DHCP_OPTIONS_LEN];
} dhcp_t;

dhcp_t waitForDiscover(int *socket);

/**
    Handler pri odchyteni ukoncujuceho signalu.

    @param sig Cislo signalu
    @return void
*/
void sigCatch(int sig);

/**
    Konfiguracia socketu na odoslielanie a prijmanie Broadcast packetov na dane rozhrannie.

    @param *sock Odkaz na socket
    @param interface Interface zadany zo vstupnych argumentov
    @return void
*/
void configureSocket(int *sock, string interface);

/*
*   Funkcia na vypis error statusov
*   @param err cislo erroru
*   @return void
*/
void print_error(int err);