/**
 *	Program: PDS-2
 *	Author: Adam Bezak xbezak01
 */

#include "pds-dhcpstarve.h"

int main(int argc, char **argv) {
  
  	int ch;
 	bool isInterface, isPool, isGateway, isDns, isDomain, isLeasetime = false;
	string pool, delimeter = "-";

	while ((ch = getopt(argc, argv, "i:p:g:n:d:l:")) != -1) { // kontrola argumentov
		switch (ch) {
			case 'i' :
				input.interface = optarg;
				isInterface = true;
				break;
			case 'p' :
				pool = optarg;
				input.startPool = pool.substr(0, pool.find(delimeter));
				input.endPool = pool.substr(pool.find(delimeter) + 1, pool.length());
				isPool = true;
				break;
			case 'g' :
				input.gateway = optarg;
				isGateway = true;
				break;
			case 'n' :
				input.dns = optarg;
				isDns = true;
				break;
			case 'd':
				input.domain = optarg;
				isDomain = true;
				break;
			case 'l':
				char *t;
				input.leasetime = strtol(optarg, &t, 10);
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

    return 0;
}

void print_error(int err) {
	cerr << errors[err] << endl;		//vypis na stderr
	exit(err);
}