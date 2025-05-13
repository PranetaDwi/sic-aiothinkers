#include <Arduino.h>
#include <WiFi.h>
#include <FS.h>
#include <SD.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <TinyGPS++.h>
#include <HardwareSerial.h>

// GPS setup
HardwareSerial GPSSerial(1);
TinyGPSPlus gps;

String device_id = "ESP32_002";

// Pins
const int SD_CS = 5;
const int AUDIO_PIN = 34;
const int LED_PIN = 33;

// Configuration for audio recording
const int SAMPLE_RATE = 8000;
const int BIT_DEPTH = 16;
const int SHORT_RECORD_DURATION = 2; // Durasi untuk deteksi "tolong"
const int LONG_RECORD_DURATION = 30; // 30 detik untuk rekaman panjang

// Sound detection threshold
const int SOUND_THRESHOLD = 1000; // Sesuaikan berdasarkan pengujian
const int DETECTION_WINDOW = 100; // Jumlah sampel untuk deteksi suara

// WIFI connection
String SSID = "UGM Insecure";
String PASSWORD = "123456789";

// Gemini API key
String API_KEY = "AIzaSyDLxP_9e7cEdJ8hP1jW__gDZOGmkZxL_VQ";

bool sendLocation = false;
unsigned long lastLocationSend = 0;
const unsigned long LOCATION_INTERVAL = 5000;

void setupWifi() {
  WiFi.begin(SSID.c_str(), PASSWORD.c_str());
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print("...");
  }
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
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
  unsigned long sampleInterval = 1000000 / SAMPLE_RATE; // Interval dalam mikrodetik (125 µs untuk 8000 Hz)

  for (int i = 0; i < numSamples; i++) {
    unsigned long targetTime = startTime + (i * sampleInterval);
    
    // Tunggu hingga waktu target untuk sampel berikutnya
    while (micros() < targetTime) {
      // Tidak melakukan apa-apa, hanya menunggu
    }

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

  // Chunk RIFF
  file.write((const uint8_t*)"RIFF", 4);
  uint32_t fileSize = 36 + dataSize; // Ukuran total file - 8
  file.write((uint8_t*)&fileSize, 4);
  file.write((const uint8_t*)"WAVE", 4);

  // Subchunk fmt
  file.write((const uint8_t*)"fmt ", 4);
  uint32_t subchunk1Size = 16;
  file.write((uint8_t*)&subchunk1Size, 4);
  uint16_t audioFormat = 1; // PCM
  file.write((uint8_t*)&audioFormat, 2);
  file.write((uint8_t*)&channels, 2);
  file.write((uint8_t*)&sampleRate, 4);
  file.write((uint8_t*)&byteRate, 4);
  file.write((uint8_t*)&blockAlign, 2);
  file.write((uint8_t*)&bitDepth, 2);

  // Subchunk data
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
  return "";
}

void transcribeAudio() {
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
}

bool detectSound() {
  int maxAmplitude = 0;
  for (int i = 0; i < DETECTION_WINDOW; i++) {
    int rawValue = analogRead(AUDIO_PIN);
    int amplitude = abs(rawValue - 2048); // Asumsi nilai tengah ADC ~2048
    if (amplitude > maxAmplitude) {
      maxAmplitude = amplitude;
    }
    delayMicroseconds(1000000 / SAMPLE_RATE);
  }
  Serial.print("Max Amplitude: ");
  Serial.println(maxAmplitude);
  return maxAmplitude > SOUND_THRESHOLD;
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  WRITE_PERI_REG(RTC_CNTL_WDTCONFIG0_REG, 0);

  pinMode(LED_PIN, OUTPUT);
  Serial.begin(115200);
  delay(1000); // <- penting
  GPSSerial.begin(9600, SERIAL_8N1, 16, 17);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  while (!Serial);

  setupWifi();

  if (!SD.begin(SD_CS, SPI, 1000000)) {
    Serial.println("SD card initialization failed!");
    while (1);
  }
  Serial.println("SD card initialized!");

  Serial.print("Nunggu GPS fix");
  while (!gps.location.isValid()) {
    while (GPSSerial.available() > 0) {
      gps.encode(GPSSerial.read());
    }
    Serial.print(".");
    delay(1000); // kasih delay biar gak spam
  }
  Serial.println("\nGPS fix dapet!");
}

