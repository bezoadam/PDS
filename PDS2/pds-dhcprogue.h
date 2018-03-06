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

using namespace std;

enum {
	NO_ERR = 0,		//0
	ERR_BADPARAMS,	//1
};

const char *errors[] = {
	"Ziadna chyba.",
	"Chyba vo vstupnych parametroch"
};

struct input_t {
	string interface;
	string startPool;
	string endPool;
	string gateway;
	string dns;
	string domain;
	int leasetime;
} input;

/*
*	Funkcia na vypis error statusov
*	@param err cislo erroru
*	@return void
*/
void print_error(int err);