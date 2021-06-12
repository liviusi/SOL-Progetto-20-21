/**
 * @brief Tests for linked_list struct and related functions.
 * @author Giacomo Trapani.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <node.h>
#include <linked_list.h>


int main()
{
	char* key = (char*) malloc(sizeof(char) * 128);
	char* data = (char*) malloc(sizeof(char) * 128);
	char node1_tmp_key[] = "[KEY] 1";
	char node2_tmp_key[] = "[KEY] 2";
	char node3_tmp_key[] = "[KEY] 3";
	char node4_tmp_key[] = "[KEY] 4";
	char node5_tmp_key[] = "[KEY] 5";
	char node1_tmp_data[] = "[DATA] 1";
	char node2_tmp_data[] = "[DATA] 2";
	char node3_tmp_data[] = "[DATA] 3";
	char node4_tmp_data[] = "[DATA] 4";
	char node5_tmp_data[] = "[DATA] 5";

	linked_list_t* list = LinkedList_Init();
	strncpy(key, node1_tmp_key, 128);
	strncpy(data, node1_tmp_data, 128);
	LinkedList_PushBack(list, key, strlen(key) + 1, (void*) data, strlen(data) + 1);
	strncpy(key, node2_tmp_key, 128);
	strncpy(data, node2_tmp_data, 128);
	LinkedList_PushBack(list, key, strlen(key) + 1, (void*) data, strlen(data) + 1);
	strncpy(key, node3_tmp_key, 128);
	strncpy(data, node3_tmp_data, 128);
	LinkedList_PushFront(list, key, strlen(key) + 1, (void*) data, strlen(data) + 1);
	strncpy(key, node4_tmp_key, 128);
	strncpy(data, node4_tmp_data, 128);
	LinkedList_PushBack(list, key, strlen(key) + 1, (void*) data, strlen(data) + 1);
	strncpy(key, node5_tmp_key, 128);
	strncpy(data, node5_tmp_data, 128);
	LinkedList_PushFront(list, key, strlen(key) + 1, (void*) data, strlen(data) + 1);
	free(key);
	free(data);
	LinkedList_Print(list);
	LinkedList_PopFront(list, &key, (void**) &data);
	fprintf(stderr, "KEY : %s\tDATA : %s\n", key, data);
	free(key);
	free(data);
	LinkedList_Print(list);
	LinkedList_PopBack(list, &key, (void**) &data);
	fprintf(stderr, "KEY : %s\tDATA : %s\n", key, data);
	free(key);
	free(data);
	LinkedList_Print(list);
	LinkedList_PopBack(list, &key, (void**) &data);
	fprintf(stderr, "KEY : %s\tDATA : %s\n", key, data);
	free(key);
	free(data);
	LinkedList_Free(list);
	return 0;
}