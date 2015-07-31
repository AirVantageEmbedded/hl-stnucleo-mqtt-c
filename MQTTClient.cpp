/*******************************************************************************
 * MQTT Client class for Sierra Wireless' HL Serie Modules
 * Develloped for the distribution Kit with ST NUCLEO and KOMPEL card for HL modules.
 * Implement only CONNECT,PING and PUBLISH MQTT protocol.
 * 
 *
 * Charles Renoux - May 2015
 *******************************************************************************/
 
#include "MQTTClient.h"
#include "MQTTPublish.h"
#include "MQTTPacket.h"
#include "mbed.h"


MQTTClient::MQTTClient(char *server, void (*callback)(const char *, const char*), char *password,int port, Serial* pc, char* apn):
_server(server),callback(callback),_password(password),_port(port),_pc(pc),_apn(apn)
{
       
    // variable to check if the HL modem is registerd on the network
    registred = false;
    
    // variable to check if the signal is enough powerful
    signalPower = false;
    
    // variable to check if it's a HL6 or HL8
    HL8 = false;
    
    // pingCounter set to zero, if 2 pings are missed->reboot
    pingCounter = 0;

    // Setup a serial interrupt function to receive data
    _pc->attach(this,&MQTTClient::Rx_interrupt, Serial::RxIrq);
    
    //index of the RX buffer(where all the data received from the Kompel card is stocked)
    rx_in = 0;

    init();
}

MQTTClient::~MQTTClient()
{
    delete(_topic);  
    delete(_id); 
}
    
// Interupt Routine to read in data from serial port
void MQTTClient::Rx_interrupt() 
{
// Loop just in case more than one character is in UART's receive FIFO buffer
   while ((_pc->readable()) ) 
   {
        rx_buffer[rx_in] = _pc->getc();
        
        rx_in = (rx_in + 1) % BUFFER_SIZE;
    }    
    
    return;
}
//Generic function to check a response waited
bool MQTTClient::isResponseExpected(char* Response, int waitTime)
{
    bool    ret = false;
    
    char*   pch;

    wait(waitTime);
    
    pch = strstr(&rx_buffer[0],Response);
    
    _pc->printf("Response OK %d\n\r", pch);
    
    if(pch != NULL)
    {
        ret = true;
    }  
    
    rx_in = 0;
    
    return ret;

}

bool MQTTClient::getIMEI()
{
    bool    ret = false;
    
    char*   pch;
    char*   pchEOC;
    int     length;
    
    _pc->printf("AT+KGSN=0\r\n"); 
    
    wait(1);

    pch = strstr(&rx_buffer[0], "+KGSN: ");

    if (pch != NULL)
    {
        pch = pch + strlen("+KGSN: ");
        
        pchEOC = strstr(pch, "\r\n");
        
        if(pchEOC != NULL)
        {
           
           length = pchEOC - pch;
                      
           memcpy(_id, pch, length);
           memcpy(_username, pch, length);
           
           _id[length]='\0';
           _username[length]='\0';
           
           _pc->printf("IMEI: %s\r\n",_id); 
           _pc->printf("IMEI: %s\r\n",_username); 
           
           ret = true;
        
        }

    } 
    
    return ret; 
    
}
//check the Notification, if true the HL is connected 
bool MQTTClient::connectionOK()
{   
    bool    ret = false;
    
    char*   pch;
    
    pch = strstr(&rx_buffer[0],"+KTCP_IND: 1,1");
    
    _pc->printf("Connection OK %d\n\r", pch);
    
    if(pch != NULL)
    {
        ret = true;
    }
      
    return ret;
}

bool MQTTClient::configuration()
{
    bool    ret = false;
        
    rx_in = 0;
    
    //Configure the address to be connected + set the APN (HL6 or HL8)    
    if(HL8 == true)
    {
        _pc->printf("AT+KTCPCFG=1,0,\"%s\",%d\r\n",_server,_port); 
        //wait(1);
        if(isResponseExpected("ERROR", 1) == true)
        {
            reconnect();
        }
        //Configure APN for HL8
        _pc->printf("AT+KCNXCFG=1,\"GPRS\",\"%s\",\"\",\"\",IPV4,\"0.0.0.0\"\r\n",_apn);
        
        if(isResponseExpected("ERROR", 1) == true)
        {
            reconnect();
        }
    }
    else
    {    

        _pc->printf("AT+KTCPCFG=0,0,\"%s\",%d\r\n",_server,_port); 
        
        if(isResponseExpected("ERROR", 1) == true)
        {
            reconnect();
        }

        //Configure APN for HL6

        _pc->printf("AT+KCNXCFG=0,\"GPRS\",\"%s\",\"\",\"\",\"0.0.0.0\"\r\n",_apn);
        
        if(isResponseExpected("ERROR", 1) == true)
        {
            reconnect();
        }
    }
    
    wait(1);
    
              
    return ret;

}

