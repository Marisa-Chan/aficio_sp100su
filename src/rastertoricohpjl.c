#include <stdio.h>
#include <time.h>
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>

#include <cups/cups.h>
#include <cups/raster.h>

#include "libjbig/jbig.h"

//#define DEBUG

struct OutPutting
{
    unsigned char *buffer;
    unsigned long size;
    unsigned long maxSize;
};

int interrupt = 0;

FILE *outfd = NULL;
FILE *logfd = NULL;

unsigned char DotCount[256] = 
{ 0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4, 
  1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
  1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 
  1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 
  3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 
  1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 
  3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 
  3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 
  3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 
  4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8 };

void logg(const char *format, ...)
{
    if (logfd)
    {

        va_list lst;
        va_start(lst, format);
        vfprintf(logfd, format, lst);
        va_end(lst);
    }
}

void ToUpper(char *str)
{
    while(*str != 0)
    {
        *str = toupper(*str);
        str++;
    }
}

void StrUpCpy(char *dst, const char *src)
{
    while (1)
    {
        if (*src == 0)
        {
            *dst = 0;
            break;
        }
        *dst = toupper(*src);
        dst++;
        src++;
    }
}

unsigned long CountDots(unsigned char *data, unsigned long size)
{
    unsigned long count = 0;
    while(size > 0)
    {
        count += DotCount[*data];
        data++;
        size--;
    }
    return count;
}


void OutputHeader(const char *date, const char *username)
{
    fprintf(outfd, "\x1b%%-12345X@PJL\r\n");
    fprintf(outfd, "@PJL SET TIMESTAMP=%s\r\n", date);
    fprintf(outfd, "@PJL SET FILENAME=Document\r\n");
    fprintf(outfd, "@PJL SET COMPRESS=JBIG\r\n");
    fprintf(outfd, "@PJL SET USERNAME=%s\r\n", username);
    fprintf(outfd, "@PJL SET COVER=OFF\r\n");
    fprintf(outfd, "@PJL SET HOLD=OFF\r\n");
}

void OutputFooter()
{
    fprintf(outfd, "@PJL EOJ\r\n");
    fprintf(outfd, "\x1b%%-12345X\r\n");
}

void JbigOutCallback(unsigned char *start, size_t len, void *file)
{
    struct OutPutting *jdata = (struct OutPutting *)file;
    if (jdata->size + len > jdata->maxSize)
    {
        /* try grow by 1.5 */
        unsigned long newsize = jdata->maxSize + (jdata->maxSize >> 1);
        if (newsize < jdata->size + len)
            newsize = jdata->size + len;

        jdata->buffer = (unsigned char *)realloc(jdata->buffer, newsize);
    }
    
    memcpy(jdata->buffer + jdata->size, start, len);
    jdata->size += len;
}

void OutputPage(cups_raster_t *ras, cups_page_header2_t *header, const char *msource, const char *psize, const char *resolution)
{
    unsigned long dataSize = header->cupsHeight * header->cupsBytesPerLine;

    void *data = malloc(dataSize);
    cupsRasterReadPixels(ras, data, dataSize);
    
    struct OutPutting jdata;

    jdata.buffer = (unsigned char *)malloc(dataSize);
    jdata.maxSize = dataSize;
    jdata.size = 0;

    struct jbg_enc_state enc;
    jbg_enc_init(&enc, header->cupsWidth, header->cupsHeight, 1, (unsigned char **)&data, JbigOutCallback, &jdata);
    jbg_enc_layers(&enc, 0);
    jbg_enc_lrange(&enc, -1, -1);
    jbg_enc_options(&enc, JBG_SMID | JBG_ILEAVE, JBG_LRLTWO | JBG_TPBON, 0, 0, -1);
    jbg_enc_out(&enc);
    jbg_enc_free(&enc);

    unsigned long dots = CountDots(data, dataSize);

    free(data);

    fprintf(outfd, "@PJL SET PAGESTATUS=START\r\n");
    fprintf(outfd, "@PJL SET COPIES=1\r\n");
    fprintf(outfd, "@PJL SET MEDIASOURCE=%s\r\n", msource);
    fprintf(outfd, "@PJL SET MEDIATYPE=PLAINRECYCLE\r\n");
    fprintf(outfd, "@PJL SET PAPER=%s\r\n", psize);
    fprintf(outfd, "@PJL SET PAPERWIDTH=%d\r\n", header->cupsWidth);
    fprintf(outfd, "@PJL SET PAPERLENGTH=%d\r\n", header->cupsHeight);
    fprintf(outfd, "@PJL SET RESOLUTION=%s\r\n", resolution);
    fprintf(outfd, "@PJL SET IMAGELEN=%d\r\n", jdata.size);

    fwrite(jdata.buffer, jdata.size, 1, outfd);

    fprintf(outfd, "@PJL SET DOTCOUNT=%d\r\n", dots / 10);
    fprintf(outfd, "@PJL SET PAGESTATUS=END\r\n");

    free(jdata.buffer);
}

