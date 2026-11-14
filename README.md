# Tree Network — Web Page in a Tree

## Overview

A self-contained web server that lives in a tree. The Arduino UNO R4 WiFi reads an HC-SR04 ultrasonic distance sensor, creates its own **open Wi-Fi hotspot** (no password), and serves a web page directly from the board. No internet, no cloud, no router — the board *is* the network.

The web page shows the live distance reading and spawns falling "leaf" words — more when someone is close, fewer when far. It also plays a melodious Geiger-counter-style blip: fast when close, slow when far. The board's own 12×8 LED matrix runs a matching falling-pixel animation, so the visual works even without a phone.

Built for autumn under a tree: leaves falling, sensor watching, page served locally.

## Hardware Required

- Arduino UNO R4 WiFi
- HC-SR04 ultrasonic distance sensor
- USB cable (for uploading only)
- Power source: USB power bank or 5V adapter (for outdoor use)
- A phone or computer with Wi-Fi and a web browser

## Wiring (HC-SR04)

| Sensor Pin | Arduino Pin |
|---|---|
| VCC | 5V |
| GND | GND |
| Trig | 13 |
| Echo | 12 |

The HC-SR04 is a 5V device and connects directly — no level shifting needed.

## How It Works

1. **Hotspot:** `WiFi.beginAP("R4-Test")` creates an **open** Wi-Fi network (no password). The board gets the fixed gateway address `192.168.4.1`.
2. **Sensor loop:** Every 1 second, `readDistance()` triggers the HC-SR04 and computes centimeters from the echo pulse (30 ms timeout, clamped to 2–400 cm).
3. **HTTP server:** The board listens on port 80 and serves two endpoints:
   - `/` → the full HTML page (once per page load)
   - `/data` → the current distance as plain text (fetched by the page every second)
4. **Page behavior:** The HTML loads once. JavaScript then polls `/data` every second, updates the distance + leaf count display, and spawns falling "leaf" words. The page never reloads, so the DOM and animation persist.
5. **LED matrix:** A separate particle system runs every 150 ms, mapping distance to particle count (closer → more falling pixels).
6. **Sound (optional):** Clicking **Enable Sound** starts a Web Audio scheduler that blips at intervals mapped from distance — 100 ms when close, 2000 ms when far — cycling through a C-major pentatonic scale.

## Connecting and Viewing

1. Upload the sketch via USB.
2. Power the board (USB or 5V source).
3. On your phone/computer, connect to the Wi-Fi network **`R4-Test`** (open — no password; a "no internet" warning is normal, keep the connection).
4. Open a browser and go to:
   ```
   http://192.168.4.1
   ```
5. You should see the distance reading, the falling leaves, and the leaf count. Click **Enable Sound** for the audio.
6. Wave your hand in front of the sensor — closer means more leaves, faster blips, and more falling pixels on the board.

### Why `192.168.4.1`?

In Access Point mode the board is the router. The `WiFiS3` library assigns it a fixed gateway address of `192.168.4.1` by default. Devices joining the hotspot receive addresses in the `192.168.4.x` range via DHCP from the board. The IP is also printed to the Serial Monitor at startup.

## The Sketch

```cpp
#include "WiFiS3.h"
#include "ArduinoGraphics.h"
#include "Arduino_LED_Matrix.h"

char ssid[] = "R4-Test";
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

  status = WiFi.beginAP(ssid, pass);
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
      client.println("let audioCtx = null;");
      client.println("let soundOn = false;");
      client.println("let latestDistance = 400;");
      client.println("let blipIndex = 0;");
      client.println("function mapDist(d, inMin, inMax, outMin, outMax) {");
      client.println("  return (d - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;");
      client.println("}");
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
```

## Key Concepts Demonstrated

| Concept | Explanation |
|---|---|
| Access Point mode | `WiFi.beginAP()` makes the board its own hotspot — no router or internet needed |
| Open network | Calling `beginAP(ssid)` without a password creates an open (no-password) hotspot |
| Fixed IP | AP mode gives the board `192.168.4.1`, the gateway of its own network |
| Web server | `WiFiServer` + `client.println()` writes raw HTTP responses byte-by-byte |
| Path routing | The request line is inspected to serve `/` (HTML) or `/data` (plain text) |
| Stateless HTTP | The board remembers nothing per client; variables live in `loop()` |
| Polling with `fetch` | The page loads once; JS re-fetches `/data` every second without reloading |
| HTML/JS from the board | All markup, CSS, and JS are emitted as strings from the sketch |
| Web Audio API | Oscillator + gain envelope produce short blips; tempo mapped from distance |
| Pentatonic scale | Notes chosen from C-major pentatonic so random pitches stay melodious |
| LED matrix particles | A small particle array updated every 150 ms, rendered with `renderBitmap` |
| Distance mapping | `map()`/`mapDist()` convert distance to leaf count, particle count, and blip interval |

## Notes and Troubleshooting

- **Include order matters:** `ArduinoGraphics.h` must come **before** `Arduino_LED_Matrix.h`.
- **Library not found on a new computer:** install the **Arduino UNO R4 Boards** package via **Tools → Boards → Boards Manager**; `WiFiS3` ships with it. Then select **Tools → Board → Arduino UNO R4 WiFi** and restart the IDE.
- **Browser won't load the page:** try `http://192.168.4.1` explicitly (some browsers default to HTTPS). Disable mobile data temporarily if the browser keeps using cellular.
- **Sensor reads 400 constantly:** check wiring (Trig/Echo swapped, VCC not on 5V) and confirm the pins match the sketch.
- **Sound doesn't start:** browsers require a user gesture — click **Enable Sound**. On iOS Safari the context may need the `audioCtx.resume()` call in `toggleSound()`.
- **Open network:** anyone in range can connect. Fine for a temporary local project; don't use for sensitive data.
- **USB:** only required for uploading. After that, any 5V source runs the board.
- **Range:** the onboard PCB antenna is basic. Test at your actual outdoor location; range drops sharply through walls or foliage.

## Security Note

This sketch uses `WiFi.beginAP(ssid, pass)` with a password in the code shown above. To make the network **open (no password)**, change that line in `setup()` to:

```cpp
status = WiFi.beginAP(ssid);
```

Then anyone in range can connect without a password.

## Next Steps

- Replace the random/ultrasonic value with other sensors (temperature, light, motion).
- Add a QR code on a sign pointing to `http://192.168.4.1` to make connecting easier.
- Log readings to an SD card for offline data collection.
- Battery + solar for longer unattended outdoor deployment.

## Philosophy

Most "connected" projects assume the internet is always there. This one assumes it isn't. The page and the data live at the point of collection — which means the project works in a field, a forest, a basement, or anywhere the cloud doesn't reach.

**The internet can go down. The tree is fine.**
