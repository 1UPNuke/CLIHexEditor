#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdbool.h>

#define BYTES_PER_ROW 16
#define MAX_STR_LEN 2048

//Define ANSI escape sequences
#define RESET_COLOR "\x1b[0m"
#define ACCENT_COLOR "\x1b[90m"
#define SUCCESS_COLOR "\x1b[92m"
#define HIGHLIGHT_COLOR "\x1b[30;46m"
#define INFO_COLOR "\x1b[36m"

FILE* openfile(char* path, char* mode);
uint32_t swapendian(uint32_t n);
void printheader();
void printrow(uint32_t offset, uint8_t* row, uint8_t size, uint32_t diffoffset, uint32_t diffsize);
void seekoffset(FILE* fp, uint32_t offset);
bool inoffsetrange(uint32_t x, uint32_t start, uint8_t size);
void printfile(FILE* fp, uint32_t offset, int32_t rows);
void printdiff(FILE* fp, uint32_t offset, uint8_t size, uint8_t* bytes);
void getinput(char* msg, char* format, void* output);
void read(char* path);
void write(char* path);
void save(char* path);
void load(char* path);
void terminal(char* path);
void log2file(FILE* log, FILE* file);



uint32_t swapendian(uint32_t n) {
    return (
        ((n >> 24) & 0xff) |     // move byte 3 to byte 0
        ((n << 8) & 0xff0000) |  // byte 1 to byte 2
        ((n >> 8) & 0xff00) |    // byte 2 to byte 1
        ((n << 24) & 0xff000000) // byte 0 to byte 3    
    );
}



int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Missing file path!\n");
        return 1;
    }
    if (argc > 2) {
        fprintf(stderr, "Too many arguments!\n");
        return 1;
    }

    //Try opening file before asking what to do with it
    FILE* fp = openfile(argv[1], "r");
    fclose(fp);
    //Enter the interactive terminal
    terminal(argv[1]);

    return 0;
}

void terminal(char* path) {
    while (1) {
        //Get operation from user
        char op[MAX_STR_LEN];
        printf("\nSpecify operation (r)ead / (w)rite / (s)ave / (l)oad / (e)xit: ");
        fgets(op, sizeof(op), stdin);
        switch (tolower(*op)) {
            case 'r': read(path); break;
            case 'w': write(path); break;
            case 's': save(path); break;
            case 'l': load(path); break;
            case 'e': exit(0); return;
        }
    }
}




FILE* openfile(char* path, char* mode) {
    FILE* fp = fopen(path, mode);
    //If we failed to read the file print an error and exit
    if (fp == NULL || fp == 0) {
        fprintf(stderr, "Error: %s\n", strerror(errno));
        exit(1);
    }

    return fp;
}

void seekoffset(FILE* fp, uint32_t offset) {
    //Seek to the specified offset, work around offset being signed
    if (offset > LONG_MAX) {
        fseek(fp, LONG_MAX, SEEK_SET);
        fseek(fp, offset - LONG_MAX, SEEK_CUR);
    }
    else {
        fseek(fp, offset, SEEK_SET);
    }
}

bool inoffsetrange(uint32_t x, uint32_t start, uint8_t size) {
    return (x >= start && x < start + size);
}



void printheader() {
    printf(ACCENT_COLOR "  OFFSET  ");

    //Print digits along the header depending on the row width
    for (uint8_t i = 0; i < BYTES_PER_ROW; i++) {
        printf("%02" SCNx8 " ", i);
    }

    printf("\tDECODED TEXT \n" RESET_COLOR);
}

