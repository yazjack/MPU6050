//////////////////////////////////////////////////////////////
//Temperature correction and quick setup is still to be checked on controller unit
//////////////////////////////////////////////////////////////

/*
  ///////////////////////////////////////////////////////////////////////////////////////
  //Connections
  ///////////////////////////////////////////////////////////////////////////////////////

  Gyro - Arduino pro mini
  VCC  -  5V
  GND  -  GND
  SDA  -  A4
  SCL  -  A5

  Gyro sensitivity
  Full scale  FS_SEL=0  +-250  deg/s
            FS_SEL=1  +-500
            FS_SEL=2  +-1000
            FS_SEL=3  +-2000

  Sens scale  FS_SEL=0  131  value/deg //meaning the reading for deg traveld
            FS_SEL=1  65.5
            FS_SEL=2  32.8
            FS_SEL=3  16.4

  Accelerometer sensitivity
  Full scale  AFS_SEL=0  +-2  g
            AFS_SEL=1  +-4
            AFS_SEL=2  +-8
            AFS_SEL=3  +-16

  Sens scale  AFS_SEL=0  16.384  LSB/g
            AFS_SEL=1  8.192
            AFS_SEL=2  4.096
            AFS_SEL=3  2.048
  //////////////////////////////////////////////////////////////////////////////////////
*/

//Include I2C library
#include <Wire.h>

//Declaring some global variables
#define gyro_sens 250                                                    //Steup gyro sensitivity to 250/500/1000/2000 DPS
#define acc_sens 2                                                       //Setup accelerometer sensitivity to 2/4/8/16 g
#define gyro_scale                                                       //Placeholder for values calcualted by setup_mpu_6050_registers based on above. Translates full scale to sens scale
#define acc_scale                                                        //Placeholder for values calcualted by setup_mpu_6050_registers based on above. Translates full scale to sens scale
#define temperature                                                      //Placeholder for values from temperature sensor
#define lcd_loop_counter                                                 //Self explanatory
int gyro_x, gyro_y, gyro_z;                                              //Sens values for gyro sensor
long acc_x, acc_y, acc_z, acc_total_vector;                              //Sens values for acc sensor
long gyro_x_cal, gyro_y_cal, gyro_z_cal;                                 //Temporary placeholder for gyro calibration values
long loop_timer;                                                         //Placeholder for milis later on in the main loop
float angle_x, angle_y;                                                  //values reading and calculating travel distance in deg from raw gyro readings
float angle_y_acc, angle_x_acc;                                          //angle_x and y drift compensation
float angle_x_output, angle_y_output;                                    //values held after complementary filter
boolean set_gyro_angles;                                                 //boolean checking compensation

void setup() {
  Wire.begin();                                                        //Start I2C as master
  Serial.begin(57600);                                                 //Use only for debugging
  pinMode(13, OUTPUT);                                                 //Set output 13 (LED) as output

  setup_mpu_6050_registers();                                          //Setup the registers of the MPU-6050 (500dfs / +/-8g) and start the gyro

  digitalWrite(13, HIGH);                                              //Set digital output 13 high to indicate startup

  for (int cal_int = 0; cal_int < 2000 ; cal_int ++) {                 //Run this code 2000 times
    read_mpu_6050_data();                                              //Read the raw acc and gyro data from the MPU-6050
    gyro_x_cal += gyro_x;                                              //Add the gyro x-axis offset to the gyro_x_cal variable
    gyro_y_cal += gyro_y;                                              //Add the gyro y-axis offset to the gyro_y_cal variable
    gyro_z_cal += gyro_z;                                              //Add the gyro z-axis offset to the gyro_z_cal variable
    delay(3);                                                          //Delay 3us to simulate the 250Hz program loop
  }

  gyro_x_cal /= 2000;                                                  //Divide the gyro_x_cal variable by 2000 to get the avarage offset
  gyro_y_cal /= 2000;                                                  //Divide the gyro_y_cal variable by 2000 to get the avarage offset
  gyro_z_cal /= 2000;                                                  //Divide the gyro_z_cal variable by 2000 to get the avarage offset

  digitalWrite(13, LOW);                                               //All done, turn the LED off

  loop_timer = micros();                                               //Reset the loop timer
}

