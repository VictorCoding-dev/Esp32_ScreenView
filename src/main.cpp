#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <WiFi.h>
#include <WebServer.h>
#include <BluetoothSerial.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
String nome = "";

// Pinos
#define POT_PIN      35
#define BUTTON_PIN   25
#define BUZZER_PIN   19
#define RELAY_PIN    26
#define DIST_PIN     33
#define LDR_PIN      34

int menuIndex = 0;
const int totalOptions = 9;
bool lastButtonState = HIGH;
bool relayOn = false;
int buzzerFreq = 1000;

BluetoothSerial SerialBT;
WebServer server(80);

// Wi-Fi config
const char* ssid = "ESP32_Buzzer";
const char* password = "12345678";

// Feedback remoto
String lastCommand = "Nenhum";
String lastStatus = "Aguardando comando";

// Protótipos
void showPotValue();
void showLDRValue();
void showDistance();
void activateBuzzer();
void toggleRelay();
void startWiFi();
void startBluetooth();
void handleRemote();
void sendStatusBluetooth();
void processCommand(String cmd);
String getStatus();

void setup() {
  Serial.begin(115200); // nome Bluetooth visível no celular //reconhecimento facial remoto
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);

  ledcSetup(0, buzzerFreq, 8);
  ledcAttachPin(BUZZER_PIN, 0);
}

void loop() {
  // Recebe nome via USB serial
  if (Serial.available()) {
    String entrada = Serial.readStringUntil('\n');
    if (entrada.startsWith("NOME:")) {
      nome = entrada.substring(5);
    }
  }

  // Navegação de menu
  bool currentButtonState = digitalRead(BUTTON_PIN);
  if (currentButtonState == LOW && lastButtonState == HIGH) {
    menuIndex = (menuIndex + 1) % totalOptions;
    delay(200);
  }
  lastButtonState = currentButtonState;

  // Atualiza display
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  switch (menuIndex) {
    case 0: display.println("1. Potenciometro"); showPotValue(); break;
    case 1: display.println("2. Fotoresistor"); showLDRValue(); break;
    case 2: display.println("3. Distancia"); showDistance(); break;
    case 3: display.println("4. Buzzer"); activateBuzzer(); break;
    case 4: display.println("5. Rele"); toggleRelay(); break;
    case 5: display.println("6. Wi-Fi ON"); startWiFi(); break;
    case 6: display.println("7. Bluetooth ON"); startBluetooth(); break;
    case 7: display.println("8. Remoto ativo"); handleRemote(); break;
    case 8:
      display.println("9. Reconhecimento Facial");
      display.setCursor(0, 10);
      display.print("Bem-vindo, ");
      display.println(nome);
      display.setCursor(0, 20);
      display.println("Aguardando comando...");
      
      // Escuta Bluetooth só nesse menu
   if (menuIndex == 8 && SerialBT.available()) {
  String comando = SerialBT.readStringUntil('\n');
  comando.trim();

  if (comando == "RECON") {
    Serial.println("RECON");  // envia para o PC via USB
  }
}

      break;
  }

  display.display();
  delay(100);
}

void showLDRValue() {
  int light = analogRead(LDR_PIN);
  display.print("LDR: ");
  display.println(light);

  display.setCursor(0, 20);
  if (light < 500) {
    display.println("Alta luminosidade");
  } else if (light < 2000) {
    display.println("Media luminosidade");
  } else {
    display.println("Baixa luminosidade");
  }
}

void showPotValue() {
  int valor = analogRead(POT_PIN);
  display.setCursor(0, 20);
  display.print("Valor: ");
  display.println(valor);
}


void showDistance() {
  int raw = analogRead(33);  // OUT conectado ao D33

  // Conversão direta: valor analógico já representa distância em cm
  // Ajuste conforme calibração: 0 = longe, ~147 = muito perto
  int distance = map(raw, 0, 4095, 0, 147);  // inverso: mais próximo = maior valor

  display.print("Distancia: ");
  display.print(distance);
  display.println(" cm");

  if (distance <= 3) {
    display.setCursor(0, 20);
    display.println("Olá! ");
  }
}