void sendAudioMultipart(String filepath, String device_id) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi belum terhubung.");
    return;
  }

  File audioFile = SD.open(filepath, FILE_READ);
  if (!audioFile) {
    Serial.println("Gagal membuka file audio.");
    return;
  }

  WiFiClient client;
  String boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";

  if (!client.connect("192.168.27.150", 5000)) {
    Serial.println("Gagal terhubung ke server.");
    return;
  }

  String head = "--" + boundary + "\r\n";
  head += "Content-Disposition: form-data; name=\"device_id\"\r\n\r\n";
  head += device_id + "\r\n";

  head += "--" + boundary + "\r\n";
  head += "Content-Disposition: form-data; name=\"file\"; filename=\"long_recording.wav\"\r\n";
  head += "Content-Type: audio/wav\r\n\r\n";

  String tail = "\r\n--" + boundary + "--\r\n";
  size_t contentLength = head.length() + audioFile.size() + tail.length();

  // Kirim header HTTP
  client.print("POST /upload HTTP/1.1\r\n");
  client.print("Host: 192.168.27.150\r\n");
  client.print("Content-Type: multipart/form-data; boundary=" + boundary + "\r\n");
  client.print("Content-Length: " + String(contentLength) + "\r\n");
  client.print("Connection: close\r\n\r\n");

  // Kirim body
  client.print(head);

  const size_t bufferSize = 2048;
  uint8_t buffer[bufferSize];
  while (audioFile.available()) {
    size_t bytesRead = audioFile.read(buffer, bufferSize);
    client.write(buffer, bytesRead);
    yield();
  }

  client.print(tail);

  // Baca respons dari server
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") break;
  }

  String response = client.readString();
  Serial.println("Respons dari server:");
  Serial.println(response);

  client.stop();
  audioFile.close();
}

// lokasi pertama kata tolong dideteksi
void locationCaptured(){
  while (GPSSerial.available() > 0) {
    gps.encode(GPSSerial.read());
  }

  float lat = gps.location.lat();
  float lon = gps.location.lng();

  Serial.print("Lat: ");
  Serial.print(lat, 6);
  Serial.print(" | Lon: ");
  Serial.println(lon, 6);

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin("http://192.168.27.150:5000/vulnerable-location");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    // Format data ke string body
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
  }
}

void sendLocationData() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi belum terhubung.");
    return;
  }

  if (gps.location.isValid()) {
    float lat = gps.location.lat();
    float lon = gps.location.lng();

    Serial.print("Lat: ");
    Serial.print(lat, 6);
    Serial.print(" | Lon: ");
    Serial.println(lon, 6);

    HTTPClient http;
    http.begin("http://192.168.27.150:5000/post-location");
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
}

void loop() {
  if (detectSound()) {
    digitalWrite(LED_PIN, HIGH);
    Serial.println("Sound detected, recording short clip...");
    
    // Rekam klip pendek untuk deteksi "tolong"
    recordAudio(SHORT_RECORD_DURATION, "/tmp.wav");
    saveAudioString("/tmp.wav", "/audiostring.txt");
    createAudioJsonRequest("/audiostring.txt", "/request-tmp.json");
    
    // Periksa apakah "tolong" ada di transkripsi
    String transcript = transcribeAudioForTrigger();
    if (transcript.indexOf("tolong") != -1 || transcript.indexOf("Tolong") != -1) {
      Serial.println("Kata 'tolong' terdeteksi, merekam selama 30 detik...");
      recordAudio(LONG_RECORD_DURATION, "/long_recording.wav");
      locationCaptured();
      sendAudioMultipart("/long_recording.wav", device_id);
      sendLocation = true;
    } else {
      Serial.println("Kata 'tolong' tidak terdeteksi, melakukan transkripsi normal...");
      transcribeAudio();
    }
    
    digitalWrite(LED_PIN, LOW);
    delay(1000); // Delay untuk mencegah rekaman berulang segera
  }

  if (sendLocation && (millis() - lastLocationSend >= LOCATION_INTERVAL)) {
    sendLocationData();
    lastLocationSend = millis();
  }

}