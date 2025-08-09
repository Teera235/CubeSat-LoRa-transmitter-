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
  long timeDiff;
  int rssi;
  float snr;
  long timestamp;
  
  OrientationData(float r, float p, float y, float qw, float qx, float qy, float qz, 
                  long td, int rs, float sn) {
    roll = r; pitch = p; yaw = y;
    quatW = qw; quatX = qx; quatY = qy; quatZ = qz;
    timeDiff = td; rssi = rs; snr = sn;
    timestamp = millis();
  }
}

OrientationData currentOrientation;
ArrayList<OrientationData> orientationHistory;
final int MAX_HISTORY = 1000;

PFont uiFont, titleFont;
boolean showDebugInfo = true;
boolean showGrid = true;
boolean useQuaternion = true;
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

color backgroundColor = color(15, 15, 25);
color gridColor = color(40, 40, 60, 100);
color modelColor = color(200, 200, 255);
color axisColorX = color(255, 100, 100);
color axisColorY = color(100, 255, 100);
color axisColorZ = color(100, 100, 255);

void setup() {
  size(1400, 900, P3D);
  
  titleFont = createFont("Arial Bold", 24);
  uiFont = createFont("Arial", 14);
  
  orientationHistory = new ArrayList<OrientationData>();
  
  loadCubeSatModel();
  setupSerial();
  
  println("=== CubeSat 3D Visualizer Initialized ===");
  println("Controls:");
  println("- Mouse: Rotate camera");
  println("- Scroll: Zoom in/out");
  println("- 'g': Toggle grid");
  println("- 'd': Toggle debug info");
  println("- 'q': Toggle Quaternion/Euler mode");
  println("- 'r': Reset camera");
  println("============================================");
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
          !data.contains("Initializing") && !data.contains("Ready")) {
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
    
    if (values.length >= 10) {
      float roll = -Float.parseFloat(values[0]);
      float pitch = Float.parseFloat(values[1]);
      float yaw = Float.parseFloat(values[2]);
      float quatW = Float.parseFloat(values[3]);
      float quatX = Float.parseFloat(values[4]);
      float quatY = Float.parseFloat(values[5]);
      float quatZ = Float.parseFloat(values[6]);
      long timeDiff = Long.parseLong(values[7]);
      int rssi = Integer.parseInt(values[8]);
      float snr = Float.parseFloat(values[9]);
      
      currentOrientation = new OrientationData(roll, pitch, yaw, 
                                               quatW, quatX, quatY, quatZ,
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
  
  ambientLight(60, 60, 80);
  directionalLight(100, 100, 120, -0.5, 0.5, -1);
  pointLight(80, 80, 100, cameraX/2, cameraY/2, cameraZ/2);
}

void draw3DScene() {
  if (showGrid) {
    drawGrid();
  }
  
  drawAxes();
  
  if (currentOrientation != null) {
    pushMatrix();
    
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
      fill(modelColor);
      stroke(255, 150);
      strokeWeight(0.5);
      shape(cubeSatModel);
    } else {
      drawFallbackCube();
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
  strokeWeight(3);
  
  stroke(axisColorX);
  line(0, 0, 0, 100, 0, 0);
  
  stroke(axisColorY);
  line(0, 0, 0, 0, 0, 100);
  
  stroke(axisColorZ);
  line(0, 0, 0, 0, -100, 0);
  
  fill(axisColorX);
  textSize(12);
  pushMatrix();
  translate(110, 0, 0);
  text("X", 0, 0);
  popMatrix();
  
  fill(axisColorY);
  pushMatrix();
  translate(0, 0, 110);
  text("Y", 0, 0);
  popMatrix();
  
  fill(axisColorZ);
  pushMatrix();
  translate(0, -110, 0);
  text("Z", 0, 0);
  popMatrix();
}

void drawFallbackCube() {
  fill(modelColor, 180);
  stroke(255);
  strokeWeight(1);
  box(80);
}

void drawOrientationTrail() {
  if (orientationHistory.size() < 2) return;
  
  stroke(255, 100, 100, 100);
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

void draw2DUI() {
  fill(0, 0, 0, 150);
  noStroke();
  rect(10, 10, 350, showDebugInfo ? 300 : 100);
  
  fill(255, 220, 100);
  textFont(titleFont);
  text("CubeSat 3D Orientation By Teerathap Yaisungnoen", 20, 40);
  
  textFont(uiFont);
  fill(serialPort != null ? color(100, 255, 100) : color(255, 100, 100));
  text("Serial: " + (serialPort != null ? "Connected (COM5)" : "Disconnected"), 20, 65);
  
  fill(255, 200, 100);
  text("FPS: " + nf(currentFPS, 1, 1) + " | Data Rate: " + packetsPerSecond + " Hz", 20, 85);
  
  if (showDebugInfo && currentOrientation != null) {
    fill(255);
    int yPos = 110;
    
    text("=== Orientation Data ===", 20, yPos);
    yPos += 20;
    
    text("Mode: " + (useQuaternion ? "Quaternion" : "Euler Angles"), 20, yPos);
    yPos += 15;
    text("Coordinate: Aerospace (X-Right, Y-Forward, Z-Up)", 20, yPos);
    yPos += 20;
    
    text("Roll:  " + nf(currentOrientation.roll, 1, 2) + "° (X-axis)", 20, yPos);
    yPos += 15;
    text("Pitch: " + nf(currentOrientation.pitch, 1, 2) + "° (Y-axis)", 20, yPos);
    yPos += 15;
    text("Yaw:   " + nf(currentOrientation.yaw, 1, 2) + "° (Z-axis)", 20, yPos);
    yPos += 20;
    
    text("Quat W: " + nf(currentOrientation.quatW, 1, 4), 20, yPos);
    yPos += 15;
    text("Quat X: " + nf(currentOrientation.quatX, 1, 4), 20, yPos);
    yPos += 15;
    text("Quat Y: " + nf(currentOrientation.quatY, 1, 4), 20, yPos);
    yPos += 15;
    text("Quat Z: " + nf(currentOrientation.quatZ, 1, 4), 20, yPos);
    yPos += 20;
    
    text("Signal RSSI: " + currentOrientation.rssi + " dBm", 20, yPos);
    yPos += 15;
    text("SNR: " + nf(currentOrientation.snr, 1, 2) + " dB", 20, yPos);
    yPos += 15;
    text("Interval: " + currentOrientation.timeDiff + " ms", 20, yPos);
    yPos += 20;
    
    text("Packets: " + packetsReceived, 20, yPos);
    yPos += 15;
    text("History: " + orientationHistory.size(), 20, yPos);
  }
  
  fill(200, 200, 255, 180);
  int helpY = height - 120;
  text("Controls:", 20, helpY);
  text("Mouse: Rotate camera | Scroll: Zoom", 20, helpY + 15);
  text("G: Grid | D: Debug | Q: Quat/Euler | F: Flip | R: Reset", 20, helpY + 30);
  text("Model: " + (modelLoaded ? "Loaded" : "Using fallback cube"), 20, helpY + 50);
}

void mouseDragged() {
  if (mouseButton == LEFT) {
    cameraAngleX += (mouseX - pmouseX) * 0.01;
    cameraAngleY -= (mouseY - pmouseY) * 0.01;
    cameraAngleY = constrain(cameraAngleY, -PI/2 + 0.1, PI/2 - 0.1);
  }
}

void mouseWheel(MouseEvent event) {
  cameraDistance += event.getCount() * 20;
  cameraDistance = constrain(cameraDistance, 100, 1500);
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
      saveFrame("cubesat-screenshot-####.png");
      println("Screenshot saved");
      break;
  }
}

void dispose() {
  if (serialPort != null) {
    serialPort.stop();
  }
  println("Application closed gracefully");
}
