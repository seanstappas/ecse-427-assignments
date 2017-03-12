/*
	ECSE-427 Assignment 2: Simple Key-Value Store
	Tested using the provided "a2_testcase" tester, with 0 errors.

	Sean Stappas
	March 12, 2017
*/

#ifndef __A2_SEANSTAPPAS__
#define __A2_SEANSTAPPAS__

int kv_store_create(char *name);
int kv_store_write(char *key, char *value);
char *kv_store_read(char *key);
char **kv_store_read_all(char *key);
int kv_delete_db();

#endif
