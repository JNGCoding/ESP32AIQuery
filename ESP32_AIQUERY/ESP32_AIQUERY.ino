#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/dac.h>
#include <TTS.h>

struct pos_t {
  uint16_t x;
  uint16_t y;
};

TFT_eSPI tft = TFT_eSPI();
const char* ssid = "xxxxxxxxxxx";
const char* password = "xxxxxxxxxx";
const char* Gemini_Token = "xxxxxxxxxxxxxxxxxxx";
const char* Gemini_Max_Tokens = "250";
String str_ip;
WebServer server(80);
String Query = "";
String Answer = "";
bool enableTTS = true;
TTS T2S(25);

String POSTquery(String Query) {
  HTTPClient https;

  if (https.begin("https://generativelanguage.googleapis.com/v1beta/models/gemini-1.5-flash:generateContent?key=" + (String)Gemini_Token)) {
    https.addHeader("Content-Type", "application/json");
    String Payload = "{\"contents\": [{\"parts\":[{\"text\":" + Query + "}]}],\"generationConfig\": {\"maxOutputTokens\": " + (String)Gemini_Max_Tokens + "}}";
    int result = https.POST(Payload);

    if (result == HTTP_CODE_OK || result == HTTP_CODE_MOVED_PERMANENTLY) {
      String Response = https.getString();
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, Response);
      https.end();
      return (String)doc["candidates"][0]["content"]["parts"][0]["text"];
    } else {
      https.end();
      return "[HTTPS] GET... failed, error: " + https.errorToString(result);
    }
  } else {
    return "Failed to Connect.";
  }
}

pos_t* println(String text, uint16_t x, uint16_t y, uint16_t color, uint8_t size) {
  tft.setTextSize(size);
  tft.setTextColor(color);
  tft.setCursor(x, y);
  tft.print(text.c_str());

  pos_t* result = (pos_t*)calloc(1, sizeof(pos_t));
  result->x = (uint16_t)tft.getCursorX();
  result->y = (uint16_t)tft.getCursorY();
  return result;
}

String ASCIIfy(String input) {
  String output = "";
  for (uint16_t i = 0; i < input.length(); i++) {
    char c = input.charAt(i);
    if (c >= 0 && c <= 127) {
      output.concat(c);
    }
  }
  return output;
}


String formatQ(String Quest) {
  String result = "\"" + Quest + " - under 250 words.\"";
  result.replace("\n", "");
  return result;
}

String formatA(String input) {
  String result = "";
  String digitWords[] = {"zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine"};

  for (uint16_t i = 0; i < input.length(); i++) {
    char c = input.charAt(i);

    if (isalnum(c) || c == ' ') {  // Check if the character is alphanumeric
      if (isdigit(c)) {  // If it's a digit, convert to word
        result += " ";
        result += digitWords[c - '0'];  // Convert char digit to index
        result += " ";
      } else {
        result += c;
      }
    } else if (c == '+') {
      result += " plus ";
    } else if (c == '-') {
      result += " minus ";
    } else if (c == '*') {
      result += " asterisk ";
    } else if (c == '/') {
      result += " slash ";
    } else if (c == '=') {
      result += " is equal to ";
    } else if (c == '(' || c == '[' || c == '{' || c == '<') {
      result += " bracket start ";
    } else if (c == ')' || c == ']' || c == '}' || c == '>') {
      result += " bracket close ";
    } else if (c == '?') {
      result += " question mark ";
    }
  }

  result.trim();  // Remove trailing spaces
  return result;
}

