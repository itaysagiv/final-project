#include "stdio.h"
#include <sys/select.h>
#include "time.h"
#include <gmp.h>
#include <unistd.h>
#include <fcntl.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <string.h>

typedef unsigned long int qword;
#define STDIN 0
#pragma pack(push, 64)
volatile int Memory[1024];
#pragma pack(pop)

static qword Average = 0;
static int Samples = 0;

#define NUMBER_OF_DIFFERENT_KEYS 1
#define KEY_LEN 4096
//#define KEY_LEN 2048
#define TIME_TO_CALC_EXP 16000
#define TIME_BETWEEN_ENCRYPTIONS 70000
#define SAME_KEYS_IN_A_ROW 120
#if KEY_LEN == 4096

#define TIME_TO_CALC_EXP 16000

#else
#define TIME_TO_CALC_EXP 5200
#endif

mpz_t* SquareMultiplyExp(mpz_t Base, mpz_t Modulu, mpz_t Exponent);

int main()
{
	MP_INT a;
	char Input[100];
	fd_set fds;
	int maxfd = STDIN, encNum = 0;
	int Counter = 0;
	int i;
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 1;
	struct timespec start, end;
	unsigned long long elapsedTime;
	int FirstTime = 1;
	mpz_t Key, Base, Modulu, *Result, Keys[NUMBER_OF_DIFFERENT_KEYS];
	int State = 0, currKey = 0;
	char rsaKeys[NUMBER_OF_DIFFERENT_KEYS][KEY_LEN / 4 + 3];
	memset(rsaKeys, 0, (NUMBER_OF_DIFFERENT_KEYS * (KEY_LEN / 4) + 3));
//	for (i = 0; i < NUMBER_OF_DIFFERENT_KEYS; i++){
//
//		char keyFileName[20];
//		sprintf(keyFileName, "keys/%d_%d.key",KEY_LEN, i);
//		FILE* fd = fopen(keyFileName, "r");
//
//		EVP_PKEY* privkey = NULL;
//		EVP_PKEY *pKey = NULL;
//
//		if (!PEM_read_PrivateKey(fd, &privkey, NULL, NULL))
//		{
//			fprintf(stderr, "Error loading RSA Private Key File.\n");
//			return 2;
//		}
//
//		RSA* pRsa = privkey->pkey.rsa;
//		memcpy(&rsaKeys[i][2], (char*) (pRsa->d->d), KEY_LEN/8);
//		rsaKeys[i][KEY_LEN / 8] = 0;
//		EVP_PKEY_free(privkey);
//
//
//		fclose(fd);
//
//	}
//	if (!EVP_PKEY_assign_RSA (pKey, privkey))
//	{
//		fprintf(stderr, "EVP_PKEY_assign_RSA: failed.\n");
//		return 3;
//	}


	printf("Starting to poll memory\r\n"
			"Please press enter to move to the next step\r\n");

#if KEY_LEN == 2048
	mpz_init_set_str (Key, "0x00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
				"00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
				"00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
				"00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000", 0);

	mpz_init_set_str (Base, "0x12345678123456781234567812345678123456781234567812345678123456781234567812345678123456781234567812345678123456781234567812345678"
			"12345678123456781234567812345678123456781234567812345678123456781234567812345678123456781234567812345678123456781234567812345678"
			"12345678123456781234567812345678123456781234567812345678123456781234567812345678123456781234567812345678123456781234567812345678"
			"12345678123456781234567812345678123456781234567812345678123456781234567812345678123456781234567812345678123456781234567812345678", 0);

	mpz_init_set_str (Modulu, "0xabcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234"
			"abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234"
			"abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234"
			"abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234", 0);
#else
	mpz_init_set_str (Key, "0x00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
					"00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
					"00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
					"00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
					"00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
					"00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
					"00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
					"00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000", 0);

		mpz_init_set_str (Base, "0x12345678123456781234567812345678123456781234567812345678123456781234567812345678123456781234567812345678123456781234567812345678"
				"12345678123456781234567812345678123456781234567812345678123456781234567812345678123456781234567812345678123456781234567812345678"
				"12345678123456781234567812345678123456781234567812345678123456781234567812345678123456781234567812345678123456781234567812345678"
				"12345678123456781234567812345678123456781234567812345678123456781234567812345678123456781234567812345678123456781234567812345678"
				"12345678123456781234567812345678123456781234567812345678123456781234567812345678123456781234567812345678123456781234567812345678"
				"12345678123456781234567812345678123456781234567812345678123456781234567812345678123456781234567812345678123456781234567812345678"
				"12345678123456781234567812345678123456781234567812345678123456781234567812345678123456781234567812345678123456781234567812345678"
				"12345678123456781234567812345678123456781234567812345678123456781234567812345678123456781234567812345678123456781234567812345678", 0);

		mpz_init_set_str (Modulu, "0xabcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234"
				"abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234"
				"abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234"
				"abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234"
				"abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234"
				"abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234"
				"abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234"
				"abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234", 0);

#endif

	while(1){

		FD_ZERO(&fds);
		FD_SET(STDIN, &fds);

		timeout.tv_sec = 0;
		timeout.tv_usec = 0;

		select(maxfd+1, &fds, NULL, NULL, &timeout);

		// Check if the user is asking us to move to the next stage of the attack
		if (State == 0 && FD_ISSET(STDIN, &fds)){
			scanf("%s", Input);
			printf("Moving to 1's key\r\n");

#if KEY_LEN == 2048
			mpz_init_set_str (Key, "0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
					"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
					"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
					"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff", 0);
#else
			mpz_init_set_str (Key, "0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
								"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
								"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
								"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
								"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
								"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
								"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
								"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff", 0);
#endif
			State = 1;
			Average = 0;
			Samples = 0;
		}
		else if (State == 1 && FD_ISSET(STDIN, &fds))
		{
			scanf("%s", Input);
			printf("Moving to real key\r\n");

#if KEY_LEN == 2048
			mpz_set_str (Key, "0x023456789abcdef1dcba987654321112233445566778899aabbccddeef1eeddccbbaa99887766554433221100111222333444555666777888999aaabbbcccddd"
								"101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f"
								"505152535455565758595a5b5c5d5e5f606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e70f808182838485868788898a8b8c8d8e8"
								"909192939495969798999a9b9c9d9e91a0a1a2a3a4a5a6a7a8a9aaabacadaea1b0b1b2b3b4b5b6b7b8b9babbbcbdbeb1c0c1c2c3c4c5c6c7c8c9cacbcccdcec1", 0);
			/*
			mpz_set_str (Key, "0x55555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555"
								"55555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555"
								"55555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555"
								"55555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555", 0);
								*/
#else
//			for (i = 0; i < NUMBER_OF_DIFFERENT_KEYS; i++){
//				mpz_init_set_str(Keys[i], "0x76931fac9dab2b36c248b87d6ae33f9a62d7183a5d5789e4b2d6b441e2411dc709e111c7e1e7acb6f8cac0bb2fc4c8bc2ae3baaab9165cc458e199cb89f51b135f7091a5abb0874df3e8cb4543a5eb93b0441e9ca4c2b0fb3d30875cbf29abd5b1acf38984b35ae882809dd4cfe7abc5c61baa52e053b4c3643f204ef259d2e98042a948aac5e884cb3ec7db925643fd34fdd467e2cca406035cb2744cb90a63e51c9737903343947e02086541e4c48a99630aa9aece153843a4b190274ebc955f8592e30a2205a485846248987550aaf2094ec59e7931dc650c7451cc61c0cb2c46a1b3f2c349faff763c7f8d14ddff946351744378d62c59285a8d7915614f5a2ac9e0d68aca6248a9227ab8f1930ee38ac7a9d239c9b026a481e49d53161f9a9513fe5271c32e9c21d156eb9f1bea57f6ae4f1b1de3b7fd9cee2d9cca7b4c242d26c31d000b7f90b7fe48a131c7debfbe58165266de56e1edf26939af07ec69ab1b17d8db62143f2228b51551c3d2c7de3f5072bd4d18c3aeb64cb9e8cba838667b6ed2b2fcab04abae8676e318b402a7d15b30d2d7ddb78650cc6af82bc3d7aa805b02dd9aa523b7374a1323ee6b516d1b81e5f709c2c790edaf1c3fa9b0a1dbc6dabc2b5ed267244c458752002b106d6381fad58a7e193657bde0fe029120f8379316891f828b8d24a049e5b86d855bcfed56765f9da1ac54caeaf9257a", 0);
//				mpz_init_set_str(Keys[i], rsaKeys[i], 0);
//				Keys[i][0]._mp_size = 65;
//				mpz_init_set_str(Keys[i], rsaKeys[i], 16);
//				memcpy((char*)(Keys[i][0]._mp_d), rsaKeys[i], KEY_LEN / 8);


//			}


			mpz_set_str (Key, "0x023456789abcdef1dcba987654321112233445566778899aabbccddeef1eeddccbbaa99887766554433221100111222333444555666777888999aaabbbcccddd"
								"101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f"
								"505152535455565758595a5b5c5d5e5f606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e70f808182838485868788898a8b8c8d8e8"
								"909192939495969798999a9b9c9d9e91a0a1a2a3a4a5a6a7a8a9aaabacadaea1b0b1b2b3b4b5b6b7b8b9babbbcbdbeb1c0c1c2c3c4c5c6c7c8c9cacbcccdcec1"
								"d0d1d2d3d4d5d6d7d8d9dadbdcddded1e0e1e2e3e4e5e6e7e8e9eaebecedeee1f0f1f2f3f4f5f6f7f8f91a1b1c1d1e1f123456789abcdef1dcba987654321521"
								"18152229364350576471788592991061131201271341411481551621691761831901972032102172242312392462532602672742812892963033103173243313"
								"01123581321345589144233377610987159725844181676510946177112865746368750251213931964183178115142298320401346269217830935245785702"
								"123456789abcdef1dcba987654321112233445566778899aabbccddeef1eeddccbbaa99887766554433221100111222333444555666777888999aaabbbcccddd", 0);


			/*
			mpz_set_str (Key, "0xf97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11"
								"f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11"
								"f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11"
								"f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11"
								"f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11"
								"f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11"
								"f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11"
								"f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11f97c6e11", 0);
			mpz_init_set (Keys[0], Key);
			*/
#endif
			State = 2;
			Average = 0;
			Samples = 0;
		}
		// If we are in the real key simulation stage, wait a little between one encryption to another to do "Other things".
		else if (State == 2)
		{
			struct timespec start, end;
			qword TimeSpent;

			clock_gettime(CLOCK_REALTIME, &start);

			do {
				clock_gettime(CLOCK_REALTIME, &end);
				TimeSpent = (qword)(end.tv_nsec - start.tv_nsec) + (qword)(end.tv_sec - start.tv_sec) * 1000000000;
			} while (TimeSpent < TIME_BETWEEN_ENCRYPTIONS);
		}

		//gmp_printf("Exp: %Zx\r\n", Key);
		Result = SquareMultiplyExp(Base, Modulu, Key);
		if( (encNum % SAME_KEYS_IN_A_ROW) == 0){
			encNum =0;
			if (currKey == NUMBER_OF_DIFFERENT_KEYS - 1)
				currKey = 0;
			else
				currKey++;
		}
		encNum++;
		mpz_clear(*Result);
		free(Result);
	 }

	return 0;
}

