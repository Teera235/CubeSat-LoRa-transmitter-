import java.util.concurrent.ConcurrentLinkedQueue;
import processing.serial.*;

Serial serialPort;
String serialBuffer = "";
ConcurrentLinkedQueue<String> dataQueue = new ConcurrentLinkedQueue<String>();

PShape cubeSatModel;
boolean modelLoaded = false;

class OrientationData {
  float roll, pitch, yaw;
  float quatW, quatX, quatY, quatZ;
  float temperature;
  float pressure;
  long timeDiff;
  int rssi;
  float snr;
  long timestamp;
  
  OrientationData(float r, float p, float y, float qw, float qx, float qy, float qz, 
                  float temp, float press, long td, int rs, float sn) {
    roll = r; pitch = p; yaw = y;
    quatW = qw; quatX = qx; quatY = qy; quatZ = qz;
    temperature = temp; pressure = press;
    timeDiff = td; rssi = rs; snr = sn;
    timestamp = millis();
  }
}

OrientationData currentOrientation;
ArrayList<OrientationData> orientationHistory;
final int MAX_HISTORY = 1000;

PFont uiFont, titleFont, dataFont;
boolean showDebugInfo = true;
boolean showGrid = true;
boolean useQuaternion = true;
boolean showTemperatureGraph = true;
boolean showPressureGraph = true;
boolean showEulerGraph = true;
boolean showQuaternionGraph = true;
boolean showAttitudeIndicator = true;
float cameraDistance = 500;
float cameraAngleX = 0;
float cameraAngleY = 0;

int frameCounter = 0;
long lastFPSUpdate = 0;
float currentFPS = 0;
int packetsReceived = 0;
int packetsPerSecond = 0;
long lastPacketCount = 0;

boolean useFlippedCoords = false;

// Enhanced Color Scheme
color backgroundColor = color(12, 15, 28);
color gridColor = color(45, 55, 80, 120);
color modelColor = color(220, 235, 255);
color axisColorX = color(255, 120, 120);
color axisColorY = color(120, 255, 120);
color axisColorZ = color(120, 120, 255);
color uiBackgroundColor = color(25, 30, 45, 200);
color titleColor = color(255, 230, 120);
color dataColor = color(180, 220, 255);
color tempColor = color(255, 150, 100);
color pressureColor = color(100, 200, 255);
color signalGoodColor = color(100, 255, 120);
color signalBadColor = color(255, 100, 100);
color rollColor = color(255, 120, 120);
color pitchColor = color(120, 255, 120);
color yawColor = color(120, 120, 255);
color quatWColor = color(255, 200, 100);
color quatXColor = color(255, 120, 120);
color quatYColor = color(120, 255, 120);
color quatZColor = color(120, 120, 255);
color attitudeColor = color(255, 255, 255);

void setup() {
  size(1600, 1000, P3D);
  
  titleFont = createFont("Arial Bold", 26);
  uiFont = createFont("Arial", 15);
  dataFont = createFont("Courier New", 13);
  
  orientationHistory = new ArrayList<OrientationData>();
  
  loadCubeSatModel();
  setupSerial();
  
  println("=== CubeSat 3D Visualizer with Telemetry ===");
  println("Controls:");
  println("- Mouse: Rotate camera");
  println("- Scroll: Zoom in/out");
  println("- 'g': Toggle grid");
  println("- 'd': Toggle debug info");
  println("- 'q': Toggle Quaternion/Euler mode");
  println("- 't': Toggle temperature graph");
  println("- 'p': Toggle pressure graph");
  println("- 'e': Toggle Euler angles graph");
  println("- 'u': Toggle quaternion graph");
  println("- 'a': Toggle attitude indicator");
  println("- 'r': Reset camera");
  println("- 's': Save screenshot");
  println("===============================================");
}

void loadCubeSatModel() {
  try {
    cubeSatModel = loadShape("model/CubeSat.obj");
    
    if (cubeSatModel != null) {
      modelLoaded = true;
      cubeSatModel.scale(100);
      println("✓ CubeSat model loaded successfully");
    } else {
      println("✗ Failed to load CubeSat.obj");
      modelLoaded = false;
    }
  } catch (Exception e) {
    println("✗ Error loading CubeSat model: " + e.getMessage());
    println("  Make sure the model files exist in the model/ folder");
    modelLoaded = false;
  }
}

