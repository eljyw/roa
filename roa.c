#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>

// Attention aux indiens  little indian vs big indian !!!
// ce code ne marche que sur du x86 (octets inversés) et affiliés
// et considère les tailles des types suivants (en octets)
// sizeof(int) = 4
// sizeof(long) = 8
// sizeof(float) = 4
// sizeof(double) = 8
//
// La lecture du fichier se fera Element par element, en effet
// il y a des incohérences entre les tailles des structures
// et le nombre d'element dans le fichier
// et aussi des bugs dans le fichier, par exemple 
// le bloc Stats est marqué dans le fichier comme ayant une longueur 20 octets
// alors qu'il en fait réellement 24 !!
//

int verbose = 0;
int opt_samples = 10;

#ifndef BYTE
#define BYTE 	int_least8_t
#endif

#ifndef WORD
#define WORD	int_least16_t
#endif

#ifndef DWORD
#define DWORD	int_least32_t
#endif

// from dat file format.doc : .  I believe TDateTime is a 4 byte double (in fact a float)
#ifndef TDateTime
#define TDateTime float
#endif
// Apres analyse, il s'agit plutôt d'un double de 8 octets

//Snapshot header
#pragma pack(push,1)
typedef struct
{
  BYTE VersionNum;
  BYTE FILE_SYNC[3];
}SNAPSHOT_FILE_HEADER;
#pragma pack(pop)

//Snapshot header
#pragma pack(push,1)
typedef struct
{
  WORD     SectionID;
  BYTE     VersionNum;
  BYTE     SyncByte;
  DWORD SectionLen;
}SNAPSHOT_HEADER;
#pragma pack(pop)

// program d'index des fichiers -----
#define MAX_SAMPLES  1024
#define DATA_FILE_VERSION  1

#define    SECTION_SYNC_BYTE       0x40

#define    LOCAL_DATA_SECTION_ID   0x1111
#define    REMOTE_DATA_SECTION_ID  0x2222
#define    STATS_SECTION_ID        0x3333
#define    DESCR_SECTION_ID        0x4444

#define   FILE_SYNC_SEQ           0x414B53


#pragma pack(push,1)
typedef struct tsDatFile {
   SNAPSHOT_FILE_HEADER snapshotFileHead;
   SNAPSHOT_HEADER snapshotDataHead;
   float ROAspecta[MAX_SAMPLES];
   float RAMANspecta[MAX_SAMPLES];
   double LeftHand[2][2][2][MAX_SAMPLES];
   double RightHand[2][2][2][MAX_SAMPLES];
   float ROA_CC[MAX_SAMPLES];
   float ROAnoiseFloor[MAX_SAMPLES];
   SNAPSHOT_HEADER snapshotStatsHead;
   int dataFileVersion;
   int numberOfScans; 		// au depart long
   double elapsedTime;		// au depart float
   double TotalExposureTime;
   SNAPSHOT_HEADER snapshotDescrHead;
   char sampleName[100];
   char sampleDescription[2000];
} TDataFile;
#pragma pack(pop)

void print_header(SNAPSHOT_HEADER * pheader)
{
	char * label;

	if (verbose < 1)
		return;

	switch(pheader->SectionID){
		case LOCAL_DATA_SECTION_ID :
			label = "local-data-section";
			break;
		case REMOTE_DATA_SECTION_ID :
			label = "remote-data-section";
			break;
		case STATS_SECTION_ID :
			label = "stats-section";
			break;
		case DESCR_SECTION_ID :
			label = "description-section";
			break;
		default:
			label = "<unknown>";
	}
	printf("%s : 0x%04x : VersionNum : 0x%02x : SyncByte : 0x%02x : SectionLen :  %d 0x%04x 0%8o\n",
		       	label , pheader->SectionID,
			pheader->VersionNum,
			pheader->SyncByte,
			pheader->SectionLen, pheader->SectionLen, pheader->SectionLen);
#if 0
	printf("%s : VersionNum : 0x%02x\n", label , pheader->VersionNum);
	printf("%s : SyncByte : 0x%02x\n",   label, pheader->SyncByte);
	printf("%s : SectionLen : %d 0x%04x %8o\n", label,
			pheader->SectionLen, pheader->SectionLen, pheader->SectionLen);
#endif
}

void print_samples(TDataFile * pd, int nbsamples)
{
	int i;
	if (nbsamples >= MAX_SAMPLES){
		fprintf(stderr,"WARNING: too much samples : %d\n", nbsamples);
		nbsamples = 10;
	}
	for (i = 0; i < nbsamples; i++){
		if (i == 0){
			printf("%-15s ; %-15s ; %-15s ; %-15s\n", "roa_cc", "roanoiseFloor", "raman", "roa");
		}
		printf("%15f ; %15f ; %15.f ; %15f\n", 
			pd->ROA_CC[i], pd->ROAnoiseFloor[i],
			pd->RAMANspecta[i],
			pd->ROAspecta[i]);
	}
	printf("\n");
}

