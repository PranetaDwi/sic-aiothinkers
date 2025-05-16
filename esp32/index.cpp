#include <Arduino.h>
#include <WiFi.h>
#include <FS.h>
#include <SD.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include <SoftwareSerial.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// Pins
const int SD_CS = 5;
const int AUDIO_PIN = 34;
const int LED_PIN = 22;
const int GSM_TX = 4;  // GSM TXD to D4
const int GSM_RX = 2;  // GSM RXD to D2

// Configuration for audio recording
const int SAMPLE_RATE = 8000;
const int BIT_DEPTH = 16;
const int SHORT_RECORD_DURATION = 2; // Duration for detecting "tolong"
const int LONG_RECORD_DURATION = 30; // 30 seconds for long recording

// Sound detection threshold
const int SOUND_THRESHOLD = 1000; // Adjust based on testing
const int DETECTION_WINDOW = 100; // Number of samples for sound detection

// WiFi connection
String SSID = "UGM Insecure";
String PASSWORD = "123456789";

// GSM configuration
SoftwareSerial gsmSerial(GSM_TX, GSM_RX); // TX, RX
String APN = "internet"; // Adjust based on your SIM provider
String GPRS_USER = "";
String GPRS_PASS = "";

// Gemini API key
String API_KEY = "AIzaSyBIMsnI-ZIH1uLUdLYhk0bDUESSSoKWUp0";

// GPS setup
HardwareSerial GPSSerial(1);
TinyGPSPlus gps;
String device_id = "ESP32_001"; // Device ID
const char* serverName = "http://192.168.27.182:5000/post-location"; // Server for location
const char* audioServer = "192.168.27.150"; // Server for audio upload

// Variables for controlling location sending
bool sendLocation = false;
unsigned long lastLocationSend = 0;
const unsigned long LOCATION_INTERVAL = 5000; // Location send interval (5 seconds)

// Network status
bool useWiFi = true;

void setupWiFi() {
  WiFi.begin(SSID.c_str(), PASSWORD.c_str());
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 10) {
    delay(1000);
    Serial.print(".");
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi connected, IP address: ");
    Serial.println(WiFi.localIP());
    useWiFi = true;
  } else {
    Serial.println("WiFi connection failed, switching to GSM");
    useWiFi = false;
    setupGSM();
  }
}

