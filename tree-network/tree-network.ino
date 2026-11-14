#include "WiFiS3.h"
#include "ArduinoGraphics.h"
#include "Arduino_LED_Matrix.h"

char ssid[] = "Tree-Network";
char pass[] = "12345678";

const int trigPin = 13;
const int echoPin = 12;

int status = WL_IDLE_STATUS;
WiFiServer server(80);
ArduinoLEDMatrix matrix;

int distanceCm = 400;
unsigned long lastSensorRead = 0;
const unsigned long readInterval = 1000;

#define MAX_PARTICLES 20
int particleCol[MAX_PARTICLES];
int particleRow[MAX_PARTICLES];
bool particleActive[MAX_PARTICLES];
unsigned long lastParticleUpdate = 0;
const unsigned long particleInterval = 150;

void setup() {
  Serial.begin(9600);
  matrix.begin();

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  //status = WiFi.beginAP(ssid, pass);
  status = WiFi.beginAP(ssid);  // no password

  if (status != WL_AP_LISTENING) {
    Serial.println("AP creation failed");
    while (true);
  }

  delay(1000);
  server.begin();
  Serial.print("Hotspot IP: ");
  Serial.println(WiFi.localIP());

  for (int i = 0; i < MAX_PARTICLES; i++) {
    particleActive[i] = false;
  }
}

