#include <stdio.h>

const int NUMBER_OF_NODES = 5;
const int INF = 999;
int SRC = 0;
int DST = 4;

const char NODES[] = {'A', 'B', 'C', 'D', 'E'};

int main() {
	int FromToCostMatrix[NUMBER_OF_NODES][NUMBER_OF_NODES], distance[NUMBER_OF_NODES + 1], previous[NUMBER_OF_NODES], minDist, i, j;
	bool visited[NUMBER_OF_NODES];
	distance[NUMBER_OF_NODES] = INF;

	for (i = 0; i < NUMBER_OF_NODES; i++)
		for (j = 0; j < NUMBER_OF_NODES; j++)
			printf("From %c to %c distance: ", NODES[i], NODES[j]) && scanf_s("%d", &FromToCostMatrix[i][j]);
	printf("Source node index: ") && scanf_s("%d", &SRC) && printf("Destination node index: ") && scanf_s("%d", &DST);

	//Start Dijkstra's Algorithm
	for (i = 0; i < NUMBER_OF_NODES; i++)
		distance[i] = (visited[i] = i == (previous[i] = SRC)) ? 0 : FromToCostMatrix[SRC][i];
	for (i = 1; i < NUMBER_OF_NODES - 1; i++) {
		for (j = 0, minDist = NUMBER_OF_NODES; j < NUMBER_OF_NODES; j++) minDist = (!visited[j] && distance[j] < distance[minDist]) ? j : minDist;
		for (j = 0, visited[minDist] = true; j < NUMBER_OF_NODES; j++) distance[j] = (!visited[j] && distance[minDist] + FromToCostMatrix[minDist][j]) ? distance[previous[j] = minDist] + FromToCostMatrix[minDist][j] : distance[j];
	}
	//End Dijkstra's Algorithm

	printf("Distance from %c to %c = %d\n", NODES[SRC], NODES[DST], distance[DST]);
	printf("Path: %c", NODES[DST]);

	j = DST;
	do printf(" <- %c", NODES[j = previous[j]]); while (j != SRC);

	printf("\n");

	return 0;
}
