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
int sockfd; /* Socket file descriptor */

struct libusb_device_handle *keyboard;
uint8_t endpoint_address;

pthread_t network_thread;
pthread_t blink_thread;
void *network_thread_f(void *);
void *blink_cursor(void *arg) {
  while (1) {
      cursor_visible = !cursor_visible;         // 翻转光标可见状态
      char underChar = (cursor_index < input_len) ? 
                        input_buffer[cursor_index] : ' ';
      draw_cursor(underChar);                   // 重新绘制光标（显示或隐藏）
      usleep(BLINK_INTERVAL);
  }
}


void draw_cursor(char temp) {
  if (cursor_visible) {
    fbputchar(CURSOR_CHAR, cursor_row, cursor_col);
  } else {
    fbputchar(temp, cursor_row, cursor_col);
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
        // check if the key is newly pressed
        if(old_key1 != packet.keycode[0] && old_key2 != packet.keycode[0] && packet.keycode[0] != 0x28 && packet.keycode[0] != 0x29 && packet.keycode[0] != 0x2a && packet.keycode[0] <= 0x39){
          temp_char = (packet.modifiers == 0x02 || packet.modifiers == 0x20) ? key_value_shift[packet.keycode[0]] : key_value[packet.keycode[0]];
          if (len+1 < 100){
            str[len] = temp_char;
            str[len+1] = '\0';
            len++;
          }
          if (location_col < 63) {
            location_col += 1;
          }   
          else{
            location_col = 10;
            location_row += 1;
          }
          fbputchar(temp_char, location_row, location_col);
          old_key1 = packet.keycode[0];
          old_key2 = packet.keycode[1];
        }
        // The first key is unchanged, check the second key
        else{
          if(packet.keycode[1] != 0 && packet.keycode[1] != 0x28 && packet.keycode[1] != 0x29 && packet.keycode[1] <= 0x39){
            temp_char = (packet.modifiers == 0x02 || packet.modifiers == 0x20) ? key_value_shift[packet.keycode[1]] : key_value[packet.keycode[1]];
            if (len+1 < 100){
              str[len] = temp_char;
              str[len+1] = '\0';
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
      if (packet.keycode[0] == 0x50) {  // Left Arrow
        if (cursor_index > 0) {
            if (cursor_visible) {  /* 恢复旧位置字符 */
                char ch = (cursor_index < input_len) ? 
                          input_buffer[cursor_index] : ' ';
                fbputchar(ch, cursor_row, cursor_col);
            }
            cursor_index--;        // 光标左移
            /* 计算新光标坐标 */
            cursor_row = 22; cursor_col = 10;
            for (int j = 0; j < cursor_index; ++j) {
                if (cursor_col == 63) { cursor_col = 10; cursor_row++; }
                else { cursor_col++; }
            }
            cursor_visible = 1;    // 移动后使光标可见
            fbputchar(CURSOR_CHAR, cursor_row, cursor_col);
        }
    }
    if (packet.keycode[0] == 0x4F) {  // Right Arrow
        if (cursor_index < input_len) {
            if (cursor_visible) {
                char ch = (cursor_index < input_len) ? 
                          input_buffer[cursor_index] : ' ';
                fbputchar(ch, cursor_row, cursor_col);
            }
            cursor_index++;        // 光标右移
            /* 同上重新计算 cursor_row, cursor_col … */
            cursor_visible = 1;
            fbputchar(CURSOR_CHAR, cursor_row, cursor_col);
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
      memcpy(displayLine, displayBuff + 54*(20-i), 54);#include <stdio.h>
      #include <stdlib.h>
      #include <string.h>
      #include <sys/socket.h>
      #include <arpa/inet.h>
      #include <unistd.h>
      #include <pthread.h>
      #include "fbputchar.h"
      #include "usbkeyboard.h"
      
      #define SERVER_HOST "128.59.19.114"
      #define SERVER_PORT 42000
      #define BUFFER_SIZE 128
      #define MAX_INPUT_LEN 100
      #define BLINK_INTERVAL 500000
      
      int sockfd;
      int cursor_row = 22;
      int cursor_col = 10;
      int cursor_visible = 1;
      char cursor_char = '_';
      char input_buffer[MAX_INPUT_LEN + 1];
      int input_len = 0;
      int cursor_index = 0;
      
      struct libusb_device_handle *keyboard;
      uint8_t endpoint_address;
      pthread_t network_thread;
      pthread_t blink_thread;
      
      void *network_thread_f(void *);
      void *blink_cursor(void *);
      void draw_cursor(char underChar);
      
      int main() {
          int err, col;
          struct sockaddr_in serv_addr;
          struct usb_keyboard_packet packet;
          int transferred;
          char keystate[12];
          const char *key_value =
              "    abcdefghijklmnopqrstuvwxyz1234567890\n    -=[]\\#;'`,./";
          const char *key_value_shift =
              "    ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%^&*()\n    _+{}|~:\"~<>?";
          int old_key1 = 0, old_key2 = 0;
      
          if ((err = fbopen()) != 0) {
              fprintf(stderr, "Error: Could not open framebuffer: %d\n", err);
              exit(1);
          }
          for (int row = 0; row < 24; row++) {
              for (col = 0; col < 64; col++) {
                  fbputchar(' ', row, col);
              }
          }
          for (col = 0; col < 64; col++) {
              fbputchar('*', 21, col);
          }
          fbputs("SERVER:", 20, 0);
          fbputs("YOU:", 22, 0);
      
          if ((keyboard = openkeyboard(&endpoint_address)) == NULL) {
              fprintf(stderr, "Did not find a keyboard\n");
              exit(1);
          }
          if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
              fprintf(stderr, "Error: Could not create socket\n");
              exit(1);
          }
          memset(&serv_addr, 0, sizeof(serv_addr));
          serv_addr.sin_family = AF_INET;
          serv_addr.sin_port = htons(SERVER_PORT);
          if (inet_pton(AF_INET, SERVER_HOST, &serv_addr.sin_addr) <= 0) {
              fprintf(stderr, "Error: Could not convert host IP \"%s\"\n", SERVER_HOST);
              exit(1);
          }
          if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
              fprintf(stderr, "Error: connect() failed. Is the server running?\n");
              exit(1);
          }
          pthread_create(&network_thread, NULL, network_thread_f, NULL);
          pthread_create(&blink_thread, NULL, blink_cursor, NULL);
      
          for (;;) {
              libusb_interrupt_transfer(keyboard, endpoint_address,
                                        (unsigned char *)&packet, sizeof(packet),
                                        &transferred, 0);
              if (transferred == sizeof(packet)) {
                  sprintf(keystate, "%02x %02x %02x",
                          packet.modifiers, packet.keycode[0], packet.keycode[1]);
      
                  if (packet.keycode[0] == 0x29) {
                      break;
                  }
                  if (packet.keycode[0] == 0x50) {
                      if (old_key1 != 0x50) {
                          if (cursor_index > 0) {
                              char under = (cursor_index < input_len)
                                               ? input_buffer[cursor_index]
                                               : ' ';
                              fbputchar(under, cursor_row, cursor_col);
                              cursor_index--;
                              int r = 22, c = 10;
                              for (int i = 0; i < cursor_index; i++) {
                                  fbputchar(input_buffer[i], r, c);
                                  if (c == 63) { c = 10; r++; } else { c++; }
                              }
                              cursor_row = r;
                              cursor_col = c;
                              char newUnder = (cursor_index < input_len)
                                                  ? input_buffer[cursor_index]
                                                  : ' ';
                              draw_cursor(newUnder);
                          }
                          old_key1 = 0x50;
                      }
                  } else if (old_key1 == 0x50) {
                      old_key1 = 0;
                  }
                  if (packet.keycode[0] == 0x4F) {
                      if (old_key1 != 0x4F) {
                          if (cursor_index < input_len) {
                              char under = (cursor_index < input_len)
                                               ? input_buffer[cursor_index]
                                               : ' ';
                              fbputchar(under, cursor_row, cursor_col);
                              cursor_index++;
                              int r = 22, c = 10;
                              for (int i = 0; i < cursor_index; i++) {
                                  fbputchar(input_buffer[i], r, c);
                                  if (c == 63) { c = 10; r++; } else { c++; }
                              }
                              cursor_row = r;
                              cursor_col = c;
                              char newUnder = (cursor_index < input_len)
                                                  ? input_buffer[cursor_index]
                                                  : ' ';
                              draw_cursor(newUnder);
                          }
                          old_key1 = 0x4F;
                      }
                  } else if (old_key1 == 0x4F) {
                      old_key1 = 0;
                  }
                  if (packet.keycode[0] == 0x28) {
                      if (input_len > 0) {
                          write(sockfd, input_buffer, input_len);
                      }
                      input_len = 0;
                      cursor_index = 0;
                      input_buffer[0] = '\0';
                      for (int r = 22; r < 24; r++) {
                          for (int c = 10; c < 64; c++) {
                              fbputchar(' ', r, c);
                          }
                      }
                      cursor_row = 22;
                      cursor_col = 10;
                      draw_cursor(' ');
                      old_key1 = 0x28;
                  } else if (old_key1 == 0x28) {
                      old_key1 = 0;
                  }
                  if (packet.keycode[0] == 0x2A) {
                      if (old_key1 != 0x2A) {
                          if (cursor_index > 0) {
                              cursor_index--;
                              for (int j = cursor_index; j < input_len - 1; j++) {
                                  input_buffer[j] = input_buffer[j + 1];
                              }
                              input_len--;
                              input_buffer[input_len] = '\0';
                              for (int r = 22; r < 24; r++) {
                                  for (int c = 10; c < 64; c++) {
                                      fbputchar(' ', r, c);
                                  }
                              }
                              int r = 22, c = 10;
                              for (int j = 0; j < input_len; j++) {
                                  fbputchar(input_buffer[j], r, c);
                                  if (c == 63) { c = 10; r++; } else { c++; }
                              }
                              r = 22; c = 10;
                              for (int i = 0; i < cursor_index; i++) {
                                  if (c == 63) { c = 10; r++; } else { c++; }
                              }
                              cursor_row = r;
                              cursor_col = c;
                              char ch = (cursor_index < input_len)
                                            ? input_buffer[cursor_index]
                                            : ' ';
                              draw_cursor(ch);
                          }
                          old_key1 = 0x2A;
                      }
                  } else if (old_key1 == 0x2A) {
                      old_key1 = 0;
                  }
                  if (packet.keycode[0] >= 0x04 && packet.keycode[0] <= 0x39) {
                      if (old_key1 != packet.keycode[0]) {
                          int shift = (packet.modifiers & (USB_LSHIFT | USB_RSHIFT));
                          char newch;
                          if (shift) {
                              newch = key_value_shift[packet.keycode[0]];
                          } else {
                              newch = key_value[packet.keycode[0]];
                          }
                          if (newch != '\n') {
                              if (input_len < MAX_INPUT_LEN) {
                                  for (int j = input_len; j > cursor_index; j--) {
                                      input_buffer[j] = input_buffer[j - 1];
                                  }
                                  input_buffer[cursor_index] = newch;
                                  input_len++;
                                  input_buffer[input_len] = '\0';
                                  cursor_index++;
                                  for (int r = 22; r < 24; r++) {
                                      for (int c = 10; c < 64; c++) {
                                          fbputchar(' ', r, c);
                                      }
                                  }
                                  int r = 22, c = 10;
                                  for (int j = 0; j < input_len; j++) {
                                      fbputchar(input_buffer[j], r, c);
                                      if (c == 63) { c = 10; r++; } else { c++; }
                                  }
                                  r = 22; c = 10;
                                  for (int j = 0; j < cursor_index; j++) {
                                      if (c == 63) { c = 10; r++; } else { c++; }
                                  }
                                  cursor_row = r;
                                  cursor_col = c;
                                  char under = (cursor_index < input_len)
                                                   ? input_buffer[cursor_index]
                                                   : ' ';
                                  draw_cursor(under);
                              }
                          }
                          old_key1 = packet.keycode[0];
                      }
                  }
              }
          }
          pthread_cancel(network_thread);
          pthread_join(network_thread, NULL);
          pthread_cancel(blink_thread);
          pthread_join(blink_thread, NULL);
          close(sockfd);
          return 0;
      }
      
      void *network_thread_f(void *ignored) {
          char recvBuf[BUFFER_SIZE];
          char displayBuff[54 * 21];
          memset(displayBuff, ' ', sizeof(displayBuff));
          while (1) {
              int n = read(sockfd, &recvBuf, BUFFER_SIZE - 1);
              if (n <= 0) break;
              int offset = 0;
              while (offset < n) {
                  int lineLen = (n - offset) > 54 ? 54 : (n - offset);
                  memmove(displayBuff, displayBuff + 54, 54 * 20);
                  memset(displayBuff + 54 * 20, ' ', 54);
                  memcpy(displayBuff + 54 * 20, &recvBuf[offset], lineLen);
                  offset += lineLen;
                  for (int row = 0; row < 21; row++) {
                      char line[55];
                      memcpy(line, displayBuff + 54 * (20 - row), 54);
                      line[54] = '\0';
                      fbputs(line, row, 10);
                  }
              }
          }
          return NULL;
      }
      
      void *blink_cursor(void *arg) {
          while (1) {
              char underChar = ' ';
              if (cursor_index < input_len) {
                  underChar = input_buffer[cursor_index];
              }
              cursor_visible = !cursor_visible;
              draw_cursor(underChar);
              usleep(BLINK_INTERVAL);
          }
          return NULL;
      }
      
      void draw_cursor(char underChar) {
          if (cursor_visible) {
              fbputchar(cursor_char, cursor_row, cursor_col);
          } else {
              fbputchar(underChar, cursor_row, cursor_col);
          }
      }
      
      displayLine[54] = '\0';
      fbputs(displayLine, i, 10);
    }
  }

  return NULL;
}