void setupGSM() {
  gsmSerial.begin(9600);
  Serial.println("Initializing GSM module...");
  if (!sendATCommand("AT", "OK", 2000)) {
    Serial.println("GSM module not responding");
    return;
  }
  sendATCommand("AT+CPIN?", "READY", 2000); // Check SIM
  sendATCommand("AT+CREG?", "+CREG: 0,1", 2000); // Check network registration
  sendATCommand("AT+CGATT=1", "OK", 2000); // Attach to GPRS
  sendATCommand("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"", "OK", 2000);
  sendATCommand("AT+SAPBR=3,1,\"APN\",\"" + APN + "\"", "OK", 2000);
  if (GPRS_USER != "") {
    sendATCommand("AT+SAPBR=3,1,\"USER\",\"" + GPRS_USER + "\"", "OK", 2000);
  }
  if (GPRS_PASS != "") {
    sendATCommand("AT+SAPBR=3,1,\"PWD\",\"" + GPRS_PASS + "\"", "OK", 2000);
  }
  sendATCommand("AT+SAPBR=1,1", "OK", 2000); // Enable GPRS
  sendATCommand("AT+SAPBR=2,1", "OK", 2000); // Query GPRS status
  Serial.println("GSM module initialized");
}

bool sendATCommand(String command, String expectedResponse, int timeout) {
  gsmSerial.println(command);
  String response = "";
  unsigned long startTime = millis();
  while (millis() - startTime < timeout) {
    while (gsmSerial.available()) {
      char c = gsmSerial.read();
      response += c;
    }
    if (response.indexOf(expectedResponse) != -1) {
      return true;
    }
  }
  Serial.print("AT command failed: ");
  Serial.println(response);
  return false;
}

void recordAudio(int duration, const char* filename) {
  if (!SD.begin(SD_CS, SPI, 1000000)) {
    Serial.println("SD card initialization failed!");
    while (1);
  }
  Serial.println("SD card initialized!");

  if (SD.exists(filename)) {
    if (SD.remove(filename)) {
      Serial.println("Previous audio file deleted.");
    } else {
      Serial.println("Failed to delete previous audio file.");
      return;
    }
  } else {
    Serial.println("No previous audio file detected, starting new");
  }

  File audioFile = SD.open(filename, FILE_WRITE);
  if (!audioFile) {
    Serial.println("Failed to create audio file.");
    return;
  }

  Serial.println("Start recording");
  writeWavHeader(audioFile, SAMPLE_RATE, BIT_DEPTH, 1, duration);

  int numSamples = SAMPLE_RATE * duration;
  unsigned long startTime = micros();
  unsigned long sampleInterval = 1000000 / SAMPLE_RATE;

  for (int i = 0; i < numSamples; i++) {
    unsigned long targetTime = startTime + (i * sampleInterval);
    while (micros() < targetTime);
    int rawValue = analogRead(AUDIO_PIN);
    int16_t sample = map(rawValue, 0, 4095, -32768, 32767);
    audioFile.write((uint8_t*)&sample, 2);
  }

  audioFile.close();
  Serial.print("Audio recorded to ");
  Serial.println(filename);
}

void writeWavHeader(File& file, int sampleRate, int bitDepth, int channels, int duration) {
  uint32_t byteRate = sampleRate * channels * bitDepth / 8;
  uint16_t blockAlign = channels * bitDepth / 8;
  uint32_t dataSize = sampleRate * duration * blockAlign;

  file.write((const uint8_t*)"RIFF", 4);
  uint32_t fileSize = 36 + dataSize;
  file.write((uint8_t*)&fileSize, 4);
  file.write((const uint8_t*)"WAVE", 4);

  file.write((const uint8_t*)"fmt ", 4);
  uint32_t subchunk1Size = 16;
  file.write((uint8_t*)&subchunk1Size, 4);
  uint16_t audioFormat = 1;
  file.write((uint8_t*)&audioFormat, 2);
  file.write((uint8_t*)&channels, 2);
  file.write((uint8_t*)&sampleRate, 4);
  file.write((uint8_t*)&byteRate, 4);
  file.write((uint8_t*)&blockAlign, 2);
  file.write((uint8_t*)&bitDepth, 2);

  file.write((const uint8_t*)"data", 4);
  file.write((uint8_t*)&dataSize, 4);
}

String base64Encode(const uint8_t* data, size_t length) {
  const char* b64_alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  String encodedString = "";
  uint32_t i = 0;
  uint8_t b1, b2, b3;

  while (i < length) {
    b1 = data[i++];
    encodedString += b64_alphabet[b1 >> 2];
    if (i < length) {
      b2 = data[i++];
      encodedString += b64_alphabet[((b1 & 0x03) << 4) | (b2 >> 4)];
    } else {
      encodedString += b64_alphabet[(b1 & 0x03) << 4];
      encodedString += "==";
      break;
    }
    if (i < length) {
      b3 = data[i++];
      encodedString += b64_alphabet[((b2 & 0x0F) << 2) | (b3 >> 6)];
      encodedString += b64_alphabet[b3 & 0x3F];
    } else {
      encodedString += b64_alphabet[(b2 & 0x0F) << 2];
      encodedString += '=';
      break;
    }
  }
  return encodedString;
}

void saveAudioString(const char* inputFile, const char* outputFile) {
  File audioFile = SD.open(inputFile, FILE_READ);
  if (!audioFile) {
    Serial.println("Failed to open audio file for reading");
    return;
  }

  size_t fileSize = audioFile.size();
  uint8_t* audioData = (uint8_t*)malloc(fileSize);
  if (audioData == NULL) {
    Serial.println("Failed to allocate memory for audio data");
    audioFile.close();
    return;
  }
  audioFile.read(audioData, fileSize);
  audioFile.close();

  String base64AudioData = base64Encode(audioData, fileSize);
  free(audioData);

  File stringFile = SD.open(outputFile, FILE_WRITE);
  if (!stringFile) {
    Serial.println("Failed to open string file for writing");
    return;
  }
  stringFile.print(base64AudioData);
  stringFile.close();

  Serial.print("Audio base64 string saved to ");
  Serial.println(outputFile);
}

void createAudioJsonRequest(const char* inputFile, const char* outputFile) {
  if (SD.exists(outputFile)) {
    if (SD.remove(outputFile)) {
      Serial.println("Previous request file deleted.");
    } else {
      Serial.println("Failed to delete previous request file.");
      return;
    }
  }

  File stringFile = SD.open(inputFile, FILE_READ);
  if (!stringFile) {
    Serial.println("Failed to open string file for reading");
    return;
  }

  String base64EncodedData = stringFile.readString();
  stringFile.close();

  const size_t jsonBufferSize = 1024 * 48;
  DynamicJsonDocument doc(jsonBufferSize);
  JsonArray contents = doc.createNestedArray("contents");
  JsonObject content = contents.createNestedObject();
  JsonArray parts = content.createNestedArray("parts");

  JsonObject textPart = parts.createNestedObject();
  textPart["text"] = "Provide a transcript of this audio clip. Only include words said in the audio.";

  JsonObject audioPart = parts.createNestedObject();
  JsonObject inlineData = audioPart.createNestedObject("inline_data");
  inlineData["mime_type"] = "audio/x-wav";
  inlineData["data"] = base64EncodedData;

  File jsonFile = SD.open(outputFile, FILE_WRITE);
  if (!jsonFile) {
    Serial.println("Failed to open JSON file for writing");
    return;
  }

  serializeJson(doc, jsonFile);
  jsonFile.close();

  Serial.print("JSON request saved to ");
  Serial.println(outputFile);
}

String transcribeAudioForTrigger() {
  if (useWiFi && WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;

    String url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.0-flash:generateContent?key=" + API_KEY;
    if (http.begin(client, url)) {
      http.addHeader("Content-Type", "application/json");

      File file = SD.open("/request-tmp.json", FILE_READ);
      if (!file) {
        Serial.println("Failed to open file for reading from SD card");
        http.end();
        return "";
      }

      const int BUFFER_SIZE = 64;
      uint8_t fileBuffer[BUFFER_SIZE];
      const int JSON_STRING_SIZE = 65536;
      char* jsonString = (char*)malloc(JSON_STRING_SIZE);
      if (jsonString == NULL) {
        Serial.println("Failed to allocate memory for JSON string");
        file.close();
        http.end();
        return "";
      }
      int jsonStringIndex = 0;

      while (file.available()) {
        int bytesRead = file.read(fileBuffer, BUFFER_SIZE);
        for (int i = 0; i < bytesRead && jsonStringIndex < JSON_STRING_SIZE - 1; i++) {
          jsonString[jsonStringIndex++] = fileBuffer[i];
        }
      }
      jsonString[jsonStringIndex] = '\0';

      file.close();

      int httpCode = http.POST(jsonString);
      free(jsonString);
      Serial.print(F("Http code: "));
      Serial.println(httpCode);

      String responseText = "";
      if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, payload);
        responseText = doc["candidates"][0]["content"]["parts"][0]["text"].as<String>();
        Serial.print("Trigger Response: ");
        Serial.println(responseText);
      }
      http.end();
      return responseText;
    }
  } else {
    // GSM transcription
    return transcribeAudioGSM("/request-tmp.json");
  }
  return "";
}

