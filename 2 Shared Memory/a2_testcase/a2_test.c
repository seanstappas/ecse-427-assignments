#include "a2_lib.h"

/*
* Handler for the SIGINT signal (Ctrl-C).
*/

static void handlerSIGINT(int sig) {
	if (sig == SIGINT) {
		printf("SEANSTAPPAS: Ctrl-C pressed!\n");
		kv_delete_db();
	}
}

void infinite_write() {
	while (1) {
		kv_store_write("key1", "value1");
		printf("Write key1 value1\n");
		sleep(1);
	}
}

void infinite_read() {
	while (1) {
		char *value = kv_store_read("key1");
		printf("Read key1: %s\n", value);
		free(value);
		sleep(1);
	}
}


void test_all() {
	char *value;

	value = kv_store_read("key1");
	if (value != NULL) {
		printf("Read key1: %s\n", value);
		free(value);
	}

	char **vals = kv_store_read_all("key1");
	if (vals != NULL) {
		for (int i = 0; vals[i] != NULL; i++) {
			printf("Read %d: %s\n", i, vals[i]);
			free(vals[i]);
		}
		free(vals);
	}

	printf("Read all\n");

	kv_store_write("fN7Y81yc006aAW5Uljl73NcCAjwh25", "Ft2uK7jZ9fd9ogKy88gx4HoxqFE54463UY6TnjM57zdmLZTSpt27pGNHI4lyYXFk84R3yMyNBVPL4k2w73r3PeIw9z7Fxhn0722OAfNXu7L7f64427B9QPRUMWs5r0y5PIs784wmnLbF73XN5DP63Vc7uZ0p1B4P870WnpD2859Y777LH0572fn1xag1bVsq0F1zVlUn0Hp50J7rTtrZzYXOdO47sBXWTDMApIN5XLlAuuY5kfRlHwVn84Ynk52");
	printf("Write fN7Y81yc006aAW5Uljl73NcCAjwh25 -> Ft2uK7jZ9fd9ogKy88gx4HoxqFE54463UY6TnjM57zdmLZTSpt27pGNHI4lyYXFk84R3yMyNBVPL4k2w73r3PeIw9z7Fxhn0722OAfNXu7L7f64427B9QPRUMWs5r0y5PIs784wmnLbF73XN5DP63Vc7uZ0p1B4P870WnpD2859Y777LH0572fn1xag1bVsq0F1zVlUn0Hp50J7rTtrZzYXOdO47sBXWTDMApIN5XLlAuuY5kfRlHwVn84Ynk52\n");
	value = kv_store_read("fN7Y81yc006aAW5Uljl73NcCAjwh25");
	if (value != NULL) {
		printf("Read fN7Y81yc006aAW5Uljl73NcCAjwh25: %s\n", value);
		free(value);
	}

	kv_store_write("key1", "value2");
	printf("Write key1 -> value2\n");
	value = kv_store_read("key1");
	if (value != NULL) {
		printf("Read key1: %s\n", value);
		free(value);
	}

	kv_store_write("key1", "value3");
	printf("Write key1 -> value3\n");
	value = kv_store_read("key1");
	if (value != NULL) {
		printf("Read key1: %s\n", value);
		free(value);
	}

	kv_store_write("key1", "value4");
	printf("Write key1 -> value4\n");
	value = kv_store_read("key1");
	if (value != NULL) {
		printf("Read key1: %s\n", value);
		free(value);
	}

	value = kv_store_read("key1");
	if (value != NULL) {
		printf("Read key1: %s\n", value);
		free(value);
	}

	value = kv_store_read("key1");
	if (value != NULL) {
		printf("Read key1: %s\n", value);
		free(value);
	}

	value = kv_store_read("key1");
	if (value != NULL) {
		printf("Read key1: %s\n", value);
		free(value);
	}

	value = kv_store_read("key1");
	if (value != NULL) {
		printf("Read key1: %s\n", value);
		free(value);
	}

	value = kv_store_read("key1");
	if (value != NULL) {
		printf("Read key1: %s\n", value);
		free(value);
	}

	value = kv_store_read("key1");
	if (value != NULL) {
		printf("Read key1: %s\n", value);
		free(value);
	}

	value = kv_store_read("key1");
	if (value != NULL) {
		printf("Read key1: %s\n", value);
		free(value);
	}

	value = kv_store_read("key1");
	if (value != NULL) {
		printf("Read key1: %s\n", value);
		free(value);
	}

	char **all_values = kv_store_read_all("key1");
	if (all_values != NULL) {
		for (int i = 0; all_values[i] != NULL; i++) {
			printf("read_all %d: %s\n", i, all_values[i]);
			free(all_values[i]);
		}
		free(all_values);
	}
}

void read_all_test_sean() {
	char **all_values = kv_store_read_all("key0");
	if (all_values != NULL) {
		for (int i = 0; all_values[i] != NULL; i++) {
			printf("read_all %d: %s\n", i, all_values[i]);
			free(all_values[i]);
		}
		free(all_values);
	}
}

// int main(int argc, char **argv) { // TODO: Remove this in final code!
// 	if (signal(SIGINT, handlerSIGINT) == SIG_ERR) { // Handle Ctrl-C interrupt
// 		printf("ERROR: Could not bind SIGINT signal handler\n");
// 		exit(EXIT_FAILURE);
// 	}
// 	kv_store_create("/seanstappas");

// 	for (int i = 0; i < 1024; i++) {
// 		char key[32];
// 		if (i % 2 == 0) {
// 			sprintf(key, "%s%d", "key", 135);
// 		} else {
// 			sprintf(key, "%s%d", "key", 216);
// 		}
// 		// int pod_number = hash(key) % NUMBER_OF_PODS;
// 		// printf("Pod number for %s: %d\n", key, pod_number);
// 		char value[256];
// 		sprintf(value, "%s%d", "value", i);
// 		printf("Write %s -> %s\n", key, value);
// 		kv_store_write(key, value);
// 	}

// 	printf("-------------------READ-------------------\n");

// 	for (int i = 0; i < 512; i++) {
// 		int suffix = 216;
// 		if (i % 2 == 0) {
// 			suffix = 135;
// 		}

// 		char key[32];
// 		sprintf(key, "%s%d", "key", suffix);
// 		char* value = kv_store_read(key);
// 		if (value != NULL) {
// 			printf("Read key%d -> %s\n", suffix, value);
// 			free(value);
// 		}
// 	}

// 	printf("----------------READ ALL----------------\n");

// 	char **all_values = kv_store_read_all("key216");
// 	if (all_values != NULL) {
// 		for (int i = 0; all_values[i] != NULL; i++) {
// 			printf("read_all %d: %s\n", i, all_values[i]);
// 			free(all_values[i]);
// 		}
// 		free(all_values);
// 	}

// 	kv_delete_db();
// }