//Check the registration
bool MQTTClient::isRegistred()
{
    char* pch = NULL;
    
    rx_in = 0;
    
    _pc->printf("AT+CGREG?\r\n");
    
    wait(1);
    
    pch = strstr(&rx_buffer[0], ",");

    if (pch != NULL)
    {
        pch++;
        if (*pch == '1' || *pch == '5')//1 for registred, homme network. 5 for registred ,roaming
        {
            registred = true;
            return true;   //ready to connect. CGREG equals to 1 (home) nor 5 (roaming)
        }
        else if(*pch == '2')//2, not registred 
        {
            NVIC_SystemReset();
        }
    }

    return false;   //not attached to GPRS yet
}

//Test the power of the signal
bool MQTTClient::isSignalEnough()
{
    char*   pch = NULL;
    int     value = 0;
    
    rx_in = 0;
    
    _pc->printf("AT+CSQ\r\n");
    
    wait(1);

    pch = strstr(&rx_buffer[0], "+CSQ: ");

    if (pch != NULL)
    {
        value = atoi(pch+strlen("+CSQ: "));
        if(value>=6)
        {
            signalPower = true;
            return true;
        }
    }

    return false;   //not attached to GPRS yet  

}

//Configure the HL to be ready to send data
int MQTTClient::init() 
{
    bool    ret = false;
    
    //Check if the SIM is well inserted
    _pc->printf("AT+CPIN?\r\n");
     
    if(isResponseExpected("+CPIN: READY",1))
    {
       _pc->printf("CPIN OK\r\n"); 
    }
    
    //Destect if it's a HL6 or HL8
    _pc->printf("ATI3\r\n");
    
    if(isResponseExpected("HL8",1))
    {
       HL8=true; 
    }
    
    //Close the TCP connection if any 
    _pc->printf("AT+KTCPCLOSE=1,1\r\n");
        
    wait(1);
    //Clean the HL configuration if the ST Nucleo has been rebooted 
    _pc->printf("AT+KTCPDEL=1\r\n");
   
    wait(1);
    
    getIMEI();
    
    _topic = new char[strlen(_username) + 15];
    
    strcpy(_topic, _username);
    
    strcat(_topic, "/messages/json");
    
    //The application will not be launched until the signal is enough powerful    
    do{signalPower=false;} while(isSignalEnough() != true);
    
    //The application will not be launched until the HL is not registered to a network  
    do{} while(isRegistred() != true);
    
    //configuration();
    
    //Configure the address to be connected + set the APN (HL6 or HL8)    
    if(HL8 == true)
    {
        _pc->printf("AT+KTCPCFG=1,0,\"%s\",%d\r\n",_server,_port); 
        wait(1);
        //Configure APN for HL8
        _pc->printf("AT+KCNXCFG=1,\"GPRS\",\"%s\",\"\",\"\",IPV4,\"0.0.0.0\"\r\n",_apn);
    }
    else
    {      
        _pc->printf("AT+KTCPCFG=0,0,\"%s\",%d\r\n",_server,_port); 
        wait(1);
        //Configure APN for HL6
        _pc->printf("AT+KCNXCFG=0,\"GPRS\",\"%s\",\"\",\"\",\"0.0.0.0\"\r\n",_apn);
    }
    
    wait(1);
    
    //Launch the TCP connection
    _pc->printf("AT+KTCPCNX=1\r\n");
    
    wait(5);
    
    do
    {
        ret = connectionOK();
        
        wait(1);  
    }
    while(ret == false && HL8 == true);
    
    //Start the timer for the ping
    timer.start();
    
    return connect();
}

//Clean the data received in the RX buffer
void MQTTClient::cleanTCPData(char* pch,int size)
{
    int     sizeOfPatternConnect = strlen("CONNECT\r\n");
    int     sizeOfPatternEOF = strlen("--EOF--Pattern--");
    
    memset(pch-sizeOfPatternConnect,0,sizeOfPatternConnect+size+sizeOfPatternEOF);
}

//Send the CONNECT message in MQTT to the server 
int MQTTClient::connect() 
{   
    int     ret = 0;
    static MQTTPacket_connectData options = MQTTPacket_connectData_initializer;
    options.MQTTVersion = 3;
    options.clientID.lenstring.len = 0;
    options.clientID.cstring = (char*) _id;
    options.password.lenstring.len = 0;
    options.password.cstring = (char*) _password;
    options.username.lenstring.len = 0;
    options.username.cstring = (char*) _username;
    options.keepAliveInterval = 30;
    options.willFlag = 0;
    
    int     size = MQTTSerialize_connect(mqtt_buffer, 65536, &options);     
    
    writeTCPData(size);
        
    wait(2);
    
    rx_in = 0;
    
    ret = recvData();
    
    wait(1); 
       
    return ret;   
}