void LoadSnapShot(char * fname)
{
	FILE * fd;
	TDataFile d;
	size_t ret;
	SNAPSHOT_HEADER header;
	size_t position;
	size_t old_position;

	printf("fname: %s\n", fname);

	fd = fopen(fname, "rb");
	if (!fd){
		fprintf(stderr,"%s : %d : %s\n", fname, errno, strerror(errno));
		return;
	}
	// File Header
        ret = fread(&d.snapshotFileHead, sizeof(d.snapshotFileHead), 1, fd);
	if (verbose)
		printf("VersionNum: %d\n", d.snapshotFileHead.VersionNum);
	if (d.snapshotFileHead.VersionNum != DATA_FILE_VERSION ){
		fprintf(stderr, "WARNING: bad version number in file: '%s', bad format.\n", fname);
	}
	position = sizeof(d.snapshotFileHead);

	while( fread(&header, sizeof(header), 1, fd) == 1 ){ 
		
		position += sizeof(header);
		position += header.SectionLen; // position théorique du prochain header
		print_header(&header);
		switch(header.SectionID) {
			case LOCAL_DATA_SECTION_ID :
				memcpy(&d.snapshotDataHead, &header, sizeof(header));
				ret = fread(d.ROAspecta, sizeof(d.ROAspecta), 1, fd);
				ret = fread(d.RAMANspecta, sizeof(d.RAMANspecta), 1, fd);
				ret = fread(d.LeftHand, sizeof(d.LeftHand), 1, fd);
				ret = fread(d.RightHand, sizeof(d.RightHand), 1, fd);
				ret = fread(d.ROA_CC, sizeof(d.ROA_CC), 1, fd);
				ret = fread(d.ROAnoiseFloor, sizeof(d.ROAnoiseFloor), 1, fd);

				print_samples(&d, opt_samples);

				break;
			case STATS_SECTION_ID :
				memcpy(&d.snapshotStatsHead, &header, sizeof(header));
				ret = fread(&d.dataFileVersion, sizeof(d.dataFileVersion), 1, fd);
				ret = fread(&d.numberOfScans, sizeof(d.numberOfScans), 1, fd);
				ret = fread(&d.elapsedTime, sizeof(d.elapsedTime), 1, fd);
				ret = fread(&d.TotalExposureTime, sizeof(d.TotalExposureTime), 1, fd);
				if (verbose){
					printf("dataFileVersion = %d\n", d.dataFileVersion);
				}
				printf("numberOfScans = %d\n", d.numberOfScans);
				printf("elapsedTime = %.2lf\n", d.elapsedTime);
				printf("TotalExposureTime = %.2lf\n", d.TotalExposureTime);
				break;
			case DESCR_SECTION_ID :
				memcpy(&d.snapshotDescrHead, &header, sizeof(header));
				ret = fread(&d.sampleName, sizeof(d.sampleName), 1, fd);
				ret = fread(&d.sampleDescription, sizeof(d.sampleDescription), 1, fd);
				d.sampleName[99] = '\0';
				d.sampleDescription[1999] = '\0';
				printf("sample name: %s\n", d.sampleName);
				printf("sample Description:\n%s\n", d.sampleDescription);
				break;
			case REMOTE_DATA_SECTION_ID :
			default:
				break;
		}
		// Ici on jongle entre les bugs du fichier et ceux des structures
		// Si on n'a pas assez lu on fait confiance à la valeur de SectionLen du fichier
		// Si on a trop lu on fait confiance à la structure
		old_position = ftell(fd);
		if (old_position != position){
			if (verbose)
				fprintf(stderr, "WARNING: ecart de position old : %ld  %s new : %ld\n",
					old_position,
					old_position > position ? ">" : "<",
					position);
		}
		if (old_position > position){
			position = old_position;
		} else {
			fseek(fd, position, SEEK_SET);
		}
	}
	fclose(fd);
}

void main(int argc, char ** argv)
{
	int c;
	char * cvalue;
	int index;
#if 0
	printf("sizeof(int) = %ld\n", sizeof(int));
	printf("sizeof(long) = %ld\n", sizeof(long));
	printf("sizeof(float) = %ld\n", sizeof(float));
	printf("sizeof(double) = %ld\n", sizeof(double));
	printf("sizeof(SNAPSHOT_FILE_HEADER) = %ld\n", sizeof(SNAPSHOT_FILE_HEADER));
	printf("sizeof(SNAPSHOT_HEADER) = %ld\n", sizeof(SNAPSHOT_HEADER));
	printf("sizeof(struct tsDatFile) = %ld -- %08lo\n", sizeof(struct tsDatFile),  sizeof(struct tsDatFile));
#endif

   	double LeftHand[2][2][2][MAX_SAMPLES];
   	printf(" double LeftHand : %ld\n", sizeof(LeftHand));
   	printf(" double LeftHand[2] : %ld\n", sizeof(LeftHand[2]));
   	printf(" double LeftHand[2][2] : %ld\n", sizeof(LeftHand[2][2]));
   	printf(" double LeftHand[2][2][2] : %ld\n", sizeof(LeftHand[2][2][2]));
   	printf(" double LeftHand[2][2][2][MAX_SAMPLES] : %ld\n", sizeof(LeftHand[2][2][2][MAX_SAMPLES]));

	while((c = getopt(argc, argv, "hvz:+:")) != -1){
		switch(c){
			case 'v':
				verbose = 1;
				break;
			case 'h':
				//cvalue = optarg;
				break;
			case '+':
				opt_samples = atoi(optarg);
				break;
			case '?':
			default:
				if (optopt == 'z')
					fprintf(stderr,"Option -%c requires an argument.\n", optopt);
				else if (isprint(optopt))
					fprintf(stderr,"Unknown option '-%c'.\n", optopt);
				else
					fprintf(stderr,"Unknown character '\\x%x'.\n", optopt);
				return;

		}
	}
	for(index = optind; index < argc; index++){
		LoadSnapShot(argv[index]);
	}
}