void loop() {
  read_mpu_6050_data();                                                //Read the raw acc and gyro data from the MPU-6050

  gyro_x -= gyro_x_cal;                                                //Subtract the offset calibration value from the raw gyro_x value
  gyro_y -= gyro_y_cal;                                                //Subtract the offset calibration value from the raw gyro_y value
  gyro_z -= gyro_z_cal;                                                //Subtract the offset calibration value from the raw gyro_z value

  temp_adj();                                                          //Use tmep_adj function to include temperature corretion

  //Gyro angle calculations
  //0.0000611 = 1 / (250Hz / 65.5)                                     //Refresh rate of 250Hz
  angle_x += gyro_x * 1 / 250 / gyro_scale;                            //Calculate the traveled pitch angle and add this to the angle_pitch variable
  angle_y += gyro_y * 1 / 250 / gyro_scale;                            //Calculate the traveled roll angle and add this to the angle_roll variable

  //0.000001066 = 0.0000611 * (3.142(PI) / 180degr) The Arduino sin function is in radians
  angle_x += angle_y * sin(gyro_z * 1 / 250 / gyro_scale * 3.142 / 180);               //If the IMU has yawed transfer the roll angle to the pitch angel
  angle_y -= angle_x * sin(gyro_z * 1 / 250 / gyro_scale * 3.142 / 180);               //If the IMU has yawed transfer the pitch angle to the roll angel

  //To solve the drift problem
  //Use accelerometer to compensete the drift

  //Accelerometer angle calculations
  acc_total_vector = sqrt((acc_x * acc_x) + (acc_y * acc_y) + (acc_z * acc_z)); //Calculate the total accelerometer vector

  //57.296 = 1 / (3.142 / 180) The Arduino asin function is in radians so we convert it to deg
  angle_x_acc = asin((float)acc_y / acc_total_vector) * 57.296;     //Calculate the pitch angle
  angle_y_acc = asin((float)acc_x / acc_total_vector) * -57.296;    //Calculate the roll angle

  //Place the MPU-6050 spirit level and note the values in the following two lines for calibration
  angle_x_acc -= 0.0;                                              //Accelerometer calibration value for pitch
  angle_y_acc -= 0.0;                                               //Accelerometer calibration value for roll

  if (set_gyro_angles) {                                               //If the IMU is already started
    angle_x = angle_x * 0.9996 + angle_x_acc * 0.0004;     //Correct the drift of the gyro pitch angle with the accelerometer pitch angle
    angle_y = angle_y * 0.9996 + angle_y_acc * 0.0004;        //Correct the drift of the gyro roll angle with the accelerometer roll angle
  } else {                                                               //At first start
    angle_x = angle_x_acc;                                     //Set the gyro pitch angle equal to the accelerometer pitch angle
    angle_y = angle_y_acc;                                       //Set the gyro roll angle equal to the accelerometer roll angle
    set_gyro_angles = true;                                            //Set the IMU started flag
  }

  //To dampen the pitch and roll angles a complementary filter is used
  angle_x_output = angle_x_output * 0.9 + angle_x * 0.1;   //Take 90% of the output pitch value and add 10% of the raw pitch value
  angle_y_output = angle_y_output * 0.9 + angle_y * 0.1;   //Take 90% of the output roll value and add 10% of the raw roll value

  Serial.print("Pitch: ");
  Serial.println(angle_x_output);
  Serial.print("Roll: ");
  Serial.println(angle_y_output);

  while (micros() - loop_timer < 4000);                                //Wait until the loop_timer reaches 4000us (250Hz) before starting the next loop
  loop_timer = micros();                                               //Reset the loop timer
}


void read_mpu_6050_data() {                                            //Subroutine for reading the raw gyro and accelerometer data
  Wire.beginTransmission(0x68);                                        //Start communicating with the MPU-6050
  Wire.write(0x3B);                                                    //Send the requested starting register
  Wire.endTransmission();                                              //End the transmission
  Wire.requestFrom(0x68, 14);                                          //Request 14 bytes from the MPU-6050
  while (Wire.available() < 14);                                       //Wait until all the bytes are received
  acc_x = Wire.read() << 8 | Wire.read();                              //Add the low and high byte to the acc_x variable
  acc_y = Wire.read() << 8 | Wire.read();                              //Add the low and high byte to the acc_y variable
  acc_z = Wire.read() << 8 | Wire.read();                              //Add the low and high byte to the acc_z variable
  temperature = Wire.read() << 8 | Wire.read();                        //Add the low and high byte to the temperature variable
  gyro_x = Wire.read() << 8 | Wire.read();                             //Add the low and high byte to the gyro_x variable
  gyro_y = Wire.read() << 8 | Wire.read();                             //Add the low and high byte to the gyro_y variable
  gyro_z = Wire.read() << 8 | Wire.read();                             //Add the low and high byte to the gyro_z variable
}

