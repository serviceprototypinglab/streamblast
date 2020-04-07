#include <stdio.h>
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include <string.h>

#define MAXBUFFERSIZE 1024 * 1024 * 16
#define OPPORTUNISTIC_JUMP 0

static int matchline(pcre2_code *re, pcre2_match_data *mdata, PCRE2_SPTR subj, PCRE2_SIZE subjsize){
	int res = 0;
	//uint32_t mcount = 0;
	PCRE2_SIZE *mvector = NULL;
	char buffer[1024];
	int len = 0;
	PCRE2_SIZE offset = 0;
	int i = 0;

	while(1){
		res = pcre2_match(re, subj + offset, subjsize - offset, 0, 0, mdata, NULL);
		//printf("Res: %i\n", res);

		if(res > 0){
			//mcount = pcre2_get_ovector_count(mdata);
			mvector = pcre2_get_ovector_pointer(mdata);
			//printf("Count total: %i\n", res);
			for(i = 1; i < res; i++){
				//printf("%i: %lu..%lu\n", i, mvector[2 * i], mvector[2 * i + 1]);
				len = mvector[2 * i + 1] - mvector[2 * i];
				if(len >= sizeof(buffer)){
					printf("! match too large :(\n");
				}else{
					strncpy(buffer, subj + offset + mvector[2 * i], len);
					buffer[len] = '\0';
					//printf("%s\n", buffer);
					//puts(buffer);
					fputs(buffer, stdout);
					if(i < res - 1){
						fputs("\t", stdout);
					}else{
						fputs("\n", stdout);
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

	if(argc != 3){
		printf("ERROR: Syntax: logxre <file.log> <refile.re>\n");
		return -1;
	}

	refile = argv[2];
	logfile = argv[1];

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
	//printf("RE: %s [%i]\n", regex, res);

	fclose(f);

	f = fopen(logfile, "r");
	if(!f){
		printf("ERROR: Cannot open logfile '%s'.\n", logfile);
		return -1;
	}

	buffersize = MAXBUFFERSIZE;
	fseek(f, 0, SEEK_END);
	filesize = ftell(f);
	rewind(f);
	if(filesize < buffersize)
		buffersize = filesize;
	//printf("bufsize: %li\n", buffersize);

	rbuffer = malloc(buffersize);

	pat = regex;
	//".*src:([^;]*).*";
	re = pcre2_compile(pat, strlen(pat), options, &error, &erroroffset, NULL);
	if(!re){
		pcre2_get_error_message(error, buffer, sizeof(buffer));
		printf("ERROR: NOT COMPILED! %s\n", buffer);
		return -1;
	}

	mdata = pcre2_match_data_create_from_pattern(re, NULL);

	while(1){
		oldpos = ftell(f);
		res = fread(rbuffer, buffersize, 1, f);
		//printf("res=%i ferror=%i feof=%i diff=%li\n", res, ferror(f), feof(f), ftell(f) - oldpos);
		if(ftell(f) - oldpos < buffersize)
			buffersize = ftell(f) - oldpos;

		matchline(re, mdata, (PCRE2_SPTR)rbuffer, (PCRE2_SIZE)buffersize);

		if(res == 0){
			break;
		}
		if(ftell(f) == filesize){
			break;
		}
		fseek(f, -60, SEEK_CUR);
	}

	pcre2_match_data_free(mdata);
	mdata = NULL;

	pcre2_code_free(re);
	re = NULL;

	free(rbuffer);

	fclose(f);
	f = NULL;

	return 0;
}