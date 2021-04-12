#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include "mqttutil.h"

#include <ctype.h>

int mysplit(char *src, char *first, char *last, char *separator) {
	if (src == NULL || strlen(src) <= 0 || separator == NULL || strlen(separator) <= 0) {
		return -1;
	}
	char *second=(char*)strstr(src, separator); 
	int first_len = strlen(src);
	if (second != NULL) {
		first_len = strlen(src) - strlen(second);
		strncpy(first, src, first_len);
		first[first_len] = '\0';
		strncpy(last, "b", 1);
		last[1] = '\0';
		int last_len = strlen(second) - strlen(separator);
		if (last_len > 0) {
			strcpy(last, second + strlen(separator));
		} else {
			last[0] = '\0';
		}
		return 0;
	}
	return -1;
}