void setupSerial() {
  try {
    println("Available Serial Ports:");
    String[] portList = Serial.list();
    for (int i = 0; i < portList.length; i++) {
      println("  [" + i + "] " + portList[i]);
    }
    
    serialPort = new Serial(this, "COM5", 115200);
    serialPort.bufferUntil('\n');
    println("✓ Serial connection established on COM5 at 115200 baud");
    
  } catch (Exception e) {
    println("✗ Serial connection failed: " + e.getMessage());
    println("  Make sure COM5 is available and Arduino is connected");
  }
}

void serialEvent(Serial port) {
  try {
    String data = port.readStringUntil('\n');
    if (data != null) {
      data = data.trim();
      if (!data.isEmpty() && !data.startsWith("ERROR") && 
          !data.startsWith("WARNING") && !data.startsWith("STATS") &&
          !data.contains("Initializing") && !data.contains("Ready") &&
          !data.contains("Format:")) {
        dataQueue.offer(data);
      }
    }
  } catch (Exception e) {
    println("Serial read error: " + e.getMessage());
  }
}

void processSerialData() {
  while (!dataQueue.isEmpty()) {
    String data = dataQueue.poll();
    parseOrientationData(data);
  }
}

void parseOrientationData(String data) {
  try {
    String[] values = data.split(",");
    
    // Updated for new format: Roll,Pitch,Yaw,QuatW,QuatX,QuatY,QuatZ,TempC,Pressure,TimeDiff,RSSI,SNR
    if (values.length >= 12) {
      float roll = -Float.parseFloat(values[0]);
      float pitch = Float.parseFloat(values[1]);
      float yaw = Float.parseFloat(values[2]);
      float quatW = Float.parseFloat(values[3]);
      float quatX = Float.parseFloat(values[4]);
      float quatY = Float.parseFloat(values[5]);
      float quatZ = Float.parseFloat(values[6]);
      float temperature = Float.parseFloat(values[7]);
      float pressure = Float.parseFloat(values[8]);
      long timeDiff = Long.parseLong(values[9]);
      int rssi = Integer.parseInt(values[10]);
      float snr = Float.parseFloat(values[11]);
      
      currentOrientation = new OrientationData(roll, pitch, yaw, 
                                               quatW, quatX, quatY, quatZ,
                                               temperature, pressure,
                                               timeDiff, rssi, snr);
      
      orientationHistory.add(currentOrientation);
      if (orientationHistory.size() > MAX_HISTORY) {
        orientationHistory.remove(0);
      }
      
      packetsReceived++;
    }
  } catch (Exception e) {
    println("Parse error: " + e.getMessage() + " | Data: " + data);
  }
}

void draw() {
  background(backgroundColor);
  
  processSerialData();
  updatePerformanceCounters();
  setup3DCamera();
  draw3DScene();
  
  camera();
  draw2DUI();
  
  // Draw attitude indicator in 3D space (top right)
  if (showAttitudeIndicator) {
    drawAttitudeIndicator();
  }
  
  if (showTemperatureGraph) {
    drawTemperatureGraph();
  }
  
  if (showPressureGraph) {
    drawPressureGraph();
  }
  
  if (showEulerGraph) {
    drawEulerGraph();
  }
  
  if (showQuaternionGraph) {
    drawQuaternionGraph();
  }
}

void updatePerformanceCounters() {
  frameCounter++;
  long currentTime = millis();
  
  if (currentTime - lastFPSUpdate >= 1000) {
    currentFPS = frameCounter * 1000.0 / (currentTime - lastFPSUpdate);
    frameCounter = 0;
    lastFPSUpdate = currentTime;
    
    packetsPerSecond = (int)(packetsReceived - lastPacketCount);
    lastPacketCount = packetsReceived;
  }
}

void setup3DCamera() {
  float cameraX = cameraDistance * cos(cameraAngleY) * sin(cameraAngleX);
  float cameraY = cameraDistance * sin(cameraAngleY);
  float cameraZ = cameraDistance * cos(cameraAngleY) * cos(cameraAngleX);
  
  camera(-cameraX, cameraY, cameraZ, 0, 0, 0, 0, 1, 0);
  
  ambientLight(70, 80, 100);
  directionalLight(120, 140, 160, -0.5, 0.5, -1);
  pointLight(100, 120, 140, cameraX/2, cameraY/2, cameraZ/2);
}

