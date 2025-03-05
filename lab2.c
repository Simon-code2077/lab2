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
 #define CURSOR_CHAR '_'
 #define BLINK_INTERVAL 500000 // 500 milliseconds
 
 int cursor_row = 22;
 int cursor_col = 10;
 int cursor_visible = 1;
 int cursor_index = 0;    // 光标位置

 int sockfd; /* Socket file descriptor */

 struct libusb_device_handle *keyboard;
 uint8_t endpoint_address;



pthread_t network_thread;
pthread_t blink_thread;
void *network_thread_f(void *);

void *blink_cursor(void *arg) {
  while (1) {
    cursor_visible = !cursor_visible;
    usleep(BLINK_INTERVAL);
  }
  return NULL;
}


void draw_cursor(char temp) {
  if (cursor_visible) {
    fbputchar(CURSOR_CHAR, cursor_row, cursor_col);
  } else {
    fbputchar(temp, cursor_row, cursor_col);
  }
}

void update_cursor_position() {
  cursor_col = 10 + cursor_index;
  cursor_row = 22;
  if (cursor_col >= 64) {
    cursor_col = 10;
    cursor_row++;
  }
}


int main()
{
  int err, col, row;
  const char *key_value       = "    abcdefghijklmnopqrstuvwxyz1234567890\n    -=[]\\#;'`,./";
  const char *key_value_shift = "    ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%^&*()\n    _+{}|~:\"~<>?";
  struct sockaddr_in serv_addr;

  struct usb_keyboard_packet packet;
  int transferred;
  char keystate[12];
  pthread_create(&blink_thread, NULL, blink_cursor, NULL);
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
    fbputchar('*', 21, col);
  }
  fbputs("SERVER:", 20, 0);
  fbputs("YOU:", 22, 0);

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
  int location_col = 9;
  int location_row = 22;
  // MAX 100 characters
  char temp_char;
  char cursor_char;
  int cursor_index = 0;
  int cursor_col = 10;
  int cursor_row = 22;
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
      // check the first key is pressed
      if (packet.keycode[0]!=0){
        fbputchar('_', location_row, location_col+2);// ***************cursor
        // check if the key is newly pressed
        if(old_key1 != packet.keycode[0] && old_key2 != packet.keycode[0] && packet.keycode[0] != 0x28 && packet.keycode[0] != 0x29 && packet.keycode[0] != 0x2a && packet.keycode[0] <= 0x39){
          temp_char = (packet.modifiers == 0x02 || packet.modifiers == 0x20) ? key_value_shift[packet.keycode[0]] : key_value[packet.keycode[0]];
          if (len+1 < 100){
            memmove(&str[cursor_index + 1], &str[cursor_index], len - cursor_index);
            str[len] = temp_char;
            str[len+1] = '\0';
            cursor_index++;
            len ++;
          }
          if (location_col < 63) {
            location_col += 1;
          }
          else{
            location_col = 10;
            location_row += 1;
          }
          fbputchar(temp_char, location_row, location_col);
          draw_cursor();
          old_key1 = packet.keycode[0];
          old_key2 = packet.keycode[1];
        }
        // The first key is unchanged, check the second key
        else{
          if(packet.keycode[1] != 0 && packet.keycode[1] != 0x28 && packet.keycode[1] != 0x29 && packet.keycode[1] <= 0x39){
            temp_char = (packet.modifiers == 0x02 || packet.modifiers == 0x20) ? key_value_shift[packet.keycode[1]] : key_value[packet.keycode[1]];
            if (len+1 < 100){
              memmove(&str[cursor_index + 1], &str[cursor_index], len - cursor_index);
              str[len] = temp_char;
              str[len+1] = '\0';
              cursor_index++;
              len ++;
            }
            if (location_col < 63) {
              location_col += 1;
            }
            else{
              location_col = 10;
              location_row += 1;
            }
            fbputchar(temp_char, location_row, location_col);
            draw_cursor();
            old_key1 = packet.keycode[0];
            old_key2 = packet.keycode[1];
            
          }
        } //single modifier key pressed after the first key will has no response
      }
      // No key is pressed
      else{
        old_key1 = 0;
        old_key2 = 0;
      }
      // ENTER is pressed
      if (packet.keycode[0] == 0x28){
        write(sockfd, str, len);
        len = 0;
        location_row = 22;
        location_col = 9;
        for (row = location_row; row < 24; row++){
          for (col = location_col; col < 64; col++){
            fbputchar(' ', row, col);
          }
        }
      }
      // BACKSPACE is pressed
      if (packet.keycode[0] == 0x2a){
        if (len > 0){
          len --;
          str[len] = '\0';
          fbputchar(' ', location_row, location_col);
          if (location_col > 10){
            location_col -= 1;
          }
          else if (location_row > 22){
            location_col = 63;
            location_row -= 1;
          }
          printf("%d\n", len);
          printf("%s\n", str);
        }
      }

      if (packet.keycode[0] == 0x50) {
        if (cursor_index > 0) {
          cursor_index--;
          update_cursor_position();
          //draw_cursor();
        }
      }

      // Handle right arrow key
      if (packet.keycode[0] == 0x4F) {
        if (cursor_index < len) {
          cursor_index++;
          update_cursor_position();
          //draw_cursor();
        }
      }

      // ESC is pressed
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
  char displayBuff[54*21+1];      // 10 bits to show "SERVER:" Total Max 21 Lines
  char displayLine[55];      
  int i;
  for (i = 0; i < 54*21; i++) {
    displayBuff[i] = ' ';
  }
  for (i = 0; i < 54; i++) {
    displayLine[i] = ' ';
  }

  int n;
  /* Receive data */
  while ( (n = read(sockfd, &recvBuf, BUFFER_SIZE - 1)) > 0 ) {
    // recvBuf[n] = '\0';
    printf("%s\n", recvBuf);
    char *recvPtr = recvBuf;
    int count = 1;
    while (n > 0) {
      if (n > 54) {
        memcpy(displayLine, recvPtr, 54);
        displayLine[54] = '\0';
        memcpy(displayBuff, displayBuff + 54, 54*(21 - count));
        memcpy(displayBuff + 54*(21-count), displayLine, 54);
        n -= 54;
        recvPtr += 54;
	count = count + 1;
      } else {
        memcpy(displayLine, recvPtr, n);
        for (i = n; i < 54; i++) {
          displayLine[i] = ' ';
        }
        displayLine[54] = '\0';
        memcpy(displayBuff, displayBuff + 54, 54*(21 - count));
        for (i = 54*(21-count); i < 54*(21-count+1); i++) {
          displayBuff[i] = ' ';
        }
        displayBuff[54*21] = '\0';
        memcpy(displayBuff + 54*21 - 54, displayLine, n);
        n = 0;
        break;
      }
    }
    for (i = 0; i < 21; i++) {
      memcpy(displayLine, displayBuff + 54*(20-i), 54);
      displayLine[54] = '\0';
      fbputs(displayLine, i, 10);
    }
  }

  return NULL;
}

