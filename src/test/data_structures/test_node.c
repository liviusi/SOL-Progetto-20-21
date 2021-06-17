/**
 * @brief Tests for node struct and related functions.
 * @author Giacomo Trapani.
*/

#include <node.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>


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
	strncpy(key, node1_tmp_key, 128);
	strncpy(data, node1_tmp_data, 128);
	node_t* node1 = Node_Create(key, strlen(key) + 1, (void*) data, strlen(data) + 1, NULL);
	strncpy(key, node2_tmp_key, 128);
	strncpy(data, node2_tmp_data, 128);
	node_t* node2 = Node_Create(key, strlen(key) + 1, (void*) data, strlen(data) + 1, NULL);
	strncpy(key, node3_tmp_key, 128);
	strncpy(data, node3_tmp_data, 128);
	node_t* node3 = Node_Create(key, strlen(key) + 1, (void*) data, strlen(data) + 1, NULL);
	strncpy(key, node4_tmp_key, 128);
	strncpy(data, node4_tmp_data, 128);
	node_t* node4 = Node_Create(key, strlen(key) + 1, (void*) data, strlen(data) + 1, NULL);
	strncpy(key, node5_tmp_key, 128);
	strncpy(data, node5_tmp_data, 128);
	node_t* node5 = Node_Create(key, strlen(key) + 1, (void*) data, strlen(data) + 1, NULL);
	strncpy(key, node6_tmp_key, 128);
	strncpy(data, node6_tmp_data, 128);
	node_t* node6 = Node_Create(key, strlen(key) + 1, (void*) data, strlen(data) + 1, NULL);
	strncpy(key, node7_tmp_key, 128);
	strncpy(data, node7_tmp_data, 128);
	node_t* node7 = Node_Create(key, strlen(key) + 1, (void*) data, strlen(data) + 1, NULL);
	strncpy(key, node8_tmp_key, 128);
	strncpy(data, node8_tmp_data, 128);
	node_t* node8 = Node_Create(key, strlen(key) + 1, (void*) data, strlen(data) + 1, NULL);
	strncpy(key, node9_tmp_key, 128);
	strncpy(data, node9_tmp_data, 128);
	node_t* node9 = Node_Create(key, strlen(key) + 1, (void*) data, strlen(data) + 1, NULL);
	strncpy(key, node10_tmp_key, 128);
	strncpy(data, node10_tmp_data, 128);
	node_t* node10 = Node_Create(key, strlen(key) + 1, (void*) data, strlen(data) + 1, NULL);
	free(key);
	free(data);
	Node_SetNext(node1, node2);
	Node_SetNext(node2, node3);
	Node_SetNext(node3, node4);
	Node_SetNext(node4, node5);
	Node_SetNext(node5, node6);
	Node_SetNext(node6, node7);
	Node_SetNext(node7, node8);
	Node_SetNext(node8, node9);
	Node_SetNext(node9, node10);
	Node_SetPrevious(node2, node1);
	Node_SetPrevious(node3, node2);
	Node_SetPrevious(node4, node3);
	Node_SetPrevious(node5, node4);
	Node_SetPrevious(node6, node5);
	Node_SetPrevious(node7, node6);
	Node_SetPrevious(node8, node7);
	Node_SetPrevious(node9, node8);
	Node_SetPrevious(node10, node9);
	node_t* tmp; int i = 1;
	fprintf(stderr, "Iterating over next:\n");
	while (1)
	{
		if (i == 1)
		{
			i = 0;
			tmp = node1;
		}
		else tmp = (node_t*) Node_GetNext(tmp);
		if (tmp == NULL) break;
		Node_CopyKey(tmp, &key);
		Node_CopyData(tmp, (void**) &data);
		fprintf(stderr, "%s - %s\n", key, data);
		free(key);
		free(data);
	}

	Node_ReplaceWithPrevious(&node2);
	Node_ReplaceWithNext(&node1);
	Node_ReplaceWithNext(&node10);
	Node_Fold(node3);
	Node_Fold(node9);
	fprintf(stderr, "Nodes 1, 2, 3, 9, 10 have been purged.\n");
	fprintf(stderr, "Iterating over previous node.\n");
	while (1)
	{
		if (i == 0)
		{
			i = 1;
			tmp = node8;
		}
		else tmp = (node_t*) Node_GetPrevious(tmp);
		if (tmp == NULL) break;
		Node_CopyKey(tmp, &key);
		Node_CopyData(tmp, (void**) &data);
		fprintf(stderr, "%s - %s\n", key, data);
		free(key);
		free(data);
	}
	fprintf(stderr, "Iterating over next node.\n");
	while (1)
	{
		if (i == 1)
		{
			i = 0;
			tmp = node4;
		}
		else tmp = (node_t*) Node_GetNext(tmp);
		if (tmp == NULL) break;
		Node_CopyKey(tmp, &key);
		Node_CopyData(tmp, (void**) &data);
		fprintf(stderr, "%s - %s\n", key, data);
		free(key);
		free(data);
	}
	
	fprintf(stderr, "Attempting to free allocated resources.\n");
	Node_Free(node4);
	Node_Free(node5);
	Node_Free(node6);
	Node_Free(node7);
	Node_Fold(node8);
	fprintf(stderr, "Resources have been freed.\n");
}