void draw3DScene() {
  if (showGrid) {
    drawGrid();
  }
  
  drawAxes();
  
  if (currentOrientation != null) {
    pushMatrix();
    
    // Apply temperature-based color effect
    float tempNorm = map(constrain(currentOrientation.temperature, -20, 60), -20, 60, 0, 1);
    color satColor = lerpColor(color(100, 150, 255), color(255, 100, 100), tempNorm);
    
    if (useQuaternion) {
      if (useFlippedCoords) {
        applyQuaternionRotation(currentOrientation.quatW, 
                               currentOrientation.quatX,
                               -currentOrientation.quatY,
                               currentOrientation.quatZ);
      } else {
        applyQuaternionRotation(currentOrientation.quatW, 
                               -currentOrientation.quatX,
                               currentOrientation.quatZ,
                               -currentOrientation.quatY);
      }
    } else {
      if (useFlippedCoords) {
        rotateZ(radians(currentOrientation.yaw));
        rotateX(radians(currentOrientation.pitch));
        rotateY(radians(currentOrientation.roll));
      } else {
        rotateY(radians(-currentOrientation.yaw));
        rotateX(radians(-currentOrientation.pitch));
        rotateZ(radians(currentOrientation.roll));
      }
    }
    
    if (modelLoaded && cubeSatModel != null) {
      fill(satColor);
      stroke(255, 180);
      strokeWeight(0.5);
      shape(cubeSatModel);
    } else {
      drawFallbackCube(satColor);
    }
    
    popMatrix();
  }
  
  if (orientationHistory.size() > 1) {
    drawOrientationTrail();
  }
}

void applyQuaternionRotation(float w, float x, float y, float z) {
  float norm = sqrt(w*w + x*x + y*y + z*z);
  if (norm > 0) {
    w /= norm; x /= norm; y /= norm; z /= norm;
  }
  
  float m00 = 1 - 2*(y*y + z*z);
  float m01 = 2*(x*y - w*z);
  float m02 = 2*(x*z + w*y);
  float m10 = 2*(x*y + w*z);
  float m11 = 1 - 2*(x*x + z*z);
  float m12 = 2*(y*z - w*x);
  float m20 = 2*(x*z - w*y);
  float m21 = 2*(y*z + w*x);
  float m22 = 1 - 2*(x*x + y*y);
  
  applyMatrix(m00, m01, m02, 0,
              m10, m11, m12, 0,
              m20, m21, m22, 0,
              0,   0,   0,   1);
}

void drawGrid() {
  stroke(gridColor);
  strokeWeight(1);
  
  int gridSize = 400;
  int gridSpacing = 50;
  
  for (int i = -gridSize; i <= gridSize; i += gridSpacing) {
    line(i, 0, -gridSize, i, 0, gridSize);
    line(-gridSize, 0, i, gridSize, 0, i);
  }
}

void drawAxes() {
  strokeWeight(4);
  
  // X-axis (Red)
  stroke(axisColorX);
  line(0, 0, 0, 120, 0, 0);
  
  // Y-axis (Green) 
  stroke(axisColorY);
  line(0, 0, 0, 0, 0, 120);
  
  // Z-axis (Blue)
  stroke(axisColorZ);
  line(0, 0, 0, 0, -120, 0);
  
  textSize(14);
  
  fill(axisColorX);
  pushMatrix();
  translate(130, 0, 0);
  text("X", 0, 0);
  popMatrix();
  
  fill(axisColorY);
  pushMatrix();
  translate(0, 0, 130);
  text("Y", 0, 0);
  popMatrix();
  
  fill(axisColorZ);
  pushMatrix();
  translate(0, -130, 0);
  text("Z", 0, 0);
  popMatrix();
}

void drawFallbackCube(color cubeColor) {
  fill(cubeColor, 200);
  stroke(255);
  strokeWeight(1.5);
  box(80);
}

void drawOrientationTrail() {
  if (orientationHistory.size() < 2) return;
  
  stroke(255, 120, 120, 150);
  strokeWeight(2);
  noFill();
  
  beginShape();
  for (int i = max(0, orientationHistory.size() - 100); i < orientationHistory.size(); i++) {
    OrientationData od = orientationHistory.get(i);
    float x = od.roll * 2;
    float y = od.pitch * 2;
    float z = od.yaw * 2;
    vertex(x, y, z);
  }
  endShape();
}

