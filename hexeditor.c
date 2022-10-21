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

void getinput(char* msg, char* format, void* output);
bool inoffsetrange(uint32_t x, uint32_t start, size_t size);
void load(char* path);
void logtofile(FILE* logfp, FILE* mainfp);
FILE* openfile(char* path, char* mode);
void printdiff(FILE* fp, uint32_t offset, uint8_t size, uint8_t* bytes);
void printfile(FILE* fp, uint32_t offset, uint32_t rows);
void printheader();
void printhelp(char* path);
void printrow(uint32_t offset, uint8_t* row, size_t size, uint32_t diffoffset, uint8_t difflen);
void read(char* path);
void save(char* path);
void seekoffset(FILE* fp, uint32_t offset);
uint32_t swapendian(uint32_t n);
void terminal(char* path);
void write(char* path);

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Missing file path!\n");
        printhelp(argv[0]);
        return 1;
    }
    if (argc > 2) {
        fprintf(stderr, "Too many arguments!\n");
        printhelp(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        printhelp(argv[0]);
        return 0;
    }

    //Try opening file before asking what to do with it
    FILE* fp = openfile(argv[1], "r");
    fclose(fp);
    //Enter the interactive terminal
    terminal(argv[1]);

    return 0;
}

void printhelp(char* path) {
    printf("Usage: %s [FILE]\n   or: %s [OPTION]\n", path, path);
    printf("Opens a FILE in a hex editor\n");
    printf("\n\t-h, --help\tdisplay this help and exit\n");
    printf("\nExit status:\n 0 if OK\n 1 if error\n");
}

