#ifndef MQTTCLIENT_H_
#define MQTTCLIENT_H_

#include "mbed.h"


#define KEEP_ALIVE 100 // seconds
#define TIMEOUT 1000 // ms
#define BUFFER_SIZE 1024

#define START_THREAD 1

#define PERIOD_PING 15.0
#define PERIOD_RECEIV 5.0

#define PINGRESP_MSG 208
#define CONNACK_MSG 32
#define PUBLISH_MSG 48
#define PUBACK_MSG 64
#define PINGRESP_MSG 208


class MQTTClient {
    public:
        /** Initialise and launch the MQTT Client
         * \param server the address of your server
         * \param callback a callback to execute on receiving a PUBLISH
         * \param port the port of your server
         * \param password your password for the server
         * \param pc pointer of the Serail object for transmitting and receiving data
         * \param apn APN of the SIM card
         */
        MQTTClient(char *server,void (*callback)(MQTTClient*, const char *, const char*));
        MQTTClient(char *server, void (*callback)(MQTTClient*, const char *, const char*), char *password, int port, Serial* pc, char* apn);
        void            reconnect();
        virtual         ~MQTTClient();

        /* Publish a message on a topic
         * \param topic the topic
         * \param message the message
         */
        int             pub(char *key, char *key);
        /*publish for four values*/
        int             pub(char *key1, char *value1,char *key2, char *value2,char *key3, char *value3, char *key4, char *value4);

        /* Acknowledge a command */
        int             ack(char* ticketId);
        
        void            loop();
        
        int             disconnect();
              
        bool            connected;
        bool            registred;
        bool            signalPower;

    protected:
        char*           _server;
        int             _port;
        char            _id[16];
        void            (*callback)(MQTTClient*, const char *, const char*);
        char            _username[16];
        char*           _password;
        char*           _topic;
        unsigned char   mqtt_buffer[BUFFER_SIZE];
        Serial*         _pc;
        char*           receivedBuffer;
        // might need to increase buffer size for high baud rates
        char            rx_buffer[BUFFER_SIZE];
        // volatile makes read-modify-write atomic 
        volatile int    rx_in;
        char            msgReceiv[256];
        Timer           timer;
        bool            HL8;
        char*           _apn;
        int             pingCounter;
        

        
    private:

        int             init();
        int             connect();
        int             recvData();
        bool            getIMEI();
        void            Rx_interrupt();
        bool            connectionOK();
        bool            configuration();
        int             PublishReq(
                            char* mesgReceiv, 
                            int sizeOfData);
        void            sendPing();
        bool            isStillConnected();
        bool            isDisconnected();
        bool            isSignalEnough();
        bool            isRegistred();         
        bool            isResponseExpected(
                            char* Response, 
                            int waitTime);
        bool            isPublishing;
        virtual void    call_callback(
                            const char *topic, 
                            const char *message);
        int             writeTCPData(
                            int sizeOfBuffer);
        void            decodTCPData(
                            int type, 
                            int sizeOfData);
        void            cleanTCPData(
                            char* pch,
                            int size);
        

};

#endif