void drawTemperatureGraph() {
  if (orientationHistory.size() < 2) return;
  
  int graphX = width - 320;
  int graphY = 20;
  int graphW = 280;
  int graphH = 120;
  
  // Background
  fill(uiBackgroundColor);
  noStroke();
  rect(graphX, graphY, graphW, graphH);
  
  // Title
  fill(tempColor);
  textFont(uiFont);
  text("Temperature (°C)", graphX + 10, graphY + 20);
  
  // Graph
  if (orientationHistory.size() > 1) {
    float minTemp = Float.MAX_VALUE;
    float maxTemp = Float.MIN_VALUE;
    
    // Find min/max for scaling
    int startIdx = max(0, orientationHistory.size() - 100);
    for (int i = startIdx; i < orientationHistory.size(); i++) {
      float temp = orientationHistory.get(i).temperature;
      minTemp = min(minTemp, temp);
      maxTemp = max(maxTemp, temp);
    }
    
    if (maxTemp - minTemp < 1) {
      minTemp -= 0.5;
      maxTemp += 0.5;
    }
    
    stroke(tempColor);
    strokeWeight(2);
    noFill();
    
    beginShape();
    for (int i = startIdx; i < orientationHistory.size(); i++) {
      float temp = orientationHistory.get(i).temperature;
      float x = map(i - startIdx, 0, orientationHistory.size() - startIdx - 1, graphX + 10, graphX + graphW - 10);
      float y = map(temp, minTemp, maxTemp, graphY + graphH - 20, graphY + 30);
      vertex(x, y);
    }
    endShape();
    
    // Current value
    if (currentOrientation != null) {
      fill(tempColor);
      textFont(dataFont);
      text(nf(currentOrientation.temperature, 1, 1) + "°C", graphX + graphW - 60, graphY + graphH - 5);
      
      // Min/Max labels
      textSize(11);
      fill(200);
      text(nf(maxTemp, 1, 1), graphX + 10, graphY + 35);
      text(nf(minTemp, 1, 1), graphX + 10, graphY + graphH - 15);
    }
  }
}

void drawPressureGraph() {
  if (orientationHistory.size() < 2) return;
  
  int graphX = width - 320;
  int graphY = 160;
  int graphW = 280;
  int graphH = 120;
  
  // Background
  fill(uiBackgroundColor);
  noStroke();
  rect(graphX, graphY, graphW, graphH);
  
  // Title
  fill(pressureColor);
  textFont(uiFont);
  text("Pressure (Pa)", graphX + 10, graphY + 20);
  
  // Graph
  if (orientationHistory.size() > 1) {
    float minPress = Float.MAX_VALUE;
    float maxPress = Float.MIN_VALUE;
    
    // Find min/max for scaling
    int startIdx = max(0, orientationHistory.size() - 100);
    for (int i = startIdx; i < orientationHistory.size(); i++) {
      float press = orientationHistory.get(i).pressure;
      minPress = min(minPress, press);
      maxPress = max(maxPress, press);
    }
    
    if (maxPress - minPress < 10) {
      minPress -= 5;
      maxPress += 5;
    }
    
    stroke(pressureColor);
    strokeWeight(2);
    noFill();
    
    beginShape();
    for (int i = startIdx; i < orientationHistory.size(); i++) {
      float press = orientationHistory.get(i).pressure;
      float x = map(i - startIdx, 0, orientationHistory.size() - startIdx - 1, graphX + 10, graphX + graphW - 10);
      float y = map(press, minPress, maxPress, graphY + graphH - 20, graphY + 30);
      vertex(x, y);
    }
    endShape();
    
    // Current value
    if (currentOrientation != null) {
      fill(pressureColor);
      textFont(dataFont);
      text(nf(currentOrientation.pressure, 1, 0) + " Pa", graphX + graphW - 80, graphY + graphH - 5);
      
      // Min/Max labels
      textSize(11);
      fill(200);
      text(nf(maxPress, 1, 0), graphX + 10, graphY + 35);
      text(nf(minPress, 1, 0), graphX + 10, graphY + graphH - 15);
    }
  }
}