void transcribeAudio() {
  if (useWiFi && WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;

    String url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.0-flash:generateContent?key=" + API_KEY;
    if (http.begin(client, url)) {
      http.addHeader("Content-Type", "application/json");

      File file = SD.open("/request-tmp.json", FILE_READ);
      if (!file) {
        Serial.println("Failed to open file for reading from SD card");
        http.end();
        return;
      }

      const int BUFFER_SIZE = 64;
      uint8_t fileBuffer[BUFFER_SIZE];
      const int JSON_STRING_SIZE = 65536;
      char* jsonString = (char*)malloc(JSON_STRING_SIZE);
      if (jsonString == NULL) {
        Serial.println("Failed to allocate memory for JSON string");
        file.close();
        http.end();
        return;
      }
      int jsonStringIndex = 0;

      while (file.available()) {
        int bytesRead = file.read(fileBuffer, BUFFER_SIZE);
        for (int i = 0; i < bytesRead && jsonStringIndex < JSON_STRING_SIZE - 1; i++) {
          jsonString[jsonStringIndex++] = fileBuffer[i];
        }
      }
      jsonString[jsonStringIndex] = '\0';

      file.close();
      SD.end();

      int httpCode = http.POST(jsonString);
      free(jsonString);
      Serial.print(F("Http code: "));
      Serial.println(httpCode);

      if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, payload);
        String responseText = doc["candidates"][0]["content"]["parts"][0]["text"];
        Serial.print("Response: ");
        Serial.println(responseText);
      }
      http.end();
    }
  } else {
    // GSM transcription
    transcribeAudioGSM("/request-tmp.json");
  }
}

