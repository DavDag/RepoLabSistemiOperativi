#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/stat.h>

#define CONFIG_FILE "config.txt"

#define BUFFER_SIZE 1024

#if !defined(MAXFILENAME)
#define MAXFILENAME 2048
#endif

int isdot(const char *dir) {
  int l = strlen(dir);
  return ((l > 0 && dir[l - 1] == '.'));
}

void ls(const char *nomeDir) {
    DIR * dir;
    fprintf(stdout, "-----------------------\n");
    fprintf(stdout, "Directory %s:\n", nomeDir);

    // Try opening directory
    if ((dir = opendir(nomeDir)) == NULL) {
		perror("opendir");
		fprintf(stderr, "Errore aprendo la directory %s\n", nomeDir);
		return;
    }

    // Main loop
	struct dirent *file;
    struct stat statbuf;
    int dirLen = strlen(nomeDir);
    int fileNameLen = 0;
	while((errno = 0, file = readdir(dir)) != NULL) {
        char filename[MAXFILENAME];
    
        // Calc filename length
        fileNameLen = strlen(file->d_name);
        if ((dirLen + fileNameLen + 2) > MAXFILENAME) {
            fprintf(stderr, "ERRORE: MAXFILENAME troppo piccolo\n");
            exit(EXIT_FAILURE);
        }

        // Calc filename path
        strncpy(filename, nomeDir,      MAXFILENAME - 1);
        strncat(filename, "/",          MAXFILENAME - 1);
        strncat(filename, file->d_name, MAXFILENAME - 1);
        
        // Controllo il file
        if (stat(filename, &statbuf) == -1) {
            perror("eseguendo la stat");
            fprintf(stderr, "Errore nel file %s\n", filename);
            return;
        }

        // Ricorsione se è una directory
        if(S_ISDIR(statbuf.st_mode)) {
            if (!isdot(filename)) ls(filename);
        } else {
            // Altrimenti stampo un po di informazioni
            char mode[10] = {'-','-','-','-','-','-','-','-','-','\0'};
            if (S_IRUSR & statbuf.st_mode) mode[0]='r';
            if (S_IWUSR & statbuf.st_mode) mode[1]='w';
            if (S_IXUSR & statbuf.st_mode) mode[2]='x';

            if (S_IRGRP & statbuf.st_mode) mode[3]='r';
            if (S_IWGRP & statbuf.st_mode) mode[4]='w';
            if (S_IXGRP & statbuf.st_mode) mode[5]='x';

            if (S_IROTH & statbuf.st_mode) mode[6]='r';
            if (S_IWOTH & statbuf.st_mode) mode[7]='w';
            if (S_IXOTH & statbuf.st_mode) mode[8]='x';
            
            fprintf(stdout, "%20s: %10ld  %s\n", file->d_name, statbuf.st_size, mode);
        }
    }

    // Controllo eventuali errori
    if (errno != 0) perror("readdir");
    closedir(dir);
}

void readConfig(const char *nomeDir) {
    // Calc config path
    char filename[MAXFILENAME];
    strncpy(filename, nomeDir,      MAXFILENAME - 1);
    strncat(filename, "/",          MAXFILENAME - 1);
    strncat(filename, CONFIG_FILE,  MAXFILENAME - 1);

    // Open file in read_only mode
    int fIn = open(filename, O_RDONLY);
    if (fIn == -1) {
        fprintf(stderr, "Impossibile aprire il file in lettura %s\n", filename);
        return;
    }

    // Read configs
    char buffer[BUFFER_SIZE];
    size_t readed = read(fIn, buffer, BUFFER_SIZE - 1);
    if (readed == -1) {
        fprintf(stderr, "Impossibile leggere il contenuto del file %s\n", filename);
        return;
    }

    // Print configs content
    printf("[CONFIGS] >>>\n%s\n", buffer);
}

void createConfig(const char *nomeDir) {
    // Calc config path
    char filename[MAXFILENAME];
    strncpy(filename, nomeDir,      MAXFILENAME - 1);
    strncat(filename, "/",          MAXFILENAME - 1);
    strncat(filename, CONFIG_FILE,  MAXFILENAME - 1);

    // Open file in write_only mode
    int fOut = open(filename, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);
    if (fOut == -1) {
        fprintf(stderr, "Impossibile aprire il file in scrittura %s\n", filename);
        return;
    }

    // Write configs
    const char *buffer = "test for file in/out.\nThis is the configs content\n";
    size_t written = write(fOut, buffer, 51);
    if (written == -1) {
        fprintf(stderr, "Impossibile scrivere nel file %s\n", filename);
        return;
    }
}

int main(int argc, char * argv[]) {
    // Check if called correctly
    if (argc != 2) {
		fprintf(stderr, "usa: %s dir\n", argv[0]);
		return EXIT_FAILURE;
    }

    // Check if passed argument is a directory
    struct stat statbuf;
    if (stat(argv[1], &statbuf) == -1) {
        fprintf(stderr, "Unable to call stat on %s\n", argv[1]);
        exit(errno);
    }
    if(!S_ISDIR(statbuf.st_mode)) {
		fprintf(stderr, "%s non e' una directory\n", argv[1]);
		return EXIT_FAILURE;
    }

    // Read config.txt if exists
    readConfig(argv[1]);

    // Create config.txt
    createConfig(argv[1]);

    // Call "ls"
    ls(argv[1]);

    return EXIT_SUCCESS;
}