void drawAttitudeIndicator() {
  if (currentOrientation == null) return;
  
  // Save current camera state
  pushMatrix();
  
  // Set up attitude indicator viewport (top right)
  int aiSize = 180;
  int aiX = width - aiSize - 20;
  int aiY = 300;
  
  // Draw background circle
  fill(uiBackgroundColor);
  noStroke();
  ellipse(aiX + aiSize/2, aiY + aiSize/2, aiSize, aiSize);
  
  // Draw attitude indicator
  pushMatrix();
  translate(aiX + aiSize/2, aiY + aiSize/2);
  
  // Rotate based on roll and pitch
  rotate(radians(currentOrientation.roll));
  
  // Draw horizon line (pitch indicator)
  float pitchOffset = currentOrientation.pitch * 2; // Scale factor
  
  // Sky (blue)
  fill(100, 150, 255, 180);
  noStroke();
  rect(-aiSize/2, -aiSize/2, aiSize, aiSize/2 + pitchOffset);
  
  // Ground (brown)
  fill(150, 100, 50, 180);
  rect(-aiSize/2, pitchOffset, aiSize, aiSize/2 - pitchOffset);
  
  // Horizon line
  stroke(255, 255, 100);
  strokeWeight(3);
  line(-aiSize/3, pitchOffset, aiSize/3, pitchOffset);
  
  // Pitch ladder marks
  stroke(255, 200);
  strokeWeight(1);
  for (int angle = -30; angle <= 30; angle += 10) {
    if (angle != 0) {
      float yPos = angle * 2;
      line(-10, yPos, 10, yPos);
      
      fill(255);
      textAlign(CENTER);
      textSize(10);
      text(str(angle), 0, yPos - 3);
    }
  }
  
  popMatrix();
  
  // Draw aircraft symbol (fixed)
  stroke(255, 255, 100);
  strokeWeight(4);
  line(aiX + aiSize/2 - 20, aiY + aiSize/2, aiX + aiSize/2 + 20, aiY + aiSize/2);
  line(aiX + aiSize/2, aiY + aiSize/2 - 10, aiX + aiSize/2, aiY + aiSize/2 + 10);
  
  // Draw roll scale
  stroke(255);
  strokeWeight(2);
  noFill();
  ellipse(aiX + aiSize/2, aiY + aiSize/2, aiSize - 10, aiSize - 10);
  
  // Roll tick marks
  for (int angle = 0; angle < 360; angle += 30) {
    float x1 = (aiSize/2 - 15) * cos(radians(angle - 90));
    float y1 = (aiSize/2 - 15) * sin(radians(angle - 90));
    float x2 = (aiSize/2 - 5) * cos(radians(angle - 90));
    float y2 = (aiSize/2 - 5) * sin(radians(angle - 90));
    
    line(aiX + aiSize/2 + x1, aiY + aiSize/2 + y1, 
         aiX + aiSize/2 + x2, aiY + aiSize/2 + y2);
    
    if (angle % 90 == 0) {
      fill(255);
      textAlign(CENTER);
      textSize(12);
      float labelX = (aiSize/2 - 25) * cos(radians(angle - 90));
      float labelY = (aiSize/2 - 25) * sin(radians(angle - 90));
      text(str(angle), aiX + aiSize/2 + labelX, aiY + aiSize/2 + labelY + 4);
    }
  }
  
  // Roll pointer (moves with aircraft)
  pushMatrix();
  translate(aiX + aiSize/2, aiY + aiSize/2);
  rotate(radians(currentOrientation.roll));
  
  stroke(255, 255, 100);
  strokeWeight(3);
  fill(255, 255, 100);
  triangle(0, -aiSize/2 + 5, -8, -aiSize/2 + 20, 8, -aiSize/2 + 20);
  
  popMatrix();
  
  // Title
  fill(attitudeColor);
  textAlign(CENTER);
  textFont(uiFont);
  text("Attitude Indicator", aiX + aiSize/2, aiY - 10);
  
  // Values
  textAlign(LEFT);
  textFont(dataFont);
  text("Roll: " + nf(currentOrientation.roll, 1, 1) + "°", aiX, aiY + aiSize + 20);
  text("Pitch: " + nf(currentOrientation.pitch, 1, 1) + "°", aiX, aiY + aiSize + 35);
  
  popMatrix();
}