String transcribeAudioGSM(const char* jsonFilePath) {
  File file = SD.open(jsonFilePath, FILE_READ);
  if (!file) {
    Serial.println("Failed to open JSON file for GSM transcription");
    return "";
  }

  String jsonData = file.readString();
  file.close();

  // Initialize HTTP
  sendATCommand("AT+HTTPINIT", "OK", 2000);
  sendATCommand("AT+HTTPPARA=\"CID\",1", "OK", 2000);
  sendATCommand("AT+HTTPPARA=\"URL\",\"https://generativelanguage.googleapis.com/v1beta/models/gemini-2.0-flash:generateContent?key=" + API_KEY + "\"", "OK", 2000);
  sendATCommand("AT+HTTPPARA=\"CONTENT\",\"application/json\"", "OK", 2000);

  // Send POST data
  sendATCommand("AT+HTTPDATA=" + String(jsonData.length()) + ",10000", "DOWNLOAD", 5000);
  gsmSerial.println(jsonData);
  delay(1000);

  // Perform HTTP POST
  if (sendATCommand("AT+HTTPACTION=1", "OK", 10000)) {
    String response = "";
    sendATCommand("AT+HTTPREAD", "+HTTPREAD:", 5000);
    unsigned long startTime = millis();
    while (millis() - startTime < 5000) {
      while (gsmSerial.available()) {
        response += (char)gsmSerial.read();
      }
    }
    sendATCommand("AT+HTTPTERM", "OK", 2000);

    // Parse response
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, response);
    String responseText = doc["candidates"][0]["content"]["parts"][0]["text"].as<String>();
    Serial.print("GSM Trigger Response: ");
    Serial.println(responseText);
    return responseText;
  } else {
    Serial.println("GSM HTTP POST failed");
    sendATCommand("AT+HTTPTERM", "OK", 2000);
  }
  return "";
}

bool detectSound() {
  int maxAmplitude = 0;
  for (int i = 0; i < DETECTION_WINDOW; i++) {
    int rawValue = analogRead(AUDIO_PIN);
    int amplitude = abs(rawValue - 2048);
    if (amplitude > maxAmplitude) {
      maxAmplitude = amplitude;
    }
    delayMicroseconds(1000000 / SAMPLE_RATE);
  }
  Serial.print("Max Amplitude: ");
  Serial.println(maxAmplitude);
  return maxAmplitude > SOUND_THRESHOLD;
}

void sendAudioMultipart(String filepath, String device_id, String lon) {
  if (useWiFi && WiFi.status() == WL_CONNECTED) {
    File audioFile = SD.open(filepath, FILE_READ);
    if (!audioFile) {
      Serial.println("Gagal membuka file audio.");
      return;
    }

    WiFiClient client;
    String boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";

    if (!client.connect(audioServer, 5000)) {
      Serial.println("Gagal terhubung ke server.");
      audioFile.close();
      return;
    }

    String head = "--" + boundary + "\r\n";
    head += "Content-Disposition: form-data; name=\"device_id\"\r\n\r\n";
    head += device_id + "\r\n";

    head += "--" + boundary + "\r\n";
    head += "Content-Disposition: form-data; name=\"lon\"\r\n\r\n";
    head += lon + "\r\n";

    head += "--" + boundary + "\r\n";
    head += "Content-Disposition: form-data; name=\"file\"; filename=\"long_recording.wav\"\r\n";
    head += "Content-Type: audio/wav\r\n\r\n";

    String tail = "\r\n--" + boundary + "--\r\n";
    size_t contentLength = head.length() + audioFile.size() + tail.length();

    client.print("POST /upload HTTP/1.1\r\n");
    client.print("Host: " + String(audioServer) + "\r\n");
    client.print("Content-Type: multipart/form-data; boundary=" + boundary + "\r\n");
    client.print("Content-Length: " + String(contentLength) + "\r\n");
    client.print("Connection: close\r\n\r\n");

    client.print(head);

    const size_t bufferSize = 2048;
    uint8_t buffer[bufferSize];
    while (audioFile.available()) {
      size_t bytesRead = audioFile.read(buffer, bufferSize);
      client.write(buffer, bytesRead);
      yield();
    }

    client.print(tail);

    while (client.connected()) {
      String line = client.readStringUntil('\n');
      if (line == "\r") break;
    }

    String response = client.readString();
    Serial.println("Respons dari server:");
    Serial.println(response);

    client.stop();
    audioFile.close();
  } else {
    // GSM audio upload
    sendAudioMultipartGSM(filepath, device_id, lon);
  }
}

