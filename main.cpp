#include "mbed.h"
#include "EthernetInterface.h"
#include "SDFileSystem.h"
#include <stdio.h>
#include <string.h>
#include "DHT.h"
#include "cmsis_os.h"                   /* CMSIS RTOS definitions             */
#include "rl_net.h"                     /* Network definitions                */
#include "board.h"                      /* BSP definitions                    */
 
/*----------------------------------------------------------------------------
  Main Thread 'main': Run Network
 *---------------------------------------------------------------------------*/

#define HTTPD_SERVER_PORT   80
#define HTTPD_MAX_REQ_LENGTH   1023
#define HTTPD_MAX_HDR_LENGTH   255
#define HTTPD_MAX_FNAME_LENGTH   127
#define HTTPD_MAX_DNAME_LENGTH   127

Serial uart(USBTX, USBRX);

//SDFileSystem sd(p5, p6, p7, p8, "sd"); // LPC1768 MBD2PMD
//SDFileSystem sd(P0_18, P0_17, P0_15, P0_16, "sd"); // Seeeduino Arch Pro SPI2SD
SDFileSystem sd(PTE3, PTE1, PTE2, PTE4, "sd"); // K64F

EthernetInterface eth;
TCPSocketServer server;
TCPSocketConnection client;

char buffer[HTTPD_MAX_REQ_LENGTH+1];
char httpHeader[HTTPD_MAX_HDR_LENGTH+1];
char fileName[HTTPD_MAX_FNAME_LENGTH+1];
char dirName[HTTPD_MAX_DNAME_LENGTH+1];
char *uristr;
char *eou;
char *qrystr;

FILE *fp;
int rdCnt;

void get_file(char* uri)
{     
    uart.printf("get_file %s\n", uri);
    char *lstchr = strrchr(uri, NULL) -1;
    if ('/' == *lstchr) {
        uart.printf("Open directory /sd%s\n", uri);
        *lstchr = 0;
        sprintf(fileName, "/sd%s", uri);
        DIR *d = opendir(fileName);
        if (d != NULL) {
            sprintf(httpHeader,"HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: Close\r\n\r\n");
            client.send(httpHeader,strlen(httpHeader));
            sprintf(httpHeader,"<html><head><title>Directory Listing </title></head><body><h1>%s Directory Listing</h1><ul>", uri);
            printf("Temperature :", temp()); // show temperture degree 
            client.send(httpHeader,strlen(httpHeader));
            struct dirent *p;
            while((p = readdir(d)) != NULL) {
                sprintf(dirName, "%s/%s", fileName, p->d_name);
                uart.printf("%s\n", dirName);
                DIR *subDir = opendir(dirName);
                if (subDir != NULL) {
                    sprintf(httpHeader,"<li><a href=\"./%s/\">%s/</a></li>", p->d_name, p->d_name);
                } else {
                    sprintf(httpHeader,"<li><a href=\"./%s\">%s</a></li>", p->d_name, p->d_name);
                }
                client.send(httpHeader,strlen(httpHeader));
            }
        }
        closedir(d);
        uart.printf("Directory closed\n");
        sprintf(httpHeader,"</ul></body></html>");
        client.send(httpHeader,strlen(httpHeader));
    } else {
        sprintf(fileName, "/sd%s", uri);
        fp = fopen(fileName, "r");
        if (fp == NULL) {
            uart.printf("File not found\n");
            sprintf(httpHeader,"HTTP/1.1 404 Not Found \r\nContent-Type: text\r\nConnection: Close\r\n\r\n");
            client.send(httpHeader,strlen(httpHeader));
            client.send(uri,strlen(uri));
        } else {
            uart.printf("Sending: header");
            sprintf(httpHeader,"HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: Close\r\n\r\n");
            client.send(httpHeader,strlen(httpHeader));
            uart.printf(" file");
            while ((rdCnt = fread(buffer, sizeof( char ), 1024, fp)) == 1024) {
                client.send(buffer, rdCnt);
                uart.printf(".");
            }
            client.send(buffer, rdCnt);
            fclose(fp);
            uart.printf("done\n");
        }
    }
}

int main (void)
{
       hardware_init();
       net_initialize     ();
//    Serial Interface eth;
    uart.baud(115200);
    uart.printf("Initializing\n");

//    Check File System
    uart.printf("Checking File System\n");
    DIR *d = opendir("/sd/");
    if (d != NULL) {
        uart.printf("SD Card Present\n");
    } else {
        uart.printf("SD Card Root Directory Not Found\n");
    }

//    EthernetInterface eth;
    uart.printf("Initializing Ethernet\n");
    eth.init(); //Use DHCP
    uart.printf("Connecting\n");
    eth.connect();
    uart.printf("IP Address is %s\n", eth.getIPAddress());

//    TCPSocketServer server;
    server.bind(HTTPD_SERVER_PORT);
    server.listen();
    uart.printf("Server Listening\n");

    while (true) {
         net_main ();
         osThreadYield ();

        uart.printf("\nWait for new connection...\r\n");
        server.accept(client);
        client.set_blocking(false, 1500); // Timeout after (1.5)s

        uart.printf("Connection from: %s\r\n", client.get_address());
        while (true) {
            int n = client.receive(buffer, sizeof(buffer));
            if (n <= 0) break;
            uart.printf("Recieved Data: %d\r\n\r\n%.*s\r\n",n,n,buffer);
            if (n >= 1024) {
                sprintf(httpHeader,"HTTP/1.1 413 Request Entity Too Large \r\nContent-Type: text\r\nConnection: Close\r\n\r\n");
                client.send(httpHeader,strlen(httpHeader));
                client.send(buffer,n);
                break;
            } else {
                buffer[n]=0;
            }
            if (!strncmp(buffer, "GET ", 4)) {
                uristr = buffer + 4;
                eou = strstr(uristr, " ");
                if (eou == NULL) {
                    sprintf(httpHeader,"HTTP/1.1 400 Bad Request \r\nContent-Type: text\r\nConnection: Close\r\n\r\n");
                    client.send(httpHeader,strlen(httpHeader));
                    client.send(buffer,n);
                } else {
                    *eou = 0;
                    get_file(uristr);
                }
            }
        }

        client.close();
    }
}


// Read Temperature 
void Temp(){

DHT sensor(A0, DHT11);

    int error = 0;
    float h = 0.0f, c = 0.0f, f = 0.0f, k = 0.0f, dp = 0.0f, dpf = 0.0f;

    while(1) {
        wait(2.0f);
        error = sensor.readData();
        if (0 == error) {
            c   = sensor.ReadTemperature(CELCIUS);
            f   = sensor.ReadTemperature(FARENHEIT);
            k   = sensor.ReadTemperature(KELVIN);
            h   = sensor.ReadHumidity();
            dp  = sensor.CalcdewPoint(c, h);
            dpf = sensor.CalcdewPointFast(c, h);
            printf("Temperature in Kelvin: %4.2f, Celcius: %4.2f, Farenheit %4.2f\n", k, c, f);
            printf("Humidity is %4.2f, Dewpoint: %4.2f, Dewpoint fast: %4.2f\n", h, dp, dpf);
        } else {
            printf("Error: %d\n", error);
        }
    
}

}