void setup_mpu_6050_registers() {
  //Activate the MPU-6050
  Wire.beginTransmission(0x68);                                        //Start communicating with the MPU-6050
  Wire.write(0x6B);                                                    //Send the requested starting register
  Wire.write(0x00);                                                    //Set the requested starting register
  Wire.endTransmission();                                              //End the transmission

  //Configure the accelerometer (+/-8g)
  switch (acc_sens) {
    case 2:
      {
        Wire.beginTransmission(0x68);                                        //Start communicating with the MPU-6050
        Wire.write(0x1C);                                                    //Send the requested starting register
        Wire.write(0x00);                                                    //Set the requested starting register
        Wire.endTransmission();                                              //End the transmission
        acc_scale = 16.384;
      }
      break;
    case 4:
      {
        Wire.beginTransmission(0x68);                                        //Start communicating with the MPU-6050
        Wire.write(0x1C);                                                    //Send the requested starting register
        Wire.write(0x01);                                                    //Set the requested starting register
        Wire.endTransmission();                                              //End the transmission
        acc_scale = 8.192;
      }
      break;
    case 8:
      {
        Wire.beginTransmission(0x68);                                        //Start communicating with the MPU-6050
        Wire.write(0x1C);                                                    //Send the requested starting register
        Wire.write(0x10);                                                    //Set the requested starting register
        Wire.endTransmission();                                              //End the transmission
        acc_scale = 4.096;
      }
      break;
    case 16:
      {
        Wire.beginTransmission(0x68);                                        //Start communicating with the MPU-6050
        Wire.write(0x1C);                                                    //Send the requested starting register
        Wire.write(0x11);                                                    //Set the requested starting register
        Wire.endTransmission();                                              //End the transmission
        acc_scale = 2.048;
      }
      break;
    default:
      break;
  }

  //Configure the gyro (500dps full scale)
  switch (gyro_sens) {
    case 250:
      {
        Wire.beginTransmission(0x68);                                        //Start communicating with the MPU-6050
        Wire.write(0x1B);                                                    //Send the requested starting register
        Wire.write(0x00);                                                    //Set the requested starting register
        Wire.endTransmission();                                              //End the transmission
        gyro_scale = 131;
      }
      break;
    case 500:
      {
        Wire.beginTransmission(0x68);                                        //Start communicating with the MPU-6050
        Wire.write(0x1B);                                                    //Send the requested starting register
        Wire.write(0x01);                                                    //Set the requested starting register
        Wire.endTransmission();                                              //End the transmission
        gyro_scale = 65.5;
      }
      break;
    case 1000:
      {
        Wire.beginTransmission(0x68);                                        //Start communicating with the MPU-6050
        Wire.write(0x1B);                                                    //Send the requested starting register
        Wire.write(0x10);                                                    //Set the requested starting register
        Wire.endTransmission();                                              //End the transmission
        gyro_scale = 32.8;
      }
      break;
    case 2000:
      {
        Wire.beginTransmission(0x68);                                        //Start communicating with the MPU-6050
        Wire.write(0x1B);                                                    //Send the requested starting register
        Wire.write(0x11);                                                    //Set the requested starting register
        Wire.endTransmission();                                              //End the transmission
        gyro_scale = 16.4;
      }
      break;
    default:
      {
      }
      break;
  }
}

//Temperature adjustment
void temp_adj() {
  if (temperature > 25) {
    acc_x = acc_x + acc_x * (temperature - 25) * 0.02;
    acc_y = acc_y + acc_y * (temperature - 25) * 0.02;
    acc_z = acc_z + acc_z * (temperature - 25) * 0.02;
    gyro_x = gyro_x + gyro_x * (temperature - 25) * 0.02;
    gyro_y = gyro_y + gyro_y * (temperature - 25) * 0.02;
    gyro_z = gyro_z + gyro_z * (temperature - 25) * 0.02;
  } else if (temperature == 25) {
    acc_x = acc_x;
    acc_y = acc_y;
    acc_z = acc_z;
    gyro_x = gyro_x;
    gyro_y = gyro_y;
    gyro_z = gyro_z;
  } else {
    acc_x = acc_x - acc_x * (25 - temperature) * 0.02;
    acc_y = acc_y - acc_y * (25 - temperature) * 0.02;
    acc_z = acc_z - acc_z * (25 - temperature) * 0.02;
    gyro_x = gyro_x - gyro_x * (25 - temperature) * 0.02;
    gyro_y = gyro_y - gyro_y * (25 - temperature) * 0.02;
    gyro_z = gyro_z - gyro_z * (25 - temperature) * 0.02;
  }
}
