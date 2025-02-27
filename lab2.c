/*
 *
 * CSEE 4840 Lab 2 for 2019
 *
 * Name/UNI: Please Changeto Yourname (pcy2301)
 */
#include "fbputchar.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "usbkeyboard.h"
#include <pthread.h>

/* Update SERVER_HOST to be the IP address of
 * the chat server you are connecting to
 */
/* arthur.cs.columbia.edu */
#define SERVER_HOST "128.59.19.114"
#define SERVER_PORT 42000

#define BUFFER_SIZE 128

/*
 * References:
 *
 * https://web.archive.org/web/20130307100215/http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html
 *
 * http://www.thegeekstuff.com/2011/12/c-socket-programming/
 * 
 */

int sockfd; /* Socket file descriptor */

struct libusb_device_handle *keyboard;
uint8_t endpoint_address;

pthread_t network_thread;
void *network_thread_f(void *);

int main()
{
  int err, col, row;
  const char *key_value = "    abcdefghijklmnopqrstuvwxyz1234567890                                                                                                         ";
  struct sockaddr_in serv_addr;

  struct usb_keyboard_packet packet;
  int transferred;
  char keystate[12];

  if ((err = fbopen()) != 0) {
    fprintf(stderr, "Error: Could not open framebuffer: %d\n", err);
    exit(1);
  }

  /* Draw rows of asterisks across the top and bottom of the screen */
  for (col = 0 ; col < 64 ; col++) {
    for (row = 0; row < 24; row ++){
      fbputchar(' ', row, col);
    }
  }
  for (col = 0; col < 64 ; col ++){
    fbputchar('*', 12, col);
  }
  fbputs("SERVER:", 4, 0);
  fbputs("KEYBOARD:", 15, 0);

  /* Open the keyboard */
  if ( (keyboard = openkeyboard(&endpoint_address)) == NULL ) {
    fprintf(stderr, "Did not find a keyboard\n");
    exit(1);
  }
    
  /* Create a TCP communications socket */
  if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
    fprintf(stderr, "Error: Could not create socket\n");
    exit(1);
  }

  /* Get the server address */
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(SERVER_PORT);
  if ( inet_pton(AF_INET, SERVER_HOST, &serv_addr.sin_addr) <= 0) {
    fprintf(stderr, "Error: Could not convert host IP \"%s\"\n", SERVER_HOST);
    exit(1);
  }

  /* Connect the socket to the server */
  if ( connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    fprintf(stderr, "Error: connect() failed.  Is the server running?\n");
    exit(1);
  }

  /* Start the network thread */
  pthread_create(&network_thread, NULL, network_thread_f, NULL);
  int location_col = 12;
  int location_row = 16;
  char temp_char;
  int old_key1,old_key2;
  int len;
  char str[100] = "";
  len = 0;
  /* Look for and handle keypresses */
  for (;;) {
    libusb_interrupt_transfer(keyboard, endpoint_address,
			      (unsigned char *) &packet, sizeof(packet),
			      &transferred, 0);
    if (transferred == sizeof(packet)) {
      sprintf(keystate, "%02x %02x %02x", packet.modifiers, packet.keycode[0],
	      packet.keycode[1]);
      printf("%s\n", keystate);
      fbputs(keystate, 15, 12);
      if (packet.keycode[0]!=0){
	if(old_key1 != packet.keycode[0] && old_key2 != packet.keycode[0]){
	  temp_char = key_value[packet.keycode[0]];
	  if (len+1 < 100){
	    str[len] = temp_char;
	    str[len+1] = '\0';
	    len++;
	  }
          if (location_col < 63) {
            location_col += 1;
          }   
          else{
            location_col = 12;
            location_row += 1;
          }
	  fbputchar(temp_char, location_row, location_col);
	  old_key1 = packet.keycode[0];
	  old_key2 = packet.keycode[1];
	}
	else{
	  if(packet.keycode[1] != 0){
	    temp_char = key_value[packet.keycode[1]];
	    if (len+1 < 100){
	      str[len] = temp_char;
	      str[len+1] = '\0';
	      len ++;
	    }
	    if (location_col < 63) {
	      location_col += 1;
	    }
	    else{
	      location_col = 12;
	      location_row += 1;
	    }
	  }
	  fbputchar(temp_char, location_row, location_col);
	  old_key1 = packet.keycode[0];
	  old_key2 = packet.keycode[1];
	}
      }
      else{
	old_key1 = 0;
	old_key2 = 0;
      }
      if (packet.keycode[0] == 0x28){
	write(sockfd, str, len+1);
	len = 0;
	location_row = 16;
	location_col = 12;
	for (row = location_row; row < 24; row++){
	  for (col = location_col; col < 64; col++){
	    fbputchar(' ', row,col);
	  }
	}
      }
      if (packet.keycode[0] == 0x29) { /* ESC pressed? */
	break;
      }
    }
  }

  /* Teirminate the network thread */
  pthread_cancel(network_thread);

  /* Wait for the network thread to finish */
  pthread_join(network_thread, NULL);

  return 0;
}

void *network_thread_f(void *ignored)
{
  char recvBuf[BUFFER_SIZE];
  int n;
  /* Receive data */
  while ( (n = read(sockfd, &recvBuf, BUFFER_SIZE - 1)) > 0 ) {
    recvBuf[n] = '\0';
    printf("%s", recvBuf);
    fbputs(recvBuf, 4, 12);
  }

  return NULL;
}