void sendAudioMultipartGSM(String filepath, String device_id, String lon) {
  File audioFile = SD.open(filepath, FILE_READ);
  if (!audioFile) {
    Serial.println("Gagal membuka file audio untuk GSM.");
    return;
  }

  String boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
  String head = "--" + boundary + "\r\n";
  head += "Content-Disposition: form-data; name=\"device_id\"\r\n\r\n";
  head += device_id + "\r\n";
  head += "--" + boundary + "\r\n";
  head += "Content-Disposition: form-data; name=\"lon\"\r\n\r\n";
  head += lon + "\r\n";
  head += "--" + boundary + "\r\n";
  head += "Content-Disposition: form-data; name=\"file\"; filename=\"long_recording.wav\"\r\n";
  head += "Content-Type: audio/wav\r\n\r\n";

  String tail = "\r\n--" + boundary + "--\r\n";
  size_t contentLength = head.length() + audioFile.size() + tail.length();

  // Initialize HTTP
  sendATCommand("AT+HTTPINIT", "OK", 2000);
  sendATCommand("AT+HTTPPARA=\"CID\",1", "OK", 2000);
  sendATCommand("AT+HTTPPARA=\"URL\",\"http://" + String(audioServer) + ":5000/upload\"", "OK", 2000);
  sendATCommand("AT+HTTPPARA=\"CONTENT\",\"multipart/form-data; boundary=" + boundary + "\"", "OK", 2000);

  // Send POST data
  sendATCommand("AT+HTTPDATA=" + String(contentLength) + ",30000", "DOWNLOAD", 10000);
  gsmSerial.print(head);

  const size_t bufferSize = 1024;
  uint8_t buffer[bufferSize];
  while (audioFile.available()) {
    size_t bytesRead = audioFile.read(buffer, bufferSize);
    gsmSerial.write(buffer, bytesRead);
    delay(10);
  }
  gsmSerial.print(tail);
  audioFile.close();

  // Perform HTTP POST
  if (sendATCommand("AT+HTTPACTION=1", "OK", 30000)) {
    String response = "";
    sendATCommand("AT+HTTPREAD", "+HTTPREAD:", 10000);
    unsigned long startTime = millis();
    while (millis() - startTime < 10000) {
      while (gsmSerial.available()) {
        response += (char)gsmSerial.read();
      }
    }
    Serial.println("GSM Server Response: ");
    Serial.println(response);
  } else {
    Serial.println("GSM HTTP POST failed");
  }
  sendATCommand("AT+HTTPTERM", "OK", 2000);
}

void sendLocationData() {
  if (useWiFi && WiFi.status() == WL_CONNECTED) {
    if (gps.location.isValid()) {
      float lat = gps.location.lat();
      float lon = gps.location.lng();

      Serial.print("Lat: ");
      Serial.print(lat, 6);
      Serial.print(" | Lon: ");
      Serial.println(lon, 6);

      HTTPClient http;
      http.begin(serverName);
      http.addHeader("Content-Type", "application/x-www-form-urlencoded");

      String postData = "device_id=" + device_id + "&lat=" + String(lat, 6) + "&lon=" + String(lon, 6);
      int httpResponseCode = http.POST(postData);

      if (httpResponseCode > 0) {
        String response = http.getString();
        Serial.println("Server response:");
        Serial.println(response);
      } else {
        Serial.print("Error saat POST: ");
        Serial.println(httpResponseCode);
      }

      http.end();
    } else {
      Serial.println("Lokasi GPS tidak valid.");
    }
  } else {
    // GSM location data
    sendLocationDataGSM();
  }
}