mpz_t* SquareMultiplyExp(mpz_t Base, mpz_t Modulu, mpz_t Exponent)
{
	mpz_t* r = (mpz_t*)malloc(sizeof(mpz_t));
	int i;
	mpz_t tmp;

	mpz_init_set_str(*r, "1", 10);
	mpz_init(tmp);

	for (i = (KEY_LEN - 1); i >= 0; i--)
	{
		struct timespec start, end;
		qword TimeSpent;

		clock_gettime(CLOCK_REALTIME, &start);

		mpz_set(tmp, *r);

		//r = (r * r) % Modulu;
		mpz_powm_ui(*r, tmp, 2, Modulu);

		if (mpz_tstbit(Exponent, i)) {
			mpz_mul(tmp, *r, Base);
			mpz_mod(*r, tmp, Modulu);
		} else
		{
			int x = 5;
		}

/*
		clock_gettime(CLOCK_REALTIME, &end);
		TimeSpent = (qword)(end.tv_nsec - start.tv_nsec) + (qword)(end.tv_sec - start.tv_sec) * 1000000000;
		printf("Spent %d nano seconds in exponent func, average\r\n", TimeSpent);
*/
		do {
			clock_gettime(CLOCK_REALTIME, &end);
			TimeSpent = (qword)(end.tv_nsec - start.tv_nsec) + (qword)(end.tv_sec - start.tv_sec) * 1000000000;
		} while (TimeSpent < TIME_TO_CALC_EXP);

		/*
		clock_gettime(CLOCK_REALTIME, &end);
		TimeSpent = (qword)(end.tv_nsec - start.tv_nsec) + (qword)(end.tv_sec - start.tv_sec) * 1000000000;

		if (TimeSpent > 25000)
			continue;

		Average += TimeSpent;
		Samples++;
		printf("Spent %d nano seconds in exponent func, average %d\r\n", TimeSpent, Average/Samples);
*/
	}

	mpz_clear(tmp);
	return r;
}