void loop() {
  if (millis() - lastSensorRead >= readInterval) {
    lastSensorRead = millis();
    distanceCm = readDistance();
    Serial.print("Distance: ");
    Serial.print(distanceCm);
    Serial.println(" cm");
  }

  if (millis() - lastParticleUpdate >= particleInterval) {
    lastParticleUpdate = millis();
    updateParticles(distanceCm);
  }

  WiFiClient client = server.available();
  if (client) {
    String request = client.readStringUntil('\r');
    client.flush();

    if (request.indexOf("GET /data") >= 0) {
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/plain");
      client.println("Connection: close");
      client.println();
      client.print(distanceCm);
    } else {
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/html");
      client.println("Connection: close");
      client.println();
      client.println("<!DOCTYPE HTML>");
      client.println("<html>");
      client.println("<head>");
      client.println("<meta charset='UTF-8'>");
      client.println("<title>Autumn Leaves</title>");
      client.println("<style>");
      client.println("body { background: #2c3e50; color: #ecf0f1; font-family: sans-serif; text-align: center; margin: 0; padding: 0; }");
      client.println("#leaf-container { position: relative; height: 65vh; overflow: hidden; border: 1px solid #7f8c8d; margin: 10px auto; width: 95%; }");
      client.println(".leaf { position: absolute; color: #e67e22; opacity: 0; animation: fall 3s linear forwards; }");
      client.println("@keyframes fall { 0% { top: -40px; opacity: 1; } 100% { top: 100%; opacity: 0; } }");
      client.println("#distance-display { font-size: 42px; margin: 10px; }");
      client.println("#soundBtn { font-size: 18px; padding: 10px 20px; margin: 10px; border: none; border-radius: 6px; background: #e67e22; color: #fff; cursor: pointer; }");
      client.println("h1 { margin: 10px; }");
      client.println("</style>");
      client.println("</head>");
      client.println("<body>");
      client.println("<h1>Autumn Leaves</h1>");
      client.println("<div id='distance-display'>-- cm &nbsp;|&nbsp; -- leaves</div>");
      client.println("<button id='soundBtn' onclick='toggleSound()'>Enable Sound</button>");
      client.println("<div id='leaf-container'></div>");
      client.println("<script>");
      // ---- Audio state ----
      client.println("let audioCtx = null;");
      client.println("let soundOn = false;");
      client.println("let latestDistance = 400;");
      client.println("let blipIndex = 0;");
      // ---- Map helper (p5-style map, plain JS) ----
      client.println("function mapDist(d, inMin, inMax, outMin, outMax) {");
      client.println("  return (d - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;");
      client.println("}");
      // ---- Toggle sound on/off ----
      client.println("function toggleSound() {");
      client.println("  if (!audioCtx) {");
      client.println("    audioCtx = new (window.AudioContext || window.webkitAudioContext)();");
      client.println("  }");
      client.println("  if (!soundOn) {");
      client.println("    audioCtx.resume();");
      client.println("    soundOn = true;");
      client.println("    document.getElementById('soundBtn').textContent = 'Mute Sound';");
      client.println("    scheduleNextBlip();");
      client.println("  } else {");
      client.println("    audioCtx.suspend();");
      client.println("    soundOn = false;");
      client.println("    document.getElementById('soundBtn').textContent = 'Enable Sound';");
      client.println("  }");
      client.println("}");
      // ---- Schedule the next blip based on distance ----
      client.println("function scheduleNextBlip() {");
      client.println("  if (!soundOn) return;");
      client.println("  const interval = mapDist(latestDistance, 10, 400, 100, 2000);");
      client.println("  setTimeout(function() {");
      client.println("    if (soundOn) {");
      client.println("      playBlip();");
      client.println("      scheduleNextBlip();");
      client.println("    }");
      client.println("  }, interval);");
      client.println("}");
      // ---- Play one short melodious blip ----
      client.println("function playBlip() {");
      client.println("  if (!audioCtx || !soundOn) return;");
      client.println("  const now = audioCtx.currentTime;");
      client.println("  const osc = audioCtx.createOscillator();");
      client.println("  const gain = audioCtx.createGain();");
      client.println("  osc.type = 'sine';");
      client.println("  const scale = [523.25, 587.33, 659.25, 783.99, 880.00, 1046.50];");
      client.println("  osc.frequency.value = scale[blipIndex % scale.length];");
      client.println("  blipIndex++;");
      client.println("  gain.gain.setValueAtTime(0.0001, now);");
      client.println("  gain.gain.exponentialRampToValueAtTime(0.25, now + 0.01);");
      client.println("  gain.gain.exponentialRampToValueAtTime(0.0001, now + 0.12);");
      client.println("  osc.connect(gain);");
      client.println("  gain.connect(audioCtx.destination);");
      client.println("  osc.start(now);");
      client.println("  osc.stop(now + 0.15);");
      client.println("}");
      // ---- Sensor fetch + leaf generation (unchanged logic) ----
      client.println("async function updateSensor() {");
      client.println("  try {");
      client.println("    const response = await fetch('/data');");
      client.println("    const text = await response.text();");
      client.println("    const distance = parseInt(text);");
      client.println("    latestDistance = distance;");
      client.println("    const leafCount = generateLeaves(distance);");
      client.println("    document.getElementById('distance-display').innerHTML = distance + ' cm &nbsp;|&nbsp; ' + leafCount + ' leaves';");
      client.println("  } catch (e) { console.error(e); }");
      client.println("}");
      client.println("function generateLeaves(distance) {");
      client.println("  const container = document.getElementById('leaf-container');");
      client.println("  let leafCount = Math.max(0, Math.floor((400 - distance) / 10));");
      client.println("  leafCount = Math.min(leafCount, 40);");
      client.println("  for (let i = 0; i < leafCount; i++) {");
      client.println("    const leaf = document.createElement('div');");
      client.println("    leaf.className = 'leaf';");
      client.println("    leaf.textContent = 'leaf';");
      client.println("    const size = Math.floor(Math.random() * 28) + 12;");
      client.println("    leaf.style.fontSize = size + 'px';");
      client.println("    const hues = ['#e67e22', '#d35400', '#c0392b', '#f39c12', '#e74c3c'];");
      client.println("    leaf.style.color = hues[Math.floor(Math.random() * hues.length)];");
      client.println("    leaf.style.left = Math.random() * 100 + '%';");
      client.println("    leaf.style.animationDelay = Math.random() * 2 + 's';");
      client.println("    container.appendChild(leaf);");
      client.println("    setTimeout(() => leaf.remove(), 5000);");
      client.println("  }");
      client.println("  return leafCount;");
      client.println("}");
      client.println("setInterval(updateSensor, 1000);");
      client.println("updateSensor();");
      client.println("</script>");
      client.println("</body>");
      client.println("</html>");
    }

    delay(1);
    client.stop();
  }
}

int readDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 30000);
  if (duration == 0) return 400;

  int distance = duration * 0.034 / 2;
  if (distance > 400) distance = 400;
  if (distance < 2) distance = 2;
  return distance;
}

void updateParticles(int distance) {
  int targetCount = map(constrain(distance, 10, 400), 10, 400, MAX_PARTICLES, 2);

  int currentActive = 0;
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (particleActive[i]) currentActive++;
  }

  for (int i = 0; i < MAX_PARTICLES && currentActive < targetCount; i++) {
    if (!particleActive[i]) {
      particleActive[i] = true;
      particleCol[i] = random(0, 12);
      particleRow[i] = 0;
      currentActive++;
    }
  }

  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (particleActive[i]) {
      particleRow[i]++;
      if (particleRow[i] >= 8) particleActive[i] = false;
    }
  }

  uint8_t frame[8][12] = {0};
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (particleActive[i]) frame[particleRow[i]][particleCol[i]] = 1;
  }
  matrix.renderBitmap(frame, 8, 12);
}
