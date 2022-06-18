/**
 * @file MQTTclient.h
 * @author Martin Verges <martin@verges.cc>
 * @brief MQTT client library
 * @version 0.1
 * @date 2022-05-29
**/

#include <Preferences.h>
#include <PubSubClient.h>
#include <Ethernet.h>

extern bool enableMqtt;

class MQTTclient {
    public:
        String mqttTopic;
        String mqttUser;
        String mqttPass;
        String mqttHost;
        String mqttClientId;
        uint16_t mqttPort;

		MQTTclient();
        virtual ~MQTTclient();

        bool isConnected();
        bool isReady();
        void prepare(String host, uint16_t port, String topic, String user, String pass);
        void registerEvents();
        void connect();
        void disconnect();

        PubSubClient client;
    private:
        EthernetClient ethClient;
};

//void onMqttDisconnect(AsyncMqttClientDisconnectReason reason);
//void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);