void activateBuzzer() {
  int potValue = analogRead(POT_PIN);
  buzzerFreq = map(potValue, 0, 4095, 100, 2000);
  ledcWriteTone(0, buzzerFreq);
  delay(500);
  ledcWriteTone(0, 0);

  display.print("Freq: ");
  display.print(buzzerFreq);
  display.println(" Hz");
}

void toggleRelay() {
  relayOn = !relayOn;
  digitalWrite(RELAY_PIN, relayOn ? LOW : HIGH);
  display.print("Rele: ");
  display.println(relayOn ? "Ligado" : "Desligado");
}

// Wi-Fi

void startWiFi() {
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  display.print("IP: ");
  display.println(IP);

  server.on("/", []() {
    String html = "<h1>Controle do ESP32</h1>"
                  "<form action='/set'><input name='cmd' placeholder='BUZZ:1200'><input type='submit'></form>"
                  "<p><a href='/status'>Ver status</a></p>";
    server.send(200, "text/html", html);
  });

  server.on("/set", []() {
    if (server.hasArg("cmd")) {
      String cmd = server.arg("cmd");
      processCommand(cmd);
    }
    server.sendHeader("Location", "/");
    server.send(303);
  });

  server.on("/status", []() {
    String status = getStatus();
    server.send(200, "text/plain", status);
  });

  server.begin();
}

// Bluetooth

void startBluetooth() {
  if (!SerialBT.hasClient()) {
    SerialBT.begin("ESP32-Buzzer");
    display.println("Bluetooth iniciado");
  }
}

void handleRemote() {
  server.handleClient();

  if (SerialBT.available()) {
    String cmd = SerialBT.readStringUntil('\n');
    processCommand(cmd);
  }

  sendStatusBluetooth();

  // Exibe comando e status no display
  display.setCursor(0, 20);
  display.print("Cmd: ");
  display.println(lastCommand);
  display.setCursor(0, 35);
  display.print("Status: ");
  display.println(lastStatus);
}

// Comando remoto

void processCommand(String cmd) {
  cmd.trim();
  lastCommand = cmd;

  if (cmd.startsWith("BUZZ:")) {
    buzzerFreq = cmd.substring(5).toInt();
    ledcWriteTone(0, buzzerFreq);
    delay(500);
    ledcWriteTone(0, 0);
    lastStatus = "Buzzer ativado em " + String(buzzerFreq) + " Hz";
  } else if (cmd == "RELE:ON") {
    relayOn = true;
    digitalWrite(RELAY_PIN, LOW);
    lastStatus = "Relé ligado";
  } else if (cmd == "RELE:OFF") {
    relayOn = false;
    digitalWrite(RELAY_PIN, HIGH);
    lastStatus = "Relé desligado";
  } else {
    lastStatus = "Comando desconhecido";
  }
}

String getStatus() {
  int pot = analogRead(POT_PIN);
  int ldr = analogRead(LDR_PIN);

  pinMode(DIST_PIN, OUTPUT);
  digitalWrite(DIST_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(DIST_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(DIST_PIN, LOW);
  pinMode(DIST_PIN, INPUT);
  long duration = pulseIn(DIST_PIN, HIGH, 30000);
  int dist = duration * 0.034 / 2;

  String status = "Potenciometro: " + String(pot) + "\n";
  status += "LDR: " + String(ldr) + "\n";
  status += "Distancia: " + String(dist) + " cm\n";
  status += "Rele: " + String(relayOn ? "Ligado" : "Desligado") + "\n";
  status += "Buzzer: " + String(buzzerFreq) + " Hz\n";
  return status;
}

void sendStatusBluetooth() {
  if (SerialBT.hasClient()) {
    SerialBT.println(getStatus());
  }
}