void drawEulerGraph() {
  if (orientationHistory.size() < 2) return;
  
  int graphX = width - 320;
  int graphY = 500;
  int graphW = 280;
  int graphH = 150;
  
  // Background
  fill(uiBackgroundColor);
  noStroke();
  rect(graphX, graphY, graphW, graphH);
  
  // Title
  fill(255);
  textFont(uiFont);
  text("Euler Angles (°)", graphX + 10, graphY + 20);
  
  // Graph
  if (orientationHistory.size() > 1) {
    float minAngle = -180;
    float maxAngle = 180;
    
    int startIdx = max(0, orientationHistory.size() - 100);
    
    // Draw Roll (Red)
    stroke(rollColor);
    strokeWeight(2);
    noFill();
    beginShape();
    for (int i = startIdx; i < orientationHistory.size(); i++) {
      float angle = orientationHistory.get(i).roll;
      float x = map(i - startIdx, 0, orientationHistory.size() - startIdx - 1, graphX + 10, graphX + graphW - 10);
      float y = map(angle, minAngle, maxAngle, graphY + graphH - 25, graphY + 30);
      vertex(x, y);
    }
    endShape();
    
    // Draw Pitch (Green)
    stroke(pitchColor);
    strokeWeight(2);
    beginShape();
    for (int i = startIdx; i < orientationHistory.size(); i++) {
      float angle = orientationHistory.get(i).pitch;
      float x = map(i - startIdx, 0, orientationHistory.size() - startIdx - 1, graphX + 10, graphX + graphW - 10);
      float y = map(angle, minAngle, maxAngle, graphY + graphH - 25, graphY + 30);
      vertex(x, y);
    }
    endShape();
    
    // Draw Yaw (Blue)
    stroke(yawColor);
    strokeWeight(2);
    beginShape();
    for (int i = startIdx; i < orientationHistory.size(); i++) {
      float angle = orientationHistory.get(i).yaw;
      float x = map(i - startIdx, 0, orientationHistory.size() - startIdx - 1, graphX + 10, graphX + graphW - 10);
      float y = map(angle, minAngle, maxAngle, graphY + graphH - 25, graphY + 30);
      vertex(x, y);
    }
    endShape();
    
    // Zero line
    stroke(100, 100, 100);
    strokeWeight(1);
    float zeroY = map(0, minAngle, maxAngle, graphY + graphH - 25, graphY + 30);
    line(graphX + 10, zeroY, graphX + graphW - 10, zeroY);
    
    // Current values and legend
    if (currentOrientation != null) {
      textFont(dataFont);
      textAlign(LEFT);
      
      fill(rollColor);
      text("Roll: " + nf(currentOrientation.roll, 1, 1) + "°", graphX + 10, graphY + graphH - 8);
      
      fill(pitchColor);
      text("Pitch: " + nf(currentOrientation.pitch, 1, 1) + "°", graphX + 70, graphY + graphH - 8);
      
      fill(yawColor);
      text("Yaw: " + nf(currentOrientation.yaw, 1, 1) + "°", graphX + 140, graphY + graphH - 8);
      
      // Scale labels
      textSize(10);
      fill(150);
      text("+180°", graphX + graphW - 35, graphY + 35);
      text("-180°", graphX + graphW - 35, graphY + graphH - 20);
      text("0°", graphX + graphW - 20, zeroY + 4);
    }
  }
}

void drawQuaternionGraph() {
  if (orientationHistory.size() < 2) return;
  
  int graphX = width - 320;
  int graphY = 670;
  int graphW = 280;
  int graphH = 150;
  
  // Background
  fill(uiBackgroundColor);
  noStroke();
  rect(graphX, graphY, graphW, graphH);
  
  // Title
  fill(255);
  textFont(uiFont);
  text("Quaternion Components", graphX + 10, graphY + 20);
  
  // Graph
  if (orientationHistory.size() > 1) {
    float minQuat = -1.0;
    float maxQuat = 1.0;
    
    int startIdx = max(0, orientationHistory.size() - 100);
    
    // Draw Quat W (Yellow/Orange)
    stroke(quatWColor);
    strokeWeight(2);
    noFill();
    beginShape();
    for (int i = startIdx; i < orientationHistory.size(); i++) {
      float quat = orientationHistory.get(i).quatW;
      float x = map(i - startIdx, 0, orientationHistory.size() - startIdx - 1, graphX + 10, graphX + graphW - 10);
      float y = map(quat, minQuat, maxQuat, graphY + graphH - 25, graphY + 30);
      vertex(x, y);
    }
    endShape();
    
    // Draw Quat X (Red)
    stroke(quatXColor);
    strokeWeight(2);
    beginShape();
    for (int i = startIdx; i < orientationHistory.size(); i++) {
      float quat = orientationHistory.get(i).quatX;
      float x = map(i - startIdx, 0, orientationHistory.size() - startIdx - 1, graphX + 10, graphX + graphW - 10);
      float y = map(quat, minQuat, maxQuat, graphY + graphH - 25, graphY + 30);
      vertex(x, y);
    }
    endShape();
    
    // Draw Quat Y (Green)
    stroke(quatYColor);
    strokeWeight(2);
    beginShape();
    for (int i = startIdx; i < orientationHistory.size(); i++) {
      float quat = orientationHistory.get(i).quatY;
      float x = map(i - startIdx, 0, orientationHistory.size() - startIdx - 1, graphX + 10, graphX + graphW - 10);
      float y = map(quat, minQuat, maxQuat, graphY + graphH - 25, graphY + 30);
      vertex(x, y);
    }
    endShape();
    
    // Draw Quat Z (Blue)
    stroke(quatZColor);
    strokeWeight(2);
    beginShape();
    for (int i = startIdx; i < orientationHistory.size(); i++) {
      float quat = orientationHistory.get(i).quatZ;
      float x = map(i - startIdx, 0, orientationHistory.size() - startIdx - 1, graphX + 10, graphX + graphW - 10);
      float y = map(quat, minQuat, maxQuat, graphY + graphH - 25, graphY + 30);
      vertex(x, y);
    }
    endShape();
    
    // Zero line
    stroke(100, 100, 100);
    strokeWeight(1);
    float zeroY = map(0, minQuat, maxQuat, graphY + graphH - 25, graphY + 30);
    line(graphX + 10, zeroY, graphX + graphW - 10, zeroY);
    
    // Current values and legend
    if (currentOrientation != null) {
      textFont(dataFont);
      textAlign(LEFT);
      
      fill(quatWColor);
      text("W: " + nf(currentOrientation.quatW, 1, 3), graphX + 10, graphY + graphH - 8);
      
      fill(quatXColor);
      text("X: " + nf(currentOrientation.quatX, 1, 3), graphX + 60, graphY + graphH - 8);
      
      fill(quatYColor);
      text("Y: " + nf(currentOrientation.quatY, 1, 3), graphX + 110, graphY + graphH - 8);
      
      fill(quatZColor);
      text("Z: " + nf(currentOrientation.quatZ, 1, 3), graphX + 160, graphY + graphH - 8);
      
      // Scale labels
      textSize(10);
      fill(150);
      text("+1.0", graphX + graphW - 25, graphY + 35);
      text("-1.0", graphX + graphW - 25, graphY + graphH - 20);
      text("0.0", graphX + graphW - 20, zeroY + 4);
    }
  }
}