//Send TCP Data
int MQTTClient::writeTCPData(int sizeOfData)
{
    int     totalLength = 0;
    int     i;
    
    // Must add this pattern (i.e. "\r\n\r\n") to each packet sent 
    mqtt_buffer[sizeOfData]='\\';
    mqtt_buffer[sizeOfData+1]='r';
    mqtt_buffer[sizeOfData+2]='\\';
    mqtt_buffer[sizeOfData+3]='n';
    mqtt_buffer[sizeOfData+4]='\\';
    mqtt_buffer[sizeOfData+5]='r';
    mqtt_buffer[sizeOfData+6]='\\';
    mqtt_buffer[sizeOfData+7]='n';
    
    //Calculate the total size of Data to be sent    
    totalLength = sizeOfData + strlen("\r\n\r\n");
    
    // Send the command to allow the transmission   
    _pc->printf("AT+KTCPSND=1,%d\r\n",totalLength);
    
    // Temporisation are needed for sending data
    wait(2);
    
    for( i = 0 ; i < totalLength; i++)
    {
        _pc->putc(mqtt_buffer[i]);
    }
    
    //Once all the data are sent, add this pattern to finish the transmission
    _pc->printf("--EOF--Pattern--");
    
    wait(1); 
    
    return totalLength;  
}

//Publish just one information the the server with timestamp
int MQTTClient::pub(char *key, char *value)
{
    MQTTString  channel = MQTTString_initializer;
    channel.lenstring.len = 0;
    //allocate 15 bytes for the deviceId + 15 bytes for the rest of the path i.e. "/messages/json"
    char        channelName[30];
    channel.cstring = (char*)&channelName;
    sprintf(channel.cstring, "%s/messages/json",_id);
      
    int         key_length = strlen(key);
    int         value_length = strlen(value);
    char        payload[key_length + value_length + 36];
    
    //this timestamp is just for the example 
    strcpy(payload, "{\"1349907137000\": { \"");
    strcat(payload, key); 
    strcat(payload, "\":");   
    strcat(payload, value);   
    strcat(payload, "}}");
    
    int         length = MQTTSerialize_publish(mqtt_buffer, 65536, 0, 0, 0, 0, channel, (unsigned char*)payload, (unsigned)strlen(payload));    
        
    writeTCPData(length);

    return 0;
}

//This function is used to publish 4 differents values in the same time. No timestamp.
int MQTTClient::pub(char *key1, char *value1,char *key2, char *value2,char *key3, char *value3, char *key4, char *value4)
{
    MQTTString channel = MQTTString_initializer;
    channel.lenstring.len = 0;
    //allocate 15 bytes for the deviceId + 15 bytes for the rest of the path i.e. "/messages/json"
    char channelName[30];
    channel.cstring = (char*)&channelName;
    sprintf(channel.cstring, "%s/messages/json",_id);
      
    int key_length1 = strlen(key1);
    int value_length1 = strlen(value1);
    int key_length2 = strlen(key2);
    int value_length2 = strlen(value1);
    int key_length3 = strlen(key3);
    int value_length3 = strlen(value1);
    int key_length4 = strlen(key4);
    int value_length4 = strlen(value4);
    char payload[key_length1 + value_length1 + key_length2 + value_length2 + key_length3 + value_length3 + key_length4 + value_length4 + 36];
    strcpy(payload, "{ \"");
    strcat(payload, key1);
    strcat(payload, "\":");
    strcat(payload, value1);
    strcat(payload, ",\"");
    strcat(payload, key2);
    strcat(payload, "\":");
    strcat(payload, value2);
    strcat(payload, ",\"");
    strcat(payload, key3);
    strcat(payload, "\":\"");
    strcat(payload, value3);
    strcat(payload, "\",\"");
    strcat(payload, key4);
    strcat(payload, "\":");
    strcat(payload, value4);
    strcat(payload, "}");
    
    int length = MQTTSerialize_publish(mqtt_buffer, 65536, 0, 0, 0, 0, channel, (unsigned char*)payload, (unsigned)strlen(payload));    

    writeTCPData(length);

    return 0;
}

