#include <Servo.h>

#define IN1 3
#define IN2 2
#define IN3 4
#define IN4 5

#define trig_pin 8
#define echo_pin 9

int distance;
int turnDir;

Servo myservo;

// Distance thresholds for obstacle detection
#define MIN_DISTANCE_BACK 30

void setup() {
  Serial.begin(9600);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  pinMode(trig_pin, OUTPUT);
  pinMode(echo_pin, INPUT);

  myservo.attach(10);
}

void loop() {
  myservo.write(90);
  distance = checkDistance();
  delay(500);

  if (distance >= MIN_DISTANCE_BACK) { 
    forward(); 
    Serial.println("forward");
  } else {
    stop();
    delay(500);
    turnDir = checkDirection();
    Serial.println(turnDir);

    switch(turnDir) {
      case 1: // Turn left
        Serial.println("Left");
        turnLeft();
        delay(425);
        stop();
        myservo.write(90); // Adjust servo to look forward
        delay(500);
        forward();
        break;
      case 2: // Turn right
        Serial.println("Right");
        turnRight();
        delay(415);
        stop();
        myservo.write(90); // Adjust servo to look forward
        delay(500);
        forward();
        break;   
      case 4: // Move backward and recheck
        Serial.println("Backward");
        backward();
        delay(415);
        stop();
        turnDir = recheckDirection();
        if (turnDir == 1) {
          Serial.println("Rechecking - Turning left");
          turnLeft();
          delay(425);
        } else if (turnDir == 2) {
          Serial.println("Rechecking - Turning right");
          turnRight();
          delay(415);
        }
        stop();
        myservo.write(90); // Adjust servo to look forward
        delay(500);
        forward();
        break; 
      default:
        stop();
        break;
    }
  }
}

int checkDirection() {
  int leftDistance, rightDistance, frontDistance;
  
  // Check left direction
  myservo.write(180);
  delay(1000);
  leftDistance = checkDistance();
  if (leftDistance >= MIN_DISTANCE_BACK) return 1; // Turn left

  // Check front direction
  myservo.write(90);
  delay(500);
  frontDistance = checkDistance();
  if (frontDistance >= MIN_DISTANCE_BACK) return 3; // Move forward

  // Check right direction
  myservo.write(0);
  delay(1000);
  rightDistance = checkDistance();
  if (rightDistance >= MIN_DISTANCE_BACK) return 2; // Turn right
  
  
  // If no direction is clear, return 4 (move backward)
  return 4;
}

int recheckDirection() {
  int leftDistance, rightDistance;

  // Check left direction
  myservo.write(180);
  delay(1000);
  leftDistance = checkDistance();

  // Check right direction
  myservo.write(0);
  delay(1000);
  rightDistance = checkDistance();
  
  if (leftDistance >= MIN_DISTANCE_BACK) return 1; // Turn left
  if (rightDistance >= MIN_DISTANCE_BACK) return 2; // Turn right
  
  // If no direction is clear, return 4 (move backward)
  return 4;
}

void forward() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void backward() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void turnLeft() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void turnRight() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

void stop() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

int checkDistance() {
  long duration, distance;
  digitalWrite(trig_pin, LOW);
  delayMicroseconds(2);
  digitalWrite(trig_pin, HIGH);
  delayMicroseconds(10);

  duration = pulseIn(echo_pin, HIGH);
  distance = (duration / 2) / 29.1;

  return distance;
}
