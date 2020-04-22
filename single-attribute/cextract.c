/*
 * Invocation synopsis:
 * ./cextract <file.log> <key:> <outputformat>
 */

#include <stdio.h>
#include <string.h>
#include <stddef.h>

/* Higher is better, but too high may mean subsequent line is gulped */
#define MAXHOSTNAMELEN 64

/* Higher is better (but crashes with 8 MB) */
#define BUFFERSIZE 1024 * 1024 * 4

/* Higher is better (must be divider of TRANSACTIONWINDOW, & until psql limit) */
#define MULTIROWS 1000

/* Higher is better (express infinity by setting to 0) */
#define TRANSACTIONWINDOW 10000

/* Enumerated output formats */
#define OUTPUT_PLAIN 1
#define OUTPUT_JSON 2
#define OUTPUT_SQL 3
#define OUTPUT_PSQL 4
#define OUTPUT_PSQLCOPY 5

static void run(const char *filename, const char *pattern, char output){
	FILE *f = NULL;
	char buffer[BUFFERSIZE];
	size_t len = 0;
	char *ptr = NULL;
	char *tok = NULL;
	int offset = 0;
	long ctr = 0;

	if(output == OUTPUT_SQL) {
		printf("CREATE TABLE cextract (src string);\n");
		printf("BEGIN;\n");
	} else if(output == OUTPUT_PSQL) {
		printf("DROP TABLE IF EXISTS cextract;\n");
		//printf("CREATE UNLOGGED TABLE cextract (src varchar(256));\n");
		printf("CREATE TABLE cextract (src varchar(256));\n");
		printf("\\set ON_ERROR_ROLLBACK on\n");
		//printf("SET synchronous_commit TO OFF;\n");
		printf("BEGIN;\n");
	} else if(output == OUTPUT_PSQLCOPY) {
		printf("DROP TABLE IF EXISTS cextract;\n");
		printf("CREATE TABLE cextract (src varchar(256));\n");
		printf("\\copy cextract FROM STDIN;\n");
	}

	if(!strcmp(filename, "-")) {
		f = stdin;
	} else {
		f = fopen(filename, "r");
		if(!f) {
			printf("ERROR: Cannot open logfile '%s'.\n", filename);
			return;
		}
	}

	offset = strlen(pattern);

	do {
		len = fread(buffer, sizeof(buffer), 1, f);
		//printf("read %zd\n", len);
		//printf("buf: %s\n", buffer);
		if(feof(f)) {
			//printf("ok eof\n");
		} else {
			//fseek(f, -100, SEEK_CUR);
		};
		ptr = buffer;
		do {
			char *ptrold = ptr;
			ptr = strstr(ptr, pattern);
			if(ptr) {
				if(*(ptr - 1) == ' ') {
					ptr += 1;
					continue;
				}
				tok = strtok(ptr, ";");
				if(ptr - buffer + strlen(tok) >= sizeof(buffer)) {
					//printf("overread token: %s [at %zd]\n", tok, ptr - buffer);
					fseek(f, -MAXHOSTNAMELEN, SEEK_CUR);
					break;
				} else {
					//printf("token: %s [at %zd..%zd] (%zd)\n", tok, ptr - buffer, ptr - buffer + strlen(tok), sizeof(buffer));
				}
				if(tok) {
					tok += offset;
					if(output == OUTPUT_PLAIN) {
						printf("%s\n", tok);
					} else if(output == OUTPUT_JSON) {
						printf("{%s: \"%s\", @timestamp: \"2013-12-11T08:01:45.000Z\"}\n", pattern, tok);
					} else if(output == OUTPUT_SQL) {
						printf("INSERT INTO cextract (src) VALUES (\"%s\");\n", tok);
					} else if(output == OUTPUT_PSQL) {
						//printf("INSERT INTO cextract (src) VALUES ('%s');\n", tok);
						if(ctr % MULTIROWS == 0) {
							printf("INSERT INTO cextract (src) VALUES ('%s')", tok);
						} else {
							printf(", ('%s')", tok);
						}
					} else if(output == OUTPUT_PSQLCOPY) {
						printf("%s\n", tok);
					}

					ctr += 1;
					if(ctr % MULTIROWS == 0) {
						if(output == OUTPUT_PSQL) {
							printf(";\n"); /* multirow */
						}
					}
#if TRANSACTIONWINDOW
					if(ctr % TRANSACTIONWINDOW == 0) {
						if(output == OUTPUT_PSQL) {
							printf("END;\n");
							printf("BEGIN;\n");
						}
					}
#endif
				}
				ptr += MAXHOSTNAMELEN;
			};
		} while(ptr);
	} while(len > 0);

	fclose(f);

	if(output == OUTPUT_SQL) {
		printf("END;\n");
	} else if(output == OUTPUT_PSQL) {
		printf(";\n"); /* multirow */
		printf("END;\n");
	}
}

int main(int argc, char *argv[]){
	char output = OUTPUT_PLAIN;
	char *outputstr = NULL;

	if((argc == 3) || (argc == 4)) {
		if(argc == 4) {
			outputstr = argv[3];
			if(strcmp(outputstr, "plain") == 0) {
				output = OUTPUT_PLAIN;
			} else if(strcmp(outputstr, "json") == 0) {
				output = OUTPUT_JSON;
			} else if(strcmp(outputstr, "sql") == 0) {
				output = OUTPUT_SQL;
			} else if(strcmp(outputstr, "psql") == 0) {
				output = OUTPUT_PSQL;
			} else if(strcmp(outputstr, "psqlcopy") == 0) {
				output = OUTPUT_PSQLCOPY;
			}
		}
		run(argv[1], argv[2], output);
	} else {
		printf("Run: ./cextract <file.log> <key:> <outputformat:plain(default),json,sql,psql,psqlcopy>\n");
	}

	return 0;
}