void printrow(uint32_t offset, uint8_t* row, uint8_t size, uint32_t diffoffset, uint32_t diffsize) {
    //Print the current offset of the file
    printf(ACCENT_COLOR " %08" SCNx32 " " RESET_COLOR, offset);

    //Loop through each byte of the row and print it as hex
    for (uint8_t i = 0; i < BYTES_PER_ROW; i++) {
        printf(RESET_COLOR);
        //Highlight changed bytes if any
        if (inoffsetrange(offset + i, diffoffset, diffsize) && diffsize) printf(HIGHLIGHT_COLOR);

        if (i < size) printf("%02X ", row[i]);
        //Padding to prepare align decoded text
        else printf("   ");
    }
    printf("\t");

    //Loop through each byte of the row
    for (uint8_t i = 0; i < BYTES_PER_ROW; i++) {
        printf(RESET_COLOR);
        //Highlight changed bytes if any
        if (inoffsetrange(offset + i, diffoffset, diffsize) && diffsize) printf(HIGHLIGHT_COLOR);

        if (i >= size) break;
        //Print "." if character is not printable
        if (!isprint(row[i])) printf(". ");
        //Print the character
        else printf("%c ", row[i]);
    }
    printf(RESET_COLOR);
    printf("\n");
}

void printfile(FILE* fp, uint32_t offset, int32_t rows) {
    //Define a buffer to store the row currently being read
    uint8_t row[BYTES_PER_ROW] = { 0 };
    uint8_t i = 0;

    //Print the first header row
    printheader();

    seekoffset(fp, offset);

    while (!feof(fp)) {

        //If we reached the end of the row
        if (i % BYTES_PER_ROW == 0 && i != 0) {
            //Check if we reached the correct amount of rows already
            if (rows != 0) {
                rows--;
                if (rows == 0) {
                    i++;
                    offset++;
                    break;
                }
            }
            //Round the offset and print the row
            printrow(offset - i, row, i, 0, 0);
            i = 0;
        }

        //Store a character in the buffer
        row[i] = fgetc(fp);

        if (row[i] == EOF) break;

        offset++; i++;
    }
    //Print the last row that was left over
    printrow(offset - i, row, i - 1, 0, 0);
}

void getinput(char* msg, char* format, void* output) {
    char buffer[MAX_STR_LEN];
    //Keep asking for input until the format is correct
    do {
        printf("%s", msg);
        fgets(buffer, sizeof(buffer), stdin);
    } while (sscanf(buffer, format, output) == 0);
}



void read(char* path) {
    //Use 32-bit unsigned integer to support files up to 4.2GB
    uint32_t offset = 0;
    getinput("\nOffset in bytes to start reading from as hex (Enter: 0): ", "%8" SCNx32, &offset);

    //Ask the user for the number of rows to print
    int32_t rows = 0;
    getinput("Number of rows to read (Enter: 0 to read until EOF): ", "%" SCNi32, &rows);

    //Print
    printf("\n");
    FILE* fp = openfile(path, "rb");
    printfile(fp, offset, rows);
    fclose(fp);
}




void printdiff(FILE* fp, uint32_t offset, uint8_t size, uint8_t* bytes) {
    //Define a buffer to store the row currently being read
    uint8_t row[BYTES_PER_ROW] = { 0 };
    uint8_t i = 0;

    printf("\nPreview changes:\n");
    //Print the first header row
    printheader();

    //Seek to the beginning of the line before the offset
    uint32_t diffoffset = offset;
    offset = offset / BYTES_PER_ROW * BYTES_PER_ROW;
    seekoffset(fp, offset);

    bool iseof = false;

    while (1) {

        //If we reached the end of the row
        if (i % BYTES_PER_ROW == 0 && i != 0) {
            //Round the offset and print the row
            printrow(offset - i, row, i, diffoffset, size);
            i = 0;

            //Check if we are past the diff
            if (offset > diffoffset + size - 1) {
                return;
            }
        }

        //Store a character in the buffer, depending on the offset
        if (inoffsetrange(offset, diffoffset, size)) {
            //If in the diff range, use a byte from the diff
            row[i] = bytes[offset - diffoffset];
            fgetc(fp);
        }
        else {
            //Else get it from the file
            row[i] = fgetc(fp);
        }

        //If at the end of the file and past the diff range we can break
        if (feof(fp) && (offset > diffoffset + size - 1)) break;

        offset++; i++;
    }
    //Print the last row that was left over
    printrow(offset - i, row, i, diffoffset, size);
}

