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
#define MESSAGE_TYPE_LEASE_TIME             51
#define MESSAGE_TYPE_DHCP                   53
#define MESSAGE_TYPE_DHCP_SERVER            54
#define MESSAGE_TYPE_PARAMETER_REQ_LIST     55
#define MESSAGE_TYPE_END                    255

#define DHCP_OPTION_DISCOVER               1
#define DHCP_OPTION_OFFER                  2
#define DHCP_OPTION_REQUEST                3
#define DHCP_OPTION_ACK                    5

#define DHCP_CHADDR_LEN  16
#define DHCP_SNAME_LEN   64
#define DHCP_FILE_LEN    128
#define DHCP_OPTIONS_LEN 300

#define INT_TO_ADDR(_addr) \
(_addr & 0xFF), \
(_addr >> 8 & 0xFF), \
(_addr >> 16 & 0xFF), \
(_addr >> 24 & 0xFF)

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
    uint32_t startPool;
    uint32_t endPool;
    string gateway;
    string dns;
    string domain;
    int leasetime;
} input_t;

/**
    Struktura DHCP packetu.
*/
struct Dhcp {
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
};

/**
    Incrementovanie poslednej pouzitej IP adresy

    @param lastUsedIp ip adresa
    @return uint32_t 
*/
uint32_t incrementIpAddress(uint32_t lastUsedIp);

/**
    Ziskanie mac adresy a broadcast adresy z prislusneho rozhrania

    @param interface rozhranie
    @param *mac Odkaz na polem mac
    @param *broadcastAddress Odkaz na broadcast adresu
    @return void
*/
void getMacAddress(string interface, uint8_t *mac, uint32_t *broadcastAddress);

/**
    Pomocna funkcia na konverziu ip adresy v char[] formate na uint32_t format

    @param pDottedQuad ip adresa v char[]
    @param *pIpAddr Odkaz na ip adresu v uint32_t
    @return int
*/
int ipStringToNumber(const char *pDottedQuad, unsigned int *pIpAddr);

/**
    Cakanie na disover packet.

    @param *socket odkaz na socket
    @return dhcp_t struktura discover packetu
*/
void waitForDiscover(int *socket, Dhcp *dhcpDiscover);

/**
    Naplnenie Offer packetu odpovedajucimi hodnotami.

    @param *dhcpOffer Odkaz na strukturu Offer packetu
    @param *dhcpDiscover Odkaz na strukturu Discover packetu
    @param mac[] Mac adresa rozhrania
    @param interfaceBroadcastAddress Broadcast adresa rozhrania
    @param offeredIp Odkaz na ponukanu ip adresu
    @param serverIp Odkaz na gateway adresu 
    @param *input Odkaz na vstupnu strukturu
    @return int
*/
void makeOffer(Dhcp *dhcpOffer, Dhcp *dhcpDiscover, uint8_t mac[], uint32_t interfaceBroadcastAddress, uint32_t offeredIp, uint32_t serverIp, input_t *input);

/**
    Odoslanie Offer packetu na rozhranie a ziskanie Request packetu

    @param *socket Odkaz na socket
    @param *dhcpOffer Odkaz na strukturu offer packetu
    @return dhcp_t
*/
void sendOfferAndReceiveRequest(int *socket, Dhcp *dhcpOffer, Dhcp *dhcpRequest);

/**
    Naplnenie Ack packetu odpovedajucimi hodnotami.

    @param *dhcpAck 
    @param *dhcpRequest
    @param mac[]
    @param interfaceBroadcastAddress
    @param offeredIp
    @param serverIp
    @param *input
    @return int
*/
void makeAck(Dhcp *dhcpAck, Dhcp *dhcpRequest, uint8_t mac[], uint32_t interfaceBroadcastAddress, uint32_t offeredIp, uint32_t serverIp, input_t *input);

/**
    Odoslanie Ack packetu na rozhranie.

    @param *socket
    @param *dhcpAck 
    @return int
*/
void sendAck(int *socket, Dhcp *dhcpAck);

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
uint32_t configureSocket(int *sock, string interface);

/**
    Pomocna funkcia na naplnenie pola Options v DHCP packetoch

    @param *packetOptionPart Cast options packetu do ktorej vkladame nove data
    @param code Kod option
    @param *data Vkladane data
    @param len Dlzka vkladanych dat
    @return void
*/
void fillDhcpOptions(uint8_t *packetOptionPart, uint8_t code, uint8_t *data, uint8_t len);


/*
*   Funkcia na vypis error statusov
*   @param err cislo erroru
*   @return void
*/
void print_error(int err);