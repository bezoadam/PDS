/**
 *  Program: PDS - DHCP Starvation
 *  @author: Adam Bezak xbezak01
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
#include <cstdint>
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
#define DHCP_OPTIONS_LEN 300

#define DEBUG            1

/**
    Error handling.
*/
enum {
    NO_ERR = 0,      //0
    ERR_BADPARAMS,   //1
    SOCKET_ERR,      //2
    SEND_ERR,        //3
    RECV_ERR,        //4
    OTHER_ERR,       //5
    SIG_INT          //6
};

const char *errors[] = {
    "Ziadna chyba.",
    "Chyba vo vstupnych parametroch",
    "Chyba so socketom.",
    "Chyba pri odosielani packetu",
    "Chyba pri prijmani packetu",
    "Neznama chyba"
};

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
    Handler pri odchyteni ukoncujuceho signalu.

    @param sig Cislo signalu
    @return void
*/
void sigCatch(int sig);

/**
    Ziskanie novej mac adresy a noveho transaction id.

    @param mac[] Odkaz na pole s mac adresou
    @param *xid Odkaz na hodnotu transaction id
    @return vooid
*/
void getMacAddress(uint8_t mac[], uint32_t *xid);

/**
    Naplnenie Discover packetu odpovedajucimi hodnotami.

    @param mac[] Odkaz na pole s mac adresou
    @param xid Odkaz na hodnotu transaction id
    @return void
*/
void makeDiscover(Dhcp *dhcpDiscover, uint8_t mac[], uint32_t *xid);

/**
    Odoslanie Discover packetu a ziskanie Offer packetu.

    @param *dhcpDiscover Odkaz na strukturu Discover packetu
    @param socket Odkaz na socket
    @return dhcp_t Struktura Offer packetu
*/
int sendDiscoverAndReceiveOffer(Dhcp *dhcpDiscover, Dhcp *dhcpOffer, int *socket);

/**
   Naplnenie Request packetu odpovedajucimi hodnotami.

   @param dhcpRequest Odkaz na strukturu Request packetu
   @param mac[] Odkaz na pole s mac adresou 
   @param dhcpServerId[] Odkaz na pole s aktualnou ip adresou DHCP serveru
   @param *xid Odkaz na hodnotu transaction id
   @param *offeredIp Odkaz na ponukanu ip adresu z Offer packetu
   @return void
*/
void makeRequest(Dhcp *dhcpRequest, uint8_t mac[], uint8_t dhcpServerId[], uint32_t *xid, uint32_t *offeredIp);

/**
    Odoslanie Request packetu a ziskanie ACK packetu.

    @param *dhcpRequest Odkaz na strukturu Request packetu
    @param *socket Odkaz na socket
    @return void
*/
int sendRequestAndReceiveAck(Dhcp *dhcpRequest, Dhcp *dhcpAck, int *socket);

/**
    Konfiguracia socketu na odoslielanie a prijmanie Broadcast packetov na dane rozhrannie.

    @param *sock Odkaz na socket
    @param interface Interface zadany zo vstupnych argumentov
    @return void
*/
void configureSocket(int *sock, string interface);

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