void handleRoot() {
  String html = R"=====(
    <!DOCTYPE html>
    <html lang="en">
    <head>
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <title>AI Text Processor</title>
      <style>
        body {
          font-family: Arial, sans-serif;
          text-align: center;
          margin: 0;
          padding: 0;
          background: linear-gradient(to right, #4facfe, #00f2fe);
          color: #333;
        }
        .container {
          padding: 20px;
          margin-top: 10%;
        }
        input[type="text"] {
          width: 80%;
          padding: 10px;
          margin: 10px 0;
          border: none;
          border-radius: 5px;
        }
        button {
          padding: 10px 20px;
          background-color: #007BFF;
          color: white;
          border: none;
          border-radius: 5px;
          cursor: pointer;
        }
        button:hover {
          background-color: #0056b3;
        }
      </style>
      <script>
        async function sendData() {
          const input = document.getElementById('inputText').value;
          const button = document.getElementById('submitButton');
          button.disabled = true;

          const response = await fetch('/process', {
            method: 'POST',
            headers: {
              'Content-Type': 'application/x-www-form-urlencoded',
            },
            body: 'inputText=' + encodeURIComponent(input),
          });

          const result = await response.text();
          button.disabled = false;
          document.getElementById('inputText').value = '';
        }
      </script>
    </head>
    <body>
      <div class="container">
        <h1>ESP32 Text Processor</h1>
        <p>Enter text to send to the ESP32:</p>
        <input type="text" id="inputText" placeholder="Type your text here..." required>
        <br>
        <button id="submitButton" onclick="sendData()">Submit</button>
      </div>
    </body>
    </html>
  )=====";
  server.send(200, "text/html", html);
}

void handleProcess() {
  Query = server.arg("inputText");
  if (Query.length() == 0) { Serial.println("No input."); return; }
  if (Query.length() > 100) { Serial.println("Query Length Exceeded."); Query = Query.substring(0, 100); }

  if (Query.equals("ESP32.TTS Toggle")) {
    enableTTS = !enableTTS;
    if (enableTTS) {
      T2S.setPitch(6);
      T2S.sayText("Speech disabled");
    } else {
      T2S.setPitch(6);
      T2S.sayText("Speech enabled");
    }
    return;
  }

  tft.fillScreen(0x0000);
  pos_t* position = println("IP : " + str_ip, 0, 0, 0xFFFF, 1);
  position = println("You : " + Query, 0, position->y + 8, 0xFFFF, 1);

  Answer = POSTquery(formatQ(Query));

  if (!enableTTS) {
    println("AI : " + ASCIIfy(Answer), 0, position->y + 15, 0xFFFF, 1);
  } else {
    position = println("AI : ", 0, position->y + 15, 0xFFFF, 1);

    Answer = ASCIIfy(Answer);
    Serial.println(Answer);

    String temp = "";
    for (uint16_t i = 0; i < Answer.length(); i++) {
      char c = Answer.charAt(i);
      if (c != ' ') {
        temp.concat(c);
      }

      if ((!temp.equals("") || i == Answer.length() - 1) && c == ' ') {
        if (position->x < 315) {
          position = println(temp, position->x, position->y, 0xFFFF, 1);
          position = println(" ", position->x, position->y, 0xFFFF, 1);
        } else {
          position = println(temp, 0, position->y + 8, 0xFFFF, 1);
        }

        temp = formatA(temp);
        temp.toLowerCase();
        Serial.println(temp);
        T2S.setPitch(6);
        T2S.sayText(temp.c_str());
        temp = "";
      }
    }

    position = println(temp, position->x, position->y, 0xFFFF, 1);
    Serial.println(temp);
    temp = formatA(temp);
    temp.toLowerCase();
    T2S.setPitch(6);
    T2S.sayText(temp.c_str());
  }
  server.send(200, "text/plain", "Process Done");
  free(position);
}

void setup() {
  Serial.begin(115200);
  while (!Serial);

  tft.init();
  tft.fillScreen(0x0000);
  tft.setRotation(1);

  // Connection to Wifi.
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(200);
  }
  Serial.println("\nConnected to WiFi.");

  server.on("/", handleRoot);
  server.on("/process", HTTP_POST, handleProcess);
  server.begin();

  IPAddress ip = WiFi.localIP();
  str_ip = String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);

  println("IP : " + str_ip, 0, 0, 0xFFFF, 1);
}


void loop() {
  server.handleClient();
}