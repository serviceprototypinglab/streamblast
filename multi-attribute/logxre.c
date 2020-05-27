#include <stdio.h>
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include <string.h>
#include <poll.h>
#include <sys/ioctl.h>

#define MAXBUFFERSIZE 1024 * 1024 * 16
#define OPPORTUNISTIC_JUMP 0

#define FORMAT_PSQL 0
#define FORMAT_ELASTIC 1

#define DEBUG 0

static int matchline(pcre2_code *re, pcre2_match_data *mdata, PCRE2_SPTR subj, PCRE2_SIZE subjsize, char format, int *lineid, char expect, const char *nsgmapper){
	int res = 0;
	//uint32_t mcount = 0;
	PCRE2_SIZE *mvector = NULL;
	char buffer[1024];
	int len = 0;
	PCRE2_SIZE offset = 0;
	int i = 0;
	PCRE2_SPTR where = NULL;
	const char *nsg = NULL;

	if(format == FORMAT_ELASTIC)
		pcre2_pattern_info(re, PCRE2_INFO_NAMETABLE, &where);

	while(1){
		res = pcre2_match(re, subj + offset, subjsize - offset, 0, 0, mdata, NULL);
#if DEBUG
		printf("-------\n");
		printf("Res: %i / expect: %i\n", res, expect);
#endif
		if(res > 0){
			//mcount = pcre2_get_ovector_count(mdata);
			mvector = pcre2_get_ovector_pointer(mdata);
#if DEBUG == 2
			printf("Count total: %i\n", res);
#endif
			// TODO specific to fwsyslog.re; might need checks for others or use + instead of * in regexp
			if((expect == 0) || (res == expect)){
				for(i = 1; i < res; i++){
#if DEBUG == 2
					printf("%i: %lu..%lu\n", i, mvector[2 * i], mvector[2 * i + 1]);
#endif
					len = mvector[2 * i + 1] - mvector[2 * i];
					if(len >= sizeof(buffer)){
						printf("! match too large :(\n");
					}else{
						strncpy(buffer, subj + offset + mvector[2 * i], len);
						buffer[len] = '\0';
#if DEBUG == 2
						//printf("%s\n", buffer);
						//puts(buffer);
#endif
						if(format == FORMAT_PSQL){
							fputs(buffer, stdout);
							if(i < res - 1){
								fputs("\t", stdout);
							}else{
								fputs("\n", stdout);
							}
						}else if(format == FORMAT_ELASTIC){
							if(i == 1){
								printf("{\"index\":{\"_id\":\"%i\"}}\n{", *lineid);
								*lineid += 1;
							}
							//printf("\"f%i\":\"%s\"", i, buffer);
							nsg = (const char*)where + 2 + nsgmapper[0] * nsgmapper[i];
							printf("\"%s\":\"%s\"", nsg, buffer);
							if(i < res - 1){
								fputs(",", stdout);
							}else{
								fputs("}\n", stdout);
							}
						}
					}
				}
			}
			offset += mvector[1] + OPPORTUNISTIC_JUMP;
			mvector = NULL;
		}else{
			break;
		}
	}

	return 0;
}

char *buildmapper(const pcre2_code *re){
	PCRE2_SPTR where = NULL;
	uint32_t namecount = 0;
	uint32_t namesize = 0;
	char *nsgmapper = NULL;
	int res = 0;

	res = pcre2_pattern_info(re, PCRE2_INFO_NAMECOUNT, &namecount);
	res = pcre2_pattern_info(re, PCRE2_INFO_NAMEENTRYSIZE, &namesize);
	res = pcre2_pattern_info(re, PCRE2_INFO_NAMETABLE, &where);
	nsgmapper = (char*)malloc(sizeof(char) * (namecount + 1));
#if DEBUG
	printf("[%i] ??SIZE [%ux%u]\n", res, namecount, namesize);
	for(int j = 0; j < namecount; j++)
		printf("[%i] ??NAME (%i) %s\n", res, *(where + 1 + namesize * j), where + 2 + namesize * j);
#endif
	nsgmapper[0] = namesize;
	for(int j = 0; j < namecount; j++)
		nsgmapper[*(where + 1 + namesize * j)] = j;

	return nsgmapper;
}

