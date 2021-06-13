/**
 * @brief Tests for hashtable struct and related functions.
 * @author Giacomo Trapani.
*/

#include <hashtable.h>
#include <linked_list.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main()
{
	char* key = (char*) malloc(sizeof(char) * 128);
	char* data = (char*) malloc(sizeof(char) * 128);
	char node1_tmp_key[] = "[KEY] 1";
	char node2_tmp_key[] = "[KEY] 2";
	char node3_tmp_key[] = "[KEY] 3";
	char node4_tmp_key[] = "[KEY] 4";
	char node5_tmp_key[] = "[KEY] 5";
	char node6_tmp_key[] = "[KEY] 6";
	char node7_tmp_key[] = "[KEY] 7";
	char node8_tmp_key[] = "[KEY] 8";
	char node9_tmp_key[] = "[KEY] 9";
	char node10_tmp_key[] = "[KEY] 10";
	char node1_tmp_data[] = "[DATA] 1";
	char node2_tmp_data[] = "[DATA] 2";
	char node3_tmp_data[] = "[DATA] 3";
	char node4_tmp_data[] = "[DATA] 4";
	char node5_tmp_data[] = "[DATA] 5";
	char node6_tmp_data[] = "[DATA] 6";
	char node7_tmp_data[] = "[DATA] 7";
	char node8_tmp_data[] = "[DATA] 8";
	char node9_tmp_data[] = "[DATA] 9";
	char node10_tmp_data[] = "[DATA] 10";
	hashtable_t* table = HASHTABLE_INITIALIZER(5);
	strncpy(key, node1_tmp_key, 128);
	strncpy(data, node1_tmp_data, 128);
	HashTable_Insert(table, key, strlen(key) + 1, (void*) data, strlen(data) + 1);
	strncpy(key, node2_tmp_key, 128);
	strncpy(data, node2_tmp_data, 128);
	HashTable_Insert(table, key, strlen(key) + 1, (void*) data, strlen(data) + 1);
	strncpy(key, node3_tmp_key, 128);
	strncpy(data, node3_tmp_data, 128);
	HashTable_Insert(table, key, strlen(key) + 1, (void*) data, strlen(data) + 1);
	strncpy(key, node4_tmp_key, 128);
	strncpy(data, node4_tmp_data, 128);
	HashTable_Insert(table, key, strlen(key) + 1, (void*) data, strlen(data) + 1);
	strncpy(key, node5_tmp_key, 128);
	strncpy(data, node5_tmp_data, 128);
	HashTable_Insert(table, key, strlen(key) + 1, (void*) data, strlen(data) + 1);
	strncpy(key, node6_tmp_key, 128);
	strncpy(data, node6_tmp_data, 128);
	HashTable_Insert(table, key, strlen(key) + 1, (void*) data, strlen(data) + 1);
	strncpy(key, node7_tmp_key, 128);
	strncpy(data, node7_tmp_data, 128);
	HashTable_Insert(table, key, strlen(key) + 1, (void*) data, strlen(data) + 1);
	strncpy(key, node8_tmp_key, 128);
	strncpy(data, node8_tmp_data, 128);
	HashTable_Insert(table, key, strlen(key) + 1, (void*) data, strlen(data) + 1);
	strncpy(key, node9_tmp_key, 128);
	strncpy(data, node9_tmp_data, 128);
	HashTable_Insert(table, key, strlen(key) + 1, (void*) data, strlen(data) + 1);
	strncpy(key, node10_tmp_key, 128);
	strncpy(data, node10_tmp_data, 128);
	HashTable_Insert(table, key, strlen(key) + 1, (void*) data, strlen(data) + 1);
	HashTable_Insert(table, key, strlen(key) + 1, (void*) data, strlen(data) + 1);
	HashTable_Print(table);
	printf("\n\n");
	HashTable_DeleteNode(table, node1_tmp_key);
	HashTable_DeleteNode(table, node7_tmp_key);
	HashTable_DeleteNode(table, node9_tmp_key);
	HashTable_Print(table);
	printf("\n\n");
	HashTable_DeleteNode(table, node5_tmp_key);
	HashTable_DeleteNode(table, node5_tmp_key);
	HashTable_Print(table);
	HashTable_Free(table);
	free(key);
	free(data);
}