
#ifndef RDASUTILS
#define RDASUTILS

char* rdas_byte2string(uint8_t* in,int len)
{
	char str[len+1];
	memcpy(str, in, len);
	str[len] = 0; // Null termination.
	return str;
};

#endif