int main(int argc, char *argv[]){
	const char *logfile = NULL;
	const char *refile = NULL;
	FILE *f = NULL;
	char *rbuffer = NULL;
	int res = 0;
	pcre2_code *re = NULL;
	const char *pat = NULL;
	int error = 0;
	PCRE2_SIZE erroroffset;
	char buffer[1024];
	uint32_t options = 0;
	pcre2_match_data *mdata = NULL;
	size_t buffersize = 0;
	size_t filesize = 0;
	size_t oldpos = 0;
	char regex[1024];
	char format = FORMAT_PSQL;
	int lineid = 0;
	int expect = 0;
	char *nsgmapper = NULL;

	if((argc != 3) && (argc != 4)){
		printf("ERROR: Syntax: streamblast-m <file.log> <refile.re> [elastic]\n");
		return -1;
	}

	logfile = argv[1];
	refile = argv[2];
	if(argc == 4){
		if(!strcmp(argv[3], "elastic"))
			format = FORMAT_ELASTIC;
	}

	f = fopen(refile, "r");
	if(!f){
		printf("ERROR: Cannot open regular expression file '%s'.\n", refile);
		return -1;
	}

	res = fread(regex, sizeof(regex) - 1, 1, f);
	regex[ftell(f)] = '\0';
	if(regex[strlen(regex) - 1] == '\n'){
		regex[strlen(regex) - 1] = '\0';
	}

	fclose(f);

#if DEBUG
	printf("Configuration: %s %s\n", logfile, refile);
	printf("RE: %s [%i]\n", regex, res);
#endif

	if(!strcmp(logfile, "-")) {
		f = stdin;
		setvbuf(stdin, NULL, _IONBF, 0);
	} else {
		f = fopen(logfile, "r");
		if(!f){
			printf("ERROR: Cannot open logfile '%s'.\n", logfile);
			return -1;
		}
	}

	buffersize = MAXBUFFERSIZE;
	res = fseek(f, 0, SEEK_END);
	if(res == 0){
		filesize = ftell(f);
		rewind(f);
		if(filesize < buffersize)
			buffersize = filesize;
	}

	rbuffer = malloc(buffersize);

	pat = regex;
	re = pcre2_compile(pat, strlen(pat), options, &error, &erroroffset, NULL);
	if(!re){
		pcre2_get_error_message(error, buffer, sizeof(buffer));
		printf("ERROR: NOT COMPILED! %s\n", buffer);
		return -1;
	}

	mdata = pcre2_match_data_create_from_pattern(re, NULL);
	expect = pcre2_get_ovector_count(mdata);

	nsgmapper = buildmapper(re);

	while(1){
		if(filesize != 0)
			oldpos = ftell(f);
		else{
#if DEBUG
			printf("!feof=%i\n", feof(f));
#endif
			fflush(stdout);

			struct pollfd pfd;
			pfd.fd = 0;
			pfd.events = POLLIN;
			while(1){
				res = poll(&pfd, 1, -1); // -1/1000
#if DEBUG
				printf("poll %i -> res %i [%i]\n", pfd.revents & POLLIN, res, pfd.revents);
				printf("!res=%i\n", res);
#endif
				if(pfd.revents & POLLIN)
					break;
			}
			ioctl(0, FIONREAD, &buffersize);
		}
#if DEBUG
		printf("bufsize: %li\n", buffersize);
#endif
		res = fread(rbuffer, buffersize, 1, f);
#if DEBUG
		printf("res=%i ferror=%i feof=%i diff=%li\n", res, ferror(f), feof(f), ftell(f) - oldpos);
#endif
		if((filesize != 0) && (ftell(f) - oldpos < buffersize))
			buffersize = ftell(f) - oldpos;

		matchline(re, mdata, (PCRE2_SPTR)rbuffer, (PCRE2_SIZE)buffersize, format, &lineid, expect, nsgmapper);

		if(res == 0){
			break;
		}
		if(filesize != 0){
			if(ftell(f) == filesize)
				break;
			// TODO trackback does not work in streaming mode yet
			fseek(f, -60, SEEK_CUR);
		}
	}

	free(nsgmapper);
	nsgmapper = NULL;

	pcre2_match_data_free(mdata);
	mdata = NULL;

	pcre2_code_free(re);
	re = NULL;

	free(rbuffer);

	fclose(f);
	f = NULL;

	return 0;
}