void sendLocationDataGSM() {
  if (gps.location.isValid()) {
    float lat = gps.location.lat();
    float lon = gps.location.lng();

    Serial.print("Lat: ");
    Serial.print(lat, 6);
    Serial.print(" | Lon: ");
    Serial.println(lon, 6);

    String postData = "device_id=" + device_id + "&lat=" + String(lat, 6) + "&lon=" + String(lon, 6);

    // Initialize HTTP
    sendATCommand("AT+HTTPINIT", "OK", 2000);
    sendATCommand("AT+HTTPPARA=\"CID\",1", "OK", 2000);
    sendATCommand("AT+HTTPPARA=\"URL\",\"" + String(serverName) + "\"", "OK", 2000);
    sendATCommand("AT+HTTPPARA=\"CONTENT\",\"application/x-www-form-urlencoded\"", "OK", 2000);

    // Send POST data
    sendATCommand("AT+HTTPDATA=" + String(postData.length()) + ",10000", "DOWNLOAD", 5000);
    gsmSerial.println(postData);
    delay(1000);

    // Perform HTTP POST
    if (sendATCommand("AT+HTTPACTION=1", "OK", 10000)) {
      String response = "";
      sendATCommand("AT+HTTPREAD", "+HTTPREAD:", 5000);
      unsigned long startTime = millis();
      while (millis() - startTime < 5000) {
        while (gsmSerial.available()) {
          response += (char)gsmSerial.read();
        }
      }
      Serial.println("GSM Server Response: ");
      Serial.println(response);
    } else {
      Serial.println("GSM HTTP POST failed");
    }
    sendATCommand("AT+HTTPTERM", "OK", 2000);
  } else {
    Serial.println("Lokasi GPS tidak valid.");
  }
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  WRITE_PERI_REG(RTC_CNTL_WDTCONFIG0_REG, 0);

  pinMode(LED_PIN, OUTPUT);
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  while (!Serial);

  setupWiFi();

  GPSSerial.begin(9600, SERIAL_8N1, 16, 17);

  if (!SD.begin(SD_CS, SPI, 1000000)) {
    Serial.println("SD card initialization failed!");
    while (1);
  }
  Serial.println("SD card initialized!");
}

void loop() {
  // Check WiFi status and reconnect if necessary
  if (useWiFi && WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, attempting to reconnect...");
    setupWiFi();
  }

  // Read GPS data
  while (GPSSerial.available() > 0) {
    gps.encode(GPSSerial.read());
  }

  // Detect sound and record audio
  if (detectSound()) {
    digitalWrite(LED_PIN, HIGH);
    Serial.println("Sound detected, recording short clip...");

    recordAudio(SHORT_RECORD_DURATION, "/tmp.wav");
    saveAudioString("/tmp.wav", "/audiostring.txt");
    createAudioJsonRequest("/audiostring.txt", "/request-tmp.json");

    String transcript = transcribeAudioForTrigger();
    if (transcript.indexOf("tolong") != -1 || transcript.indexOf("Tolong") != -1) {
      Serial.println("Kata 'tolong' terdeteksi, merekam selama 30 detik...");
      recordAudio(LONG_RECORD_DURATION, "/long_recording.wav");
      sendAudioMultipart("/long_recording.wav", device_id, gps.location.isValid() ? String(gps.location.lng(), 6) : "110.123456");
      sendLocation = true; // Activate location sending
    } else {
      Serial.println("Kata 'tolong' tidak terdeteksi, melakukan transkripsi normal...");
      transcribeAudio();
    }

    digitalWrite(LED_PIN, LOW);
    delay(1000);
  }

  // Send location every 5 seconds if sendLocation is active
  if (sendLocation && (millis() - lastLocationSend >= LOCATION_INTERVAL)) {
    sendLocationData();
    lastLocationSend = millis();
  }
}