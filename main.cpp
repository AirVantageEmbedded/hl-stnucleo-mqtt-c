/*******************************************************************************
 * Distributor Sample for Sierra Wireless' HL Serie Modules
 * This sample has been developped to show how a HL module can be used to communicate with Airvantage.
 * The main apllication is to detect if the ambiant ligth is too low then the lamp is switch on.
 * It can receive command(like Switch On or Off) and publish data to Airvantage. 
 * For more information, see the tutorial provided.
 * Implement only CONNECT,PING and PUBLISH MQTT protocol.
 * 
 *
 * Charles Renoux - May 2015
 *******************************************************************************/
 
#include "mbed.h"
#include "MQTTPublish.h"
#include "MQTTPacket.h"
#include "MQTTClient.h"

//------------------------------------
// Hyperterminal configuration
// 115200 bauds, 8-bit data, no parity
//------------------------------------

Serial pc(SERIAL_TX, SERIAL_RX);
DigitalOut led1(LED1);

DigitalOut dout0(D5);
DigitalOut dout1(D6);
DigitalOut dstartKompel(D4);
DigitalOut dresetKompel(D3);

DigitalIn din0(D7);

AnalogIn in0(A0);
AnalogIn in1(A1);

unsigned char mqtt_buffer[1024];

enum Mode {AUTO,OFF,ON};
enum PreviousState{FAIL,LIGHT_OFF,LIGHT_ON};


//Object that controls the lamp (i.e. switch ON/OFF)
class LedCmd 
{
        
    public:
        bool setOn()
        {
            dout0=1;
            return true;
        }
        bool setOff()
        {
            dout0=0;
            return true;
        }
};

//Object that manages the different mode of the application
class ModeManager
{
    private:
        Mode mode;
        bool modeJustChanged;
    public:
    
        ModeManager(){
            mode=AUTO;
            modeJustChanged=true;
            }
        void setMode(Mode pMode){
            mode=pMode;
            }  
        Mode getMode(){
            return mode;
            }   
        bool isModeJustChanged(){
            return modeJustChanged;
            }
        void setModeJustChanged(bool pModeChanged){
            modeJustChanged=pModeChanged;
            }  
};

//Object that detects if a failure has been detected  
class FailureDetector
{
    public:
    bool isFailureDetected()
    {
        bool state;
        if(din0 >= 1) 
        {
            state=true;
        } 
        else 
        {
            state=false;
        }
        return state;
    }
};

//Object that generates a random value of the lamp consumption   
class PowerConsumption
{
    public:

    
    float RandomFloat(float a, float b) 
    {
        float random = ((float) rand()) / (float) RAND_MAX;
        float diff = b - a;
        float r = random * diff;
        return a + r;   
    }
    
    float getPower()
    {
       return RandomFloat(55.0, 65.0); 
    }
        
};

//Object that detects if the ambiant light is lower than the threshold    
class LightDetector
{
    public:
    bool isLightUnderTrigger()
    {
        bool state;
        float meas;
        meas = in0.read(); // Converts and read the analog input value (value from 0.0 to 1.0)
        meas = meas * 3300; // Change the value to be in the 0 to 3300 range
        if(meas >=1000.0) 
        {
            state=false;
        } 
        else 
        {
            state=true;
        }
        return state;
    }
    
    float getLightSensorValue()
    {
       float meas;
       meas = in0.read(); // Converts and read the analog input value (value from 0.0 to 1.0)
       meas = meas * 3300; // Change the value to be in the 0 to 3300 range
       return meas; 
    }
    
};

ModeManager modeManager;

//Callback function that is called when a commend is receied
void callback(const char *key, const char *value) 
{
    //pc.printf("CallBack %s : %s\n",key, value);

    if(strstr(value,"modeoff")!=NULL)
    {
        pc.printf("CallBack modeOFF\n\r");
        modeManager.setMode(OFF);
        modeManager.setModeJustChanged(true); 
    }
    else if(strstr(value,"modeauto")!=NULL)
    {
        pc.printf("CallBack modeAUTO\n\r"); 
        modeManager.setMode(AUTO);
        modeManager.setModeJustChanged(true); 
    }
    else if(strstr(value,"modeon")!=NULL)
    {
        pc.printf("CallBack modeON\n\r"); 
        modeManager.setMode(ON);
        modeManager.setModeJustChanged(true); 
    }
}

void startKompel()
{
    /*dresetKompel = 1;
    wait(2);
    dresetKompel = 0;*/
    dstartKompel = 1;
    wait(5);
    dstartKompel = 0;       
}

void blinkPublish()
{
    int     i;
    for(i = 0; i < 2 ; i++)
    {
        dout1 = 1;
        wait(0.5);  
        dout1 = 0;
        wait(0.5);  
    } 
}