void draw2DUI() {
  // Main info panel
  fill(uiBackgroundColor);
  noStroke();
  rect(10, 10, 400, showDebugInfo ? 380 : 120);
  
  fill(titleColor);
  textFont(titleFont);
  text("CubeSat 3D Telemetry Visualizer", 20, 40);
  textSize(16);
  text("By Teerathap Yaisungnoen", 20, 60);
  
  // Connection status
  textFont(uiFont);
  color connectionColor = (serialPort != null) ? signalGoodColor : signalBadColor;
  fill(connectionColor);
  text("Serial: " + (serialPort != null ? "Connected (COM5)" : "Disconnected"), 20, 85);
  
  // Performance info
  fill(dataColor);
  text("FPS: " + nf(currentFPS, 1, 1) + " | Data Rate: " + packetsPerSecond + " Hz", 20, 105);
  
  if (showDebugInfo && currentOrientation != null) {
    fill(255);
    int yPos = 130;
    
    text("=== ORIENTATION DATA ===", 20, yPos);
    yPos += 25;
    
    textFont(dataFont);
    
    // Mode and coordinates
    fill(200, 255, 200);
    text("Mode: " + (useQuaternion ? "Quaternion" : "Euler Angles"), 20, yPos);
    yPos += 18;
    text("Coordinate System: Aerospace (X-Right, Y-Forward, Z-Up)", 20, yPos);
    yPos += 25;
    
    // Euler angles
    fill(255, 200, 200);
    text("Roll:  " + nf(currentOrientation.roll, 2, 2) + "° (X-axis rotation)", 20, yPos);
    yPos += 18;
    text("Pitch: " + nf(currentOrientation.pitch, 2, 2) + "° (Y-axis rotation)", 20, yPos);
    yPos += 18;
    text("Yaw:   " + nf(currentOrientation.yaw, 2, 2) + "° (Z-axis rotation)", 20, yPos);
    yPos += 25;
    
    // Quaternions
    fill(200, 200, 255);
    text("Quat W: " + nf(currentOrientation.quatW, 1, 4), 20, yPos);
    yPos += 18;
    text("Quat X: " + nf(currentOrientation.quatX, 1, 4), 20, yPos);
    yPos += 18;
    text("Quat Y: " + nf(currentOrientation.quatY, 1, 4), 20, yPos);
    yPos += 18;
    text("Quat Z: " + nf(currentOrientation.quatZ, 1, 4), 20, yPos);
    yPos += 25;
    
    // Environmental data
    fill(tempColor);
    text("Temperature: " + nf(currentOrientation.temperature, 1, 2) + " °C", 20, yPos);
    yPos += 18;
    fill(pressureColor);
    text("Pressure: " + nf(currentOrientation.pressure, 1, 1) + " Pa", 20, yPos);
    yPos += 25;
    
    // Signal quality
    color rssiColor = (currentOrientation.rssi > -80) ? signalGoodColor : signalBadColor;
    fill(rssiColor);
    text("Signal RSSI: " + currentOrientation.rssi + " dBm", 20, yPos);
    yPos += 18;
    text("SNR: " + nf(currentOrientation.snr, 1, 2) + " dB", 20, yPos);
    yPos += 18;
    fill(dataColor);
    text("Packet Interval: " + currentOrientation.timeDiff + " ms", 20, yPos);
    yPos += 25;
    
    // Statistics
    fill(255, 255, 150);
    text("Total Packets: " + packetsReceived, 20, yPos);
    yPos += 18;
    text("History Buffer: " + orientationHistory.size() + "/" + MAX_HISTORY, 20, yPos);
  }
  
  // Controls help
  fill(200, 220, 255, 200);
  textFont(uiFont);
  int helpY = height - 160;
  text("CONTROLS:", 20, helpY);
  text("Mouse Drag: Rotate Camera  |  Scroll: Zoom", 20, helpY + 20);
  text("G: Grid  |  D: Debug Info  |  Q: Quat/Euler Mode", 20, helpY + 40);
  text("T: Temperature Graph  |  P: Pressure Graph", 20, helpY + 60);
  text("E: Euler Graph  |  U: Quaternion Graph  |  A: Attitude", 20, helpY + 80);
  text("F: Flip Coords  |  R: Reset Camera  |  S: Screenshot", 20, helpY + 100);
  text("Model Status: " + (modelLoaded ? "3D Model Loaded" : "Using Fallback Cube"), 20, helpY + 120);
}

