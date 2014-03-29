/*
  Linino Yun SPI-to-Serial

 Allows you to use the Yun's 32U4 processor as a
 serial terminal for the linino processor through SPI interface

 Upload this to an Arduino Yun via serial (not WiFi)
 then open the serial monitor at 115200 to see the boot process
 of the linino processor. You can also use the serial monitor
 as a basic command line interface for the linino processor using
 this sketch.

  The circuit:
 * Arduino Yun

 created March 2014
 by mimmoLinino
 
 */
 
#include <StreamSPI.h>


void setup() {
  Serial.begin(115200);      // open serial connection via USB-Serial
  StreamSPI0.begin(); // open serial connection to Linino
}

void readCharFrom(Stream &in, Stream &out, bool echo)
{
  char c = (char)in.read();     // read from USB-serial
  out.write(c);             //    otherwise write char to Linino
  if (echo) in.write(c);                  // write to USB-serial
  if (c == '\r') {
    out.write('\n'); // add the newline char when carriage return char is received
    if (echo) in.write('\n');  // add the newline char when carriage return char is received
  }
}

void loop() {
  // copy from virtual serial line to uart and vice versa
  if (Serial.available()) {           // got anything from USB-Serial?
    readCharFrom(Serial, StreamSPI0, true);
  }
  if (StreamSPI0.available()) {          // got anything from Linino?
    readCharFrom(StreamSPI0, Serial, true);
   }
}