int main() 
{
    //Declaration of the object   
    LedCmd              ledcommander;
    LightDetector       lightDetector;
    FailureDetector     failureDetector;
    PowerConsumption    powerConsumption;
    PreviousState       previousState=LIGHT_OFF;
    Mode                previousMode=AUTO;
    bool    isFirstTime=true;
    float   power=0;
    float   lightSensor=0;
    char    str1[16];
    char    str2[16];
    char*   server="eu.airvantage.net";
    char*   apn="internet.maingate.se";
    int     port = 1883;
    char*   password="1234";
    
    dout1=0;
        
    //Step 1 : Initialize in AUTO Mode
    modeManager.setMode(AUTO);
    
    //Step 2 Configure the Serial connection(used to communication with the Kompel card)
    pc.baud(115200);
    
    //Step 3 Start the Kompel card
    startKompel();
    
    wait(2);
    //Step 4 :
    MQTTClient  client(server, &callback, password, port, &pc, apn);  
     
    while(1) 
    {      
        if(client.connected == true)
        {  
            //Green Light On
            dout1 = 1;
            if(modeManager.getMode() == AUTO)
            {
                    //Check first if the ambiant light is under the threshold
                    if(lightDetector.isLightUnderTrigger())
                    {
                          
                        wait(0.5);
                        //If under, check if there is a failure and publish just once
                        if(failureDetector.isFailureDetected())
                        {
                            if(previousState!=FAIL)
                            {
                                wait(0.1);
                                ledcommander.setOff();
                                wait(0.5);
                                sprintf(str1,"%f",0.0); 
                                lightSensor = lightDetector.getLightSensorValue();
                                sprintf(str2,"%f",lightSensor); 
                                client.pub("st.power",str1,"st.lightsensor",str2,"st.lightstatus","on","st.failuredetected","true");
                                blinkPublish();
                                previousState = FAIL;
                            }
                        }
                        //If under and no failure, publish just once
                        else
                        {
                            if(previousMode == OFF || previousState == LIGHT_OFF || previousState == FAIL || previousMode == ON)
                            {  
                                wait(0.1);
                                ledcommander.setOn();
                                wait(0.5);
                                power = powerConsumption.getPower();
                                sprintf(str1,"%f",power); 
                                lightSensor=lightDetector.getLightSensorValue();
                                sprintf(str2,"%f",lightSensor); 
                                client.pub("st.power",str1,"st.lightsensor",str2,"st.lightstatus","on","st.failuredetected","false");
                                blinkPublish();
                                modeManager.setModeJustChanged(false);  
                                previousState = LIGHT_ON;
                            }    
                        }
                        
                    
                    }
                    //if the ambiant light isn't under the threshold, the lamp switches off.
                    else
                    {
                        ledcommander.setOff(); 
                        if(isFirstTime == true || previousMode == OFF || previousState == LIGHT_ON || previousState == FAIL || previousMode == ON)
                        { 
                            wait(0.1);
                            sprintf(str1,"%f",0.0); 
                            lightSensor = lightDetector.getLightSensorValue();
                            sprintf(str2,"%f",lightSensor); 
                            client.pub("st.power",str1,"st.lightsensor",str2,"st.lightstatus","off","st.failuredetected","false");
                            blinkPublish();
                            modeManager.setModeJustChanged(false); 
                            previousState = LIGHT_OFF;
                            isFirstTime = false;
                        }
                    }
                    
                previousMode = AUTO;
    
                                    
            }
            //When a command Mode OFF is received, the application switches in the OFF mode. The lamp will be always off.
            else if(modeManager.getMode() == OFF)
            {
                    if(previousMode == AUTO || previousMode == ON)
                    {
                        ledcommander.setOff();
                        wait(0.1);
                        sprintf(str1,"%f",0.0); 
                        lightSensor=lightDetector.getLightSensorValue();
                        sprintf(str2,"%f",lightSensor); 
                        client.pub("st.power",str1,"st.lightsensor",str2,"st.lightstatus","off","st.failuredetected","false");
                        blinkPublish();
                        modeManager.setModeJustChanged(false); 
                    }
                    previousState = LIGHT_OFF;
                    previousMode = OFF;
            }
            //When a command Mode ON is received, the application switches in the ON mode. The lamp will be always on.
            else if(modeManager.getMode() == ON)
            {
                    if(previousMode == AUTO || previousMode == OFF)
                    {
                        ledcommander.setOn();
                        wait(0.1);
                        power = powerConsumption.getPower();
                        sprintf(str1,"%f",power); 
                        lightSensor = lightDetector.getLightSensorValue();
                        sprintf(str2,"%f",lightSensor); 
                        client.pub("st.power",str1,"st.lightsensor",str2,"st.lightstatus","on","st.failuredetected","false");
                        blinkPublish();
                        modeManager.setModeJustChanged(false); 
                    }
                    previousState = LIGHT_ON;
                    previousMode = ON;
            }   
        }
        //If no more connected, try to reconnect.
        else
        { 
            pc.printf("Deconnected\n"); 
            //Green Light OFF
            dout1=0;
            client.reconnect();
            previousState=LIGHT_OFF;
            previousMode=AUTO;
        }  
        //Call the background task (i.e. Ping task + Receiv task)
        client.loop();
        
    }//while(1)
}