//This functiun is used to decode the MQTT message received
void MQTTClient::decodTCPData(int type, int sizeOfData)
{
    char*   pch;
    char*   pchUid;

    //Search the "CONNECT" pattern in the RX buffer
    pch = strstr(&rx_buffer[0],"CONNECT");

    if(pch != NULL)
    {
        pch = pch+strlen("CONNECT\r\n");
        memcpy(msgReceiv, pch, sizeOfData);
        msgReceiv[sizeOfData] = '\0';
               
        //CONNACK
        if(type == CONNACK_MSG)
        {
            _pc->printf("CONNACK with return %d\n\r",msgReceiv[1]);//return the error code from the server.
            
            if(msgReceiv[1]==0)
            {
                connected = true; 
            }  
            else
            {          
                _pc->printf("Problem with the server\n\r");
            }
            
        }
        //PINGRESP
        else if(type == PINGRESP_MSG)
        {
            _pc->printf("PINGRESP\n\r");  
            pingCounter-=1;   
        }
        //PUBACK 
        else if(type == PUBACK_MSG)
        {
            _pc->printf("PUBACK\n\r");       
        }
        //PUBLISH
        else if(type == PUBLISH_MSG)
        {
            _pc->printf("PUBLISH mesg\n\r"); 
            
            pchUid = strstr(&msgReceiv[6],"uid");
            
           if(pchUid!=NULL)
           {
                //send the message received to the application to decode the command(i.e. mode AUTO, mode OFF, mode ON)
                call_callback(_username, pchUid);
           }
                 
        }
         
        wait(1);
                  
        //Clean the data receive in the RX buffer              
        cleanTCPData(pch,sizeOfData);
        
        //reset the RX buffer index
        rx_in=0;
    }  
}

int MQTTClient::recvData()
{
    char*   pch;
    char*   pchEOF;
    int     size = 0;
    int     type=0;

    //reset the RX buffer index
    rx_in=0;
    
    //Check periodically the first byte of the message received. 
    _pc->printf("AT+KTCPRCV=1,1\r\n"); 
    
    wait(1);
    
    pch = strstr(&rx_buffer[0],"CONNECT");
    
    pchEOF = strstr(&rx_buffer[0],"--EOF--Pattern--");
    
    //Determine if the message is empty or not. if not we determine the type of message(PUBLISH,CONNACK,PINGRESP)
    if(pch != NULL && pchEOF != NULL)
    {
        pch = pch+strlen("CONNECT\r\n");
        
        memcpy(msgReceiv, pch, 1);
        
        msgReceiv[1]= '\0';
        
        type = msgReceiv[0];
        
        size = pchEOF - pch;     
    }

    wait(1);
    
    //If there is a message
    if(size>0)
    {
        //If it's not a PINGRESP message
        if(type != PINGRESP_MSG)
        {
            //Clean the RX buffer
            cleanTCPData(pch,size);
        
            //reset the RX buffer index
            rx_in=0;
            
            //Receive the next byte to extract the size of the message
            _pc->printf("AT+KTCPRCV=1,1\r\n"); 
        
            wait(1);
        
            pch = strstr(&rx_buffer[0],"CONNECT");
        
            do
            {    
                pchEOF = strstr(&rx_buffer[0],"--EOF--Pattern--");
                
                wait(1);
            }
            while(pchEOF == NULL);
        
        
            if(pch != NULL && pchEOF!=NULL)
            {
                pch = pch + strlen("CONNECT\r\n");
                
                memcpy(msgReceiv, pch, 1);
                
                msgReceiv[1] = '\0';

                //Clean the RX buffer
                cleanTCPData(pch,size);
                
                size = pchEOF - pch;
                  
                // reset the RX buffer index              
                rx_in=0;
                
                size=msgReceiv[0]+1;
                
                // Ask the rest of the message
                _pc->printf("AT+KTCPRCV=1,%d\r\n",size); 
                
                wait(1);
                }     
        }
    }
    
    // If the message is not empty, decode it
    if(type>0)
    {
        decodTCPData(type,size);
    }
    
    return 0;
}

// Send the PING message
void MQTTClient::sendPing()
{
    int     length = MQTTSerialize_pingreq(mqtt_buffer, 1024);

    writeTCPData(length);
    
    pingCounter+=1;
    
}

// Loop function used to send pings and handle the reception of data
void MQTTClient::loop()
{
        
    if(pingCounter>2)
    {  
        NVIC_SystemReset(); 
    }
    else
    {
        if(timer.read() > PERIOD_PING)
        {
            sendPing();
            
            timer.reset();
            
            timer.start();
        }
        
        recvData();
    }
    
} 

// Function used to check if the connection hasn't been closed 
bool MQTTClient::isDisconnected()
{

    char*   pch_offset;
    bool    ret = false;
    
    pch_offset = &rx_buffer[0];

    rx_in=0;

    while(pch_offset != NULL)
    {
        
        pch_offset = strstr(pch_offset,"+KTCNX_IND: 1,3");
        if(pch_offset!=NULL)
        {
            ret = true;
        }
    }
    return ret;
}

// Reconnect the module 
void MQTTClient::reconnect() 
{    
    connected = false; 
    memset(&rx_buffer[0],0,BUFFER_SIZE);
    init();
}

void MQTTClient::call_callback(const char *topic, const char *message) 
{
    callback(topic, message);
}