void CancelJob(int sig) 
{
    interrupt = 1;
    OutputFooter();
    fflush(outfd);
#ifdef DEBUG
    fclose(outfd);
    fclose(logfd);
#endif
    exit(0);
}


int ricohjbig(cups_raster_t *ras, const char *user, const char *title, int copies, const char *opts)
{
    char Slot[64];
    char Resolution[64];
    char PageSize[64];
    char datebuf[64];

    struct sigaction act = { 0 };
    act.sa_flags = SA_SIGINFO;
    act.sa_handler = &CancelJob;
    sigaction(SIGTERM, &act, NULL);

    setbuf(stderr, NULL);

    cups_option_t *options = NULL;
    int num_options = cupsParseOptions(opts, 0, &options);

    int Page = 0;

    strcpy(Slot, "AUTO");
    strcpy(Resolution, "600");
    strcpy(PageSize, "A4");

    const char *tmp = cupsGetOption("PageSize", num_options, options);
    if (tmp)
        StrUpCpy(PageSize, tmp);

    tmp = cupsGetOption("Resolution", num_options, options);
    if (tmp)
        StrUpCpy(Resolution, tmp);

    tmp = cupsGetOption("InputSlot", num_options, options);
    if (tmp)
        StrUpCpy(Slot, tmp);

    logg("Options:\n");
    for(int i = 0; i < num_options; ++i)
        logg("%s = %s\n", options[i].name, options[i].value);
    
    logg("\nUsed mediasource %s pagesize %s resolution %s\n", Slot, PageSize, Resolution);

    cups_page_header2_t header; 
    while (interrupt == 0 && cupsRasterReadHeader2(ras, &header))
    {
        const char *value = NULL;

        ++Page;

        if (Page == 1)
        {
            time_t t = time(NULL);
            struct tm *tm = localtime(&t);
            if (tm == NULL)
                strcpy(datebuf, "01.01.2000 00:00:00");
            else
                strftime(datebuf, 256, "%d.%m.%Y %H:%M:%S", tm);
            
            OutputHeader(datebuf, user);
        }

        logg("Writing page %d\n", Page);
        OutputPage(ras, &header, Slot, PageSize, Resolution);
    }

    OutputFooter();

    logg("End\n");
    return Page;
}


int main(int argc, const char *argv[], const char *envp[])
{
    int fd;

    setbuf(stderr, NULL);

    if (argc < 6 || argc > 7) 
    {
		fprintf(stderr, "ERROR rastertoricohpjl job-id user title copies options [raster_file]");
        return (1);
    }

    if (argc == 7) 
    {
        if ((fd = open(argv[6], O_RDONLY)) == -1) 
        {
            fprintf(stderr, "ERROR Unable to open raster file");
            sleep(1);
            return (1);
        }
    }
    else
        fd = 0; // stdin

#ifdef DEBUG
    char tmpfile[512];
    sprintf(tmpfile, "/tmp/rastertoricohpjl-%s-%s.raw", argv[2], argv[1]);
    outfd = fopen(tmpfile, "wb");
    sprintf(tmpfile, "/tmp/rastertoricohpjl-%s-%s.log", argv[2], argv[1]);
    logfd = fopen(tmpfile, "a");
#else
    outfd = stdout;
    logfd = NULL;
#endif

    cups_raster_t *ras = cupsRasterOpen(fd, CUPS_RASTER_READ);

    int pages = ricohjbig(ras, argv[2], argv[3], atoi(argv[4]), argv[5]);

    cupsRasterClose(ras);
	
    if (fd != 0)
        close(fd);

    
#ifdef DEBUG
    fclose(outfd);
    fclose(logfd);
#endif

    if (pages != 0) 
    {
        fprintf(stderr, "INFO Ready to print.");
        return 0;
    } 
    else 
    {
        fprintf(stderr, "ERROR No pages found!");
        return 1;
    }
}