FILE* openfile(char* path, char* mode) {
    FILE* fp = fopen(path, mode);
    //If we failed to read the file print an error and exit
    if (fp == NULL || fp == 0) {
        perror("Error");
        exit(1);
    }

    return fp;
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

void read(char* path) {
    //Use 32-bit unsigned integer to support files up to 4.2GB
    uint32_t offset = 0;
    getinput("\nOffset in bytes to start reading from as hex (Enter: 0): ", "%8" SCNx32, &offset);

    //Ask the user for the number of rows to print
    uint32_t rows = 0;
    getinput("Number of rows to read (Enter: 0 to read until EOF): ", "%" SCNi32, &rows);

    //Print
    printf("\n");
    FILE* fp = openfile(path, "rb");
    printfile(fp, offset, rows);
    fclose(fp);
}

void getinput(char* msg, char* format, void* output) {
    char buffer[MAX_STR_LEN];
    //Keep asking for input until the format is correct
    do {
        printf("%s", msg);
        fgets(buffer, sizeof(buffer), stdin);
    } while (sscanf(buffer, format, output) == 0);
}

void printfile(FILE* fp, uint32_t offset, uint32_t rows) {
    //Print the first header row
    printheader();

    //Seek to the correct offset to start reading
    seekoffset(fp, offset);

    //Define a buffer to store the row currently being read
    uint8_t row[BYTES_PER_ROW] = { 0 };
    size_t size = 0;

    //Read rows into buffer
    while ((size = fread(row, 1, BYTES_PER_ROW, fp)) > 0)
    {
        //Print the row
        printrow(offset, row, size, 0, 0);

        //Check if correct amount of rows has been read
        if (rows != 0) {
            rows--;
            if (rows == 0) { break; }
        }

        offset += BYTES_PER_ROW;
    }
}

void printheader() {
    printf(ACCENT_COLOR "  OFFSET  ");

    //Print digits along the header depending on the row width
    for (uint8_t i = 0; i < BYTES_PER_ROW; i++) {
        printf("%02" SCNx8 " ", i);
    }

    printf("\tDECODED TEXT \n" RESET_COLOR);
}

void seekoffset(FILE* fp, uint32_t offset) {
    //Seek to the specified offset, work around offset being signed
    if (offset > (uint32_t) LONG_MAX) {
        fseek(fp, LONG_MAX, SEEK_SET);
        fseek(fp, offset - LONG_MAX, SEEK_CUR);
    }
    else {
        fseek(fp, offset, SEEK_SET);
    }
}

void printrow(uint32_t offset, uint8_t* row, size_t size, uint32_t diffoffset, uint8_t difflen) {
    //Print the current offset of the file
    printf(ACCENT_COLOR " %08" SCNx32 " " RESET_COLOR, offset);
    bool indiff = false;

    //Loop through each byte of the row and print it as hex
    for (uint8_t i = 0; i < BYTES_PER_ROW; i++) {
        //Highlight changed bytes if any
        indiff = (inoffsetrange(offset + i, diffoffset, difflen) && difflen);
        if (indiff) { printf(HIGHLIGHT_COLOR); }

        //Print the hex
        if (i < size) { printf("%02X ", row[i]); }
        //Padding to align decoded text
        else { printf("   "); }

        if (indiff) { printf(RESET_COLOR); }
    }
    printf("\t");

    //Loop through each byte of the row
    for (uint8_t i = 0; i < BYTES_PER_ROW; i++) {
        //Highlight changed bytes if any
        indiff = (inoffsetrange(offset + i, diffoffset, difflen) && difflen);
        if (indiff) { printf(HIGHLIGHT_COLOR); }

        //If we already printed enough then stop
        if (i >= size) { break; }

        //Print "." if character is not printable
        if (!isprint(row[i])) { printf(". "); }
        //Print the character
        else { printf("%c ", row[i]); }

        if (indiff) { printf(RESET_COLOR); }
    }
    //Reset the color in case we break before row length
    if (indiff) { printf(RESET_COLOR); }
    printf("\n");
}

bool inoffsetrange(uint32_t x, uint32_t start, size_t size) {
    return (x >= start && x < start + size);
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
        if (sscanf(&buffer[size * 2], " %02" SCNx8, &bytes[size]) <= 0) { break; }
    }
    printf(INFO_COLOR "Parsed %d bytes\n" RESET_COLOR, size);

    if (size == 0) { return; }

    //Print preview of changes
    FILE* mainfp = openfile(path, "rb");
    printdiff(mainfp, offset, size, bytes);
    fclose(mainfp);

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

void printdiff(FILE* fp, uint32_t offset, uint8_t difflen, uint8_t* bytes) {
    printf("\nPreview changes:\n");
    //Print the first header row
    printheader();

    //Seek to the beginning of the line before the offset
    uint32_t diffoffset = offset;
    offset = offset / BYTES_PER_ROW * BYTES_PER_ROW;
    seekoffset(fp, offset);

    //Define a buffer to store the row currently being read
    uint8_t row[BYTES_PER_ROW] = { 0 };
    size_t size = 0;

    //Read rows into buffer
    while ((size = fread(row, 1, BYTES_PER_ROW, fp)) > 0)
    {
        //Check if we are past the diff
        if (offset > diffoffset + difflen - 1) { return; }

        //Loop through the characters in the row
        for (size_t i = 0; i < size; i++) {
            //If we are in the diff
            if (inoffsetrange(offset, diffoffset, difflen)) {
                //Replace the character
                row[i] = bytes[offset - diffoffset];
            }
            offset++;
        }

        //Print the row
        printrow(offset - BYTES_PER_ROW, row, size, diffoffset, difflen);
    }
}

uint32_t swapendian(uint32_t n) {
    return (
        ((n >> 24) & 0xff) |     // move byte 3 to byte 0
        ((n << 8) & 0xff0000) |  // byte 1 to byte 2
        ((n >> 8) & 0xff00) |    // byte 2 to byte 1
        ((n << 24) & 0xff000000) // byte 0 to byte 3    
        );
}

void save(char* path) {
    //Open logfile for reading
    char logpath[MAX_STR_LEN];
    sprintf(logpath, "%s.log", path);
    FILE* logfp = openfile(logpath, "rb");

    //Open file for writing without deleting contents
    FILE* mainfp = openfile(path, "rb+");

    //Apply the log
    logtofile(logfp, mainfp);

    fclose(mainfp);
    fclose(logfp);

    printf(SUCCESS_COLOR "Saved changes from \"%s\" to \"%s\"\n" RESET_COLOR, logpath, path);
}

void logtofile(FILE* logfp, FILE* mainfp) {
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
        seekoffset(mainfp, offset);
        fwrite(bytes, sizeof(*bytes), size, mainfp);
    }
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
    FILE* mainfp = openfile(path, "rb+");

    //Apply the log
    logtofile(logfp, mainfp);

    fclose(mainfp);

    printf(SUCCESS_COLOR "Loaded changes from \"%s\" to \"%s\"\n" RESET_COLOR, logpath, path);
}