void write(char* path) {
    //Use 32-bit unsigned integer to support files up to 4.2GB
    uint32_t offset = 0;
    getinput("\nOffset in bytes to start writing to as hex (Enter: 0): ", "%" SCNx32, &offset);

    char buffer[MAX_STR_LEN];
    //Read bytes from user into string
    uint8_t bytes[UINT8_MAX] = { 0 };
    do {
        printf("Bytes to write as a string of hex digits: ");
        fgets(buffer, sizeof(buffer), stdin);
    } while (sscanf(buffer, "%" SCNx8, bytes) == 0);

    //Write bytes to array and find the size
    uint8_t size = 0;
    for (size = 0; size < UINT8_MAX; size++) {
        if (sscanf(&buffer[size * 2], " %02" SCNx8, &bytes[size]) <= 0) break;
    }
    printf(INFO_COLOR "Parsed %d bytes\n" RESET_COLOR, size);

    if (size == 0) return;

    //Print preview of changes
    FILE* fp = openfile(path, "rb");
    printdiff(fp, offset, size, bytes);
    fclose(fp);

    //Open logfile for appending, create one if it doesn't exist
    char logpath[MAX_STR_LEN];
    sprintf(logpath, "%s.log", path);
    FILE* logfp = openfile(logpath, "ab");

    int n = 1;
    //Check if system is little endian, if so, swap it
    if (*(char*)&n == 1) {
        offset = swapendian(offset);
    }

    //Write 32-bit offset in big endian, followed by 8-bit size, and then size bytes of data
    fwrite(&offset, sizeof(offset), 1, logfp);
    fwrite(&size, sizeof(size), 1, logfp);
    fwrite(bytes, sizeof(*bytes), size, logfp);

    fclose(logfp);

    printf(SUCCESS_COLOR "Appended changelog to \"%s\", enter (s)ave to commit changes\n" RESET_COLOR, logpath);
}



void save(char* path) {
    //Open logfile for reading
    char logpath[MAX_STR_LEN];
    sprintf(logpath, "%s.log", path);
    FILE* logfp = openfile(logpath, "rb");

    //Open file for writing without deleting contents
    FILE* fp = openfile(path, "rb+");

    //Apply the log
    log2file(logfp, fp);

    fclose(fp);
    fclose(logfp);

    printf(SUCCESS_COLOR "Saved changes from \"%s\" to \"%s\"\n" RESET_COLOR, logpath, path);
}

void load(char* path) {
    //Get file from user
    char logpath[MAX_STR_LEN];
    printf("\nPath to the logfile to be loaded: ");
    fgets(logpath, sizeof(logpath), stdin);

    logpath[strlen(logpath) - 1] = 0;

    //Open logfile for reading
    FILE* logfp = openfile(logpath, "rb");

    //Open file for writing without deleting contents
    FILE* fp = openfile(path, "rb+");

    //Apply the log
    log2file(logfp, fp);

    fclose(fp);

    printf(SUCCESS_COLOR "Loaded changes from \"%s\" to \"%s\"\n" RESET_COLOR, logpath, path);
}




void log2file(FILE* logfp, FILE* fp) {
    //Parse the file
    while (!feof(logfp)) {
        uint32_t offset = 0;
        uint8_t size = 0;
        uint8_t bytes[UINT8_MAX] = { 0 };

        //Read 32-bit offset in big endian, followed by 8-bit size, and then size bytes of data
        fread(&offset, sizeof(offset), 1, logfp);
        fread(&size, sizeof(size), 1, logfp);
        fread(bytes, sizeof(*bytes), size, logfp);

        int n = 1;
        //Check if system is little endian, if so, swap it
        if (*(char*)&n == 1) {
            offset = swapendian(offset);
        }

        //Apply log to file
        seekoffset(fp, offset);
        fwrite(bytes, sizeof(*bytes), size, fp);
    }
}