// Input handlers
void mouseDragged() {
  if (mouseButton == LEFT) {
    cameraAngleX += (mouseX - pmouseX) * 0.01;
    cameraAngleY -= (mouseY - pmouseY) * 0.01;
    cameraAngleY = constrain(cameraAngleY, -PI/2 + 0.1, PI/2 - 0.1);
  }
}

void mouseWheel(MouseEvent event) {
  cameraDistance += event.getCount() * 25;
  cameraDistance = constrain(cameraDistance, 150, 2000);
}

void keyPressed() {
  switch (key) {
    case 'g':
    case 'G':
      showGrid = !showGrid;
      println("Grid display: " + (showGrid ? "ON" : "OFF"));
      break;
      
    case 'd':
    case 'D':
      showDebugInfo = !showDebugInfo;
      println("Debug info: " + (showDebugInfo ? "ON" : "OFF"));
      break;
      
    case 'q':
    case 'Q':
      useQuaternion = !useQuaternion;
      println("Rotation mode: " + (useQuaternion ? "Quaternion" : "Euler"));
      break;
      
    case 't':
    case 'T':
      showTemperatureGraph = !showTemperatureGraph;
      println("Temperature graph: " + (showTemperatureGraph ? "ON" : "OFF"));
      break;
      
    case 'p':
    case 'P':
      showPressureGraph = !showPressureGraph;
      println("Pressure graph: " + (showPressureGraph ? "ON" : "OFF"));
      break;
      
    case 'e':
    case 'E':
      showEulerGraph = !showEulerGraph;
      println("Euler angles graph: " + (showEulerGraph ? "ON" : "OFF"));
      break;
      
    case 'u':
    case 'U':
      showQuaternionGraph = !showQuaternionGraph;
      println("Quaternion graph: " + (showQuaternionGraph ? "ON" : "OFF"));
      break;
      
    case 'a':
    case 'A':
      showAttitudeIndicator = !showAttitudeIndicator;
      println("Attitude indicator: " + (showAttitudeIndicator ? "ON" : "OFF"));
      break;
      
    case 'r':
    case 'R':
      cameraAngleX = 0;
      cameraAngleY = 0;
      cameraDistance = 500;
      println("Camera reset");
      break;
      
    case 'f':
    case 'F':
      useFlippedCoords = !useFlippedCoords;
      println("Coordinate flip: " + (useFlippedCoords ? "ON" : "OFF"));
      break;
      
    case 's':
    case 'S':
      saveFrame("cubesat-telemetry-screenshot-####.png");
      println("Screenshot saved");
      break;
  }
}

void dispose() {
  if (serialPort != null) {
    serialPort.stop();
  }
  println("CubeSat Visualizer closed gracefully");
}
