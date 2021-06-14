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
	strncpy(key, node6_tmp_key, 128);
	strncpy(data, node6_tmp_data, 128);
	LinkedList_PushFront(list, key, strlen(key) + 1, (void*) data, strlen(data) + 1);
	strncpy(key, node7_tmp_key, 128);
	strncpy(data, node7_tmp_data, 128);
	LinkedList_PushBack(list, key, strlen(key) + 1, (void*) data, strlen(data) + 1);
	strncpy(key, node8_tmp_key, 128);
	strncpy(data, node8_tmp_data, 128);
	LinkedList_PushFront(list, key, strlen(key) + 1, (void*) data, strlen(data) + 1);
	strncpy(key, node9_tmp_key, 128);
	strncpy(data, node9_tmp_data, 128);
	LinkedList_PushFront(list, key, strlen(key) + 1, (void*) data, strlen(data) + 1);
	strncpy(key, node10_tmp_key, 128);
	strncpy(data, node10_tmp_data, 128);
	LinkedList_PushBack(list, key, strlen(key) + 1, (void*) data, strlen(data) + 1);
	free(key);
	free(data);
	LinkedList_Print(list);
	LinkedList_PopFront(list, &key, (void**) &data);
	fprintf(stderr, "KEY : %s\tDATA : %s\n", key, data);
	LinkedList_Print(list);
	free(key);
	free(data);
	key = (char*) malloc(sizeof(char) * 128);
	data = (char*) malloc(sizeof(char) * 128);
	strncpy(key, node8_tmp_key, 128);
	strncpy(data, node8_tmp_data, 128);
	node_t* tmp = Node_Create(key, strlen(key)+1, (void*) data, strlen(data)+1);
	LinkedList_Fold(list, tmp);
	Node_Free(tmp);
	free(key);
	free(data);
	LinkedList_PopBack(list, &key, (void**) &data);
	fprintf(stderr, "KEY : %s\tDATA : %s\n", key, data);
	free(key);
	free(data);
	LinkedList_Print(list);
	LinkedList_PopBack(list, &key, (void**) &data);
	fprintf(stderr, "KEY : %s\tDATA : %s\n", key, data);
	LinkedList_Free(list);
	free(key);
	free(data);

	key = (char*) malloc(sizeof(char) * 128);
	data = (char*) malloc(sizeof(char) * 128);
	linked_list_t* tmp_list = LinkedList_Init();
	strncpy(key, node1_tmp_key, 128);
	strncpy(data, node1_tmp_data, 128);
	LinkedList_PushBack(tmp_list, key, strlen(key) + 1, (void*) data, strlen(data) + 1);
	free(key);
	free(data);
	LinkedList_PopBack(tmp_list, NULL, NULL);
	LinkedList_Free(tmp_list);
